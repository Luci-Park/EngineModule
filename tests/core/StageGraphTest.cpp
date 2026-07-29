#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/World.hpp>
#include <engine/core/schedule/ISystem.hpp>
#include <engine/core/schedule/Schedule.hpp>
#include <engine/core/schedule/Stages.hpp>

#include <string>
#include <vector>

using namespace engine;

namespace
{
    struct RecordingSystem : ISystem
    {
        explicit RecordingSystem(std::vector<int> &log, int id) : m_log(log), m_id(id) {}
        void Update(World &) override { m_log.push_back(m_id); }
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

TEST_CASE("Anchor baseline order is unchanged from unit 11", "[core][schedule][stagegraph]")
{
    Schedule schedule;
    World world;
    std::vector<int> log;

    schedule.AddSystem<stages::Last>(std::make_unique<RecordingSystem>(log, 5));
    schedule.AddSystem<stages::Input>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<stages::PostUpdate>(std::make_unique<RecordingSystem>(log, 4));
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 3));

    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 3, 4, 5});
}

TEST_CASE("AddStageAfter against Update lands between Update and PostUpdate", "[core][schedule][stagegraph]")
{
    struct CustomStage
    {
    };

    Schedule schedule;
    std::vector<int> log;

    schedule.AddStageAfter<CustomStage, stages::Update>();
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<CustomStage>(std::make_unique<RecordingSystem>(log, 2));
    schedule.AddSystem<stages::PostUpdate>(std::make_unique<RecordingSystem>(log, 3));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 2, 3});
}

TEST_CASE("Two AddStageAfter calls against the same anchor splice in LIFO order: the more recent "
          "insertion lands closest to the anchor, pushing the earlier one further away",
          "[core][schedule][stagegraph]")
{
    // AddStageAfter/Before is a splice (see InsertStage), not a bare graph edge: it redirects
    // the anchor's existing successors through the new stage, so two insertions against the
    // same anchor are never actually tied; the graph stays a single total order throughout,
    // and the second call always lands adjacent to the anchor, ahead of the first
    struct LifoStageA
    {
    };
    struct LifoStageB
    {
    };

    Schedule schedule;
    std::vector<int> log;

    schedule.AddStageAfter<LifoStageA, stages::Input>();
    schedule.AddStageAfter<LifoStageB, stages::Input>();

    schedule.AddSystem<stages::Input>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<LifoStageA>(std::make_unique<RecordingSystem>(log, 2));
    schedule.AddSystem<LifoStageB>(std::make_unique<RecordingSystem>(log, 3));
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 4));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 3, 2, 4});
}

TEST_CASE("Stage order is deterministic across identical registration sequences", "[core][schedule][stagegraph]")
{
    struct DeterministicStageA
    {
    };
    struct DeterministicStageB
    {
    };

    auto build = [](std::vector<int> &log)
    {
        auto schedule = std::make_unique<Schedule>();
        schedule->AddStageAfter<DeterministicStageA, stages::Input>();
        schedule->AddStageAfter<DeterministicStageB, stages::Input>();
        schedule->ConstrainStageBefore<DeterministicStageA, stages::Update>();
        schedule->ConstrainStageBefore<DeterministicStageB, stages::Update>();
        schedule->AddSystem<DeterministicStageA>(std::make_unique<RecordingSystem>(log, 1));
        schedule->AddSystem<DeterministicStageB>(std::make_unique<RecordingSystem>(log, 2));
        return schedule;
    };

    std::vector<int> logA;
    std::vector<int> logB;
    auto scheduleA = build(logA);
    auto scheduleB = build(logB);

    World worldA;
    World worldB;
    scheduleA->Start(worldA);
    scheduleA->RunFrame(worldA, 0.016f);
    scheduleB->Start(worldB);
    scheduleB->RunFrame(worldB, 0.016f);

    REQUIRE(logA == logB);
}

TEST_CASE("ConstrainStageBefore adds a second edge to an already-registered stage",
          "[core][schedule][stagegraph]")
{
    struct SecondEdgeStage
    {
    };

    Schedule schedule;
    std::vector<int> log;

    schedule.AddStageAfter<SecondEdgeStage, stages::Input>(); // first edge: after Input
    schedule.ConstrainStageBefore<SecondEdgeStage, stages::Update>(); // second edge: before Update

    schedule.AddSystem<stages::Input>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<SecondEdgeStage>(std::make_unique<RecordingSystem>(log, 2));
    schedule.AddSystem<stages::Update>(std::make_unique<RecordingSystem>(log, 3));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 2, 3});
}

TEST_CASE("A cycle is reported by ValidateStageOrder without crashing the process", "[core][schedule][stagegraph]")
{
    struct CycleStageA
    {
    };
    struct CycleStageB
    {
    };

    Schedule schedule;
    schedule.AddStageAfter<CycleStageA, stages::Input>();
    schedule.AddStageAfter<CycleStageB, stages::Input>();
    schedule.ConstrainStageBefore<CycleStageA, CycleStageB>(); // A before B
    schedule.ConstrainStageBefore<CycleStageB, CycleStageA>(); // B before A: cycle

    std::string cycle;
    const bool ok = schedule.ValidateStageOrder(&cycle);

    REQUIRE_FALSE(ok);
    REQUIRE(cycle.find("stage cycle:") != std::string::npos);
}

TEST_CASE("A stage inserted against FixedUpdate sorts in the fixed domain", "[core][schedule][stagegraph]")
{
    struct StageGraphFixedStage
    {
    };

    Schedule schedule;
    schedule.AddStageAfter<StageGraphFixedStage, stages::FixedUpdate>();

    std::vector<uint32_t> steps;
    schedule.AddSystem<StageGraphFixedStage>(std::make_unique<FixedStepCountSystem>(steps));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 2.0f / 64.0f);

    REQUIRE(steps == std::vector<uint32_t>{0, 1});
}

TEST_CASE("A duplicate edge still sorts (dedup regression)", "[core][schedule][stagegraph]")
{
    struct DupEdgeStage
    {
    };

    Schedule schedule;
    std::vector<int> log;

    schedule.AddStageAfter<DupEdgeStage, stages::Input>();
    schedule.ConstrainStageAfter<DupEdgeStage, stages::Input>(); // duplicate of the AddStageAfter edge

    schedule.AddSystem<stages::Input>(std::make_unique<RecordingSystem>(log, 1));
    schedule.AddSystem<DupEdgeStage>(std::make_unique<RecordingSystem>(log, 2));

    World world;
    schedule.Start(world);
    schedule.RunFrame(world, 0.016f);

    REQUIRE(log == std::vector<int>{1, 2});
}
