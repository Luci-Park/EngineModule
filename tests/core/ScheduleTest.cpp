#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/Query.hpp>
#include <engine/core/ecs/QueryFilters.hpp>
#include <engine/core/ecs/World.hpp>
#include <engine/core/schedule/ISystem.hpp>
#include <engine/core/schedule/Schedule.hpp>
#include <engine/core/schedule/Stages.hpp>
#include <engine/core/schedule/Time.hpp>

#include <vector>

using namespace engine;

namespace
{
    struct Position
    {
        float m_x = 0.0f;
    };

    struct RecordingSystem : ISystem
    {
        explicit RecordingSystem(std::vector<int> &log, int id) : m_log(log), m_id(id) {}
        void Update(World &) override { m_log.push_back(m_id); }
        std::vector<int> &m_log;
        int m_id;
    };

    struct SpawnSystem : ISystem
    {
        void Update(World &world) override { world.Spawn(Position{1.0f}); }
    };

    struct CountingSystem : ISystem
    {
        explicit CountingSystem(int &counter) : m_counter(counter) {}
        void Update(World &) override { ++m_counter; }
        int &m_counter;
    };

    struct StartStopSystem : ISystem
    {
        explicit StartStopSystem(std::vector<int> &log, int id) : m_log(log), m_id(id) {}
        void OnStart(World &) override { m_log.push_back(m_id); }
        void OnStop(World &) override { m_log.push_back(-m_id); }
        void Update(World &) override {}
        std::vector<int> &m_log;
        int m_id;
    };

    struct FixedStepCountSystem : ISystem
    {
        explicit FixedStepCountSystem(std::vector<uint32_t> &steps) : m_steps(steps) {}
        void Update(World &world) override { m_steps.push_back(world.GetContext<FixedTime>().m_stepIndex); }
        std::vector<uint32_t> &m_steps;
    };
}

TEST_CASE("Schedule seeds six anchor stages", "[core][schedule]")
{
    Schedule schedule;
    REQUIRE(schedule.StageCount() == 6);
}

TEST_CASE("Systems run in registration order within a stage", "[core][schedule]")
{
    Schedule schedule;
    World world;
    std::vector<int> log;

    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 2));
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 2});
}

TEST_CASE("Stages run in main order across a frame", "[core][schedule]")
{
    Schedule schedule;
    World world;
    std::vector<int> log;

    schedule.AddSystem<stages::Last>(std::make_unique<RecordingSystem>(log, 3));
    schedule.AddSystem<stages::Input>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 2));
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 2, 3});
}

TEST_CASE("Tick advances once per system, not per frame or stage", "[core][schedule]")
{
    Schedule schedule;
    World world;

    world.RegisterComponent<Position>(); // touch the type before freeze; see decision F
    schedule.AddSystem<stages::Input>(std::make_unique<SpawnSystem>());
    schedule.AddSystem<stages::Update>(std::make_unique<SpawnSystem>());
    schedule.Start(world);

    const uint32_t before = world.CurrentTick();
    schedule.RunFrame(world, 0.016f);
    REQUIRE(world.CurrentTick() == before + 2);
}

TEST_CASE("Commands recorded by one system are visible to the next system in the same stage", "[core][schedule]")
{
    struct RecorderSystem : ISystem
    {
        void Update(World &world) override { world.Commands().Spawn(Position{1.0f}); }
    };
    struct CounterSystem : ISystem
    {
        explicit CounterSystem(std::size_t &count) : m_count(count) {}
        void Update(World &world) override
        {
            Query<Entity, const Position> q(world);
            m_count = q.Count();
        }
        std::size_t &m_count;
    };

    Schedule schedule;
    World world;
    std::size_t count = 0;

    world.RegisterComponent<Position>(); // touch the type before freeze; see decision F
    schedule.AddSystem<stages::Update>(std::make_unique<RecorderSystem>());
    schedule.AddSystem<stages::Update>(std::make_unique<CounterSystem>(count));
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(count == 1);
}

TEST_CASE("AddStageAfter inserts into the main order", "[core][schedule]")
{
    struct CustomMainStage
    {
    };

    Schedule schedule;
    std::vector<int> log;

    schedule.AddStageAfter<CustomMainStage, stages::Update>();
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<CustomMainStage>(std::make_unique<RecordingSystem>(log, 2));
    schedule.AddSystem<stages::PostUpdate>(std::make_unique<RecordingSystem>(log, 3));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 2, 3});
    REQUIRE(schedule.StageCount() == 7);
}

TEST_CASE("AddStageAfter against FixedUpdate lands in the fixed order", "[core][schedule]")
{
    struct CustomFixedStage
    {
    };

    Schedule schedule;
    schedule.AddStageAfter<CustomFixedStage, stages::FixedUpdate>();

    std::vector<uint32_t> steps;
    schedule.AddSystem<CustomFixedStage>(std::make_unique<FixedStepCountSystem>(steps));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 2.0f / 64.0f);

    REQUIRE(steps == std::vector<uint32_t>{0, 1});
}

TEST_CASE("OnStart runs before the first Update, in registration order", "[core][schedule]")
{
    Schedule schedule;
    World world;
    std::vector<int> log;

    schedule.AddSystem<stages::Update>(std::make_unique<StartStopSystem>(log, 1));
    schedule.AddSystem<stages::Update>(std::make_unique<StartStopSystem>(log, 2));
    schedule.Start(world);

    REQUIRE(log == std::vector<int>{1, 2});
}

TEST_CASE("OnStop runs in reverse order", "[core][schedule]")
{
    Schedule schedule;
    World world;
    std::vector<int> log;

    schedule.AddSystem<stages::Update>(std::make_unique<StartStopSystem>(log, 1));
    schedule.AddSystem<stages::Update>(std::make_unique<StartStopSystem>(log, 2));
    schedule.Start(world);
    log.clear();
    schedule.Stop(world);

    REQUIRE(log == std::vector<int>{-2, -1});
}

TEST_CASE("Start freezes components and contexts", "[core][schedule]")
{
    Schedule schedule;
    World world;
    schedule.Start(world);

    REQUIRE(world.ComponentsFrozen());
    REQUIRE(world.ContextsFrozen());
}

TEST_CASE("Time and FixedTime survive across frames without reset", "[core][schedule]")
{
    Schedule schedule;
    World world;
    schedule.Start(world);

    schedule.RunFrame(world, 0.016f);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(world.GetContext<Time>().m_frameCount == 2);
    REQUIRE(world.GetContext<Time>().m_elapsed > 0.0);
}

TEST_CASE("FixedMain runs zero sub-steps below the step size and carries the accumulator", "[core][schedule]")
{
    std::vector<uint32_t> steps;
    Schedule schedule;
    schedule.AddSystem<stages::FixedUpdate>(std::make_unique<FixedStepCountSystem>(steps));

    World world;
    schedule.Start(world);

    const float smallDt = 1.0f / 256.0f;
    for (int i = 0; i < 4; ++i)
        schedule.RunFrame(world, smallDt);

    REQUIRE(steps.size() == 1);
    REQUIRE(steps[0] == 0);
}

TEST_CASE("FixedMain runs exactly two sub-steps for a two-step dt", "[core][schedule]")
{
    std::vector<uint32_t> steps;
    Schedule schedule;
    schedule.AddSystem<stages::FixedUpdate>(std::make_unique<FixedStepCountSystem>(steps));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 2.0f / 64.0f);

    REQUIRE(steps == std::vector<uint32_t>{0, 1});
}

TEST_CASE("FixedTime delta is always the step, never the frame dt", "[core][schedule]")
{
    struct DeltaCaptureSystem : ISystem
    {
        explicit DeltaCaptureSystem(std::vector<float> &deltas) : m_deltas(deltas) {}
        void Update(World &world) override { m_deltas.push_back(world.GetContext<FixedTime>().m_delta); }
        std::vector<float> &m_deltas;
    };

    std::vector<float> deltas;
    Schedule schedule;
    schedule.AddSystem<stages::FixedUpdate>(std::make_unique<DeltaCaptureSystem>(deltas));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 0.5f); // large, arbitrary dt

    for (float d : deltas)
        REQUIRE(d == (1.0f / 64.0f));
}

TEST_CASE("A large dt is clamped and drains as backlog over subsequent frames, not dropped", "[core][schedule]")
{
    std::vector<uint32_t> steps;
    Schedule schedule;
    schedule.AddSystem<stages::FixedUpdate>(std::make_unique<FixedStepCountSystem>(steps));

    World world;
    schedule.Start(world);

    schedule.RunFrame(world, 10.0f); // far more than the 0.25s backlog bound
    REQUIRE(steps.size() == 8);      // MAX_FIXED_STEPS_PER_FRAME

    steps.clear();
    schedule.RunFrame(world, 0.0f); // drains the remaining clamped backlog
    REQUIRE(steps.size() == 8);

    steps.clear();
    schedule.RunFrame(world, 0.0f); // backlog fully drained now
    REQUIRE(steps.empty());
}

TEST_CASE("Sustained overload never runs more than the per-frame step cap", "[core][schedule]")
{
    std::vector<uint32_t> steps;
    Schedule schedule;
    schedule.AddSystem<stages::FixedUpdate>(std::make_unique<FixedStepCountSystem>(steps));

    World world;
    schedule.Start(world);

    for (int frame = 0; frame < 100; ++frame)
    {
        steps.clear();
        schedule.RunFrame(world, 10.0f);
        REQUIRE(steps.size() <= 8);
    }
}

TEST_CASE("Reading Time inside FixedUpdate asserts under debug, and FixedTime does not", "[core][schedule]")
{
    // positive-path only: reading FixedTime and reading Time from Update both succeed
    struct FixedReadsFixedTime : ISystem
    {
        void Update(World &world) override { (void)world.GetContext<FixedTime>(); }
    };
    struct UpdateReadsTime : ISystem
    {
        void Update(World &world) override { (void)world.GetContext<Time>(); }
    };

    Schedule schedule;
    schedule.AddSystem<stages::FixedUpdate>(std::make_unique<FixedReadsFixedTime>());
    schedule.AddSystem<stages::Update>(std::make_unique<UpdateReadsTime>());

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 1.0f / 64.0f);
}

TEST_CASE("Query::SetLastRunTick re-points a member query's Changed<T> filter", "[core][schedule]")
{
    World world;
    Entity a = world.Spawn(Position{1.0f});

    Query<Entity, Changed<Position>> q(world, 0);
    REQUIRE(q.Count() == 1); // lastRunTick 0 matches everything

    const uint32_t beforeMutation = world.CurrentTick(); // tick the Spawn stamped

    world.AdvanceTick();
    auto mut = world.GetComponentMut<Position>(a);
    (**mut).m_x = 2.0f; // stamps changedTick at the new CurrentTick()

    q.SetLastRunTick(beforeMutation);
    REQUIRE(q.Count() == 1); // mutation tick > beforeMutation

    q.SetLastRunTick(world.CurrentTick());
    REQUIRE(q.Count() == 0); // mutation tick == lastRunTick, not >
}

TEST_CASE("Query::SetLastRunTick picks up an archetype created after construction", "[core][schedule]")
{
    struct ScheduleTestVelocity
    {
        float m_v = 0.0f;
    };

    World world;
    world.Spawn(Position{1.0f});

    Query<Entity, const Position> q(world);
    REQUIRE(q.Count() == 1);

    world.Spawn(Position{2.0f}, ScheduleTestVelocity{3.0f}); // new archetype
    q.SetLastRunTick(0);
    REQUIRE(q.Count() == 2);
}

TEST_CASE("Despawn-in-loop via Commands applies safely and visits every entity", "[core][schedule]")
{
    World world;
    std::vector<Entity> spawned;
    for (int i = 0; i < 5; ++i)
        spawned.push_back(world.Spawn(Position{static_cast<float>(i)}));

    Query<Entity, const Position> q(world);
    std::size_t visited = 0;
    for (auto [e, pos] : q)
    {
        (void)pos;
        world.Commands().Despawn(e);
        ++visited;
    }
    world.FlushCommands();

    REQUIRE(visited == 5);
    REQUIRE(world.EntityCount() == 0);
}

TEST_CASE("An iterator parked at end does not block a spawn while still in scope", "[core][schedule]")
{
    World world;
    world.Spawn(Position{1.0f});

    Query<Entity, const Position> q(world);
    auto it = q.begin();
    auto stop = q.end();
    ++it;
    REQUIRE_FALSE(it != stop);

    world.Spawn(Position{2.0f}); // must not assert: it is parked at end, not counted
    REQUIRE(world.EntityCount() == 2);
}

TEST_CASE("IsIterating reflects only a live row-positioned iterator", "[core][schedule]")
{
    World world;
    world.Spawn(Position{1.0f});

    REQUIRE_FALSE(world.IsIterating());
    {
        Query<Entity, const Position> q(world);
        for (auto [e, pos] : q)
        {
            (void)e;
            (void)pos;
            REQUIRE(world.IsIterating());
        }
    }
    REQUIRE_FALSE(world.IsIterating());
}
