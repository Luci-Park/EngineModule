#include <catch2/catch_test_macros.hpp>

#include <engine/core/ecs/Context.hpp>
#include <engine/core/ecs/ContextRegistry.hpp>
#include <engine/core/ecs/Entity.hpp>
#include <engine/core/ecs/World.hpp>

#include <memory>
#include <vector>

using namespace engine;

namespace
{
    struct Config
    {
        int m_value = 0;
    };

    struct Other
    {
        float m_value = 0.0f;
    };

    // owning, non-trivially-copyable: proves contexts are POD-exempt (section 3 never applies
    // here, since contexts never migrate and are never memcpy'd)
    struct OwningContext
    {
        std::unique_ptr<int> m_ptr;
        std::vector<int> m_items;
    };

    int g_destroyCount = 0;

    // m_alive tracks whether THIS object still owns its "real" identity, so a moved-through
    // temporary (constructor param -> ContextHolder::m_value) does not inflate the count;
    // only the genuinely-stored value's eventual destruction increments g_destroyCount
    struct Counted
    {
        int m_value = 0;
        bool m_alive = true;
        Counted() = default;
        explicit Counted(int v) : m_value(v) {}
        Counted(const Counted &) = delete;
        Counted &operator=(const Counted &) = delete;
        Counted(Counted &&other) noexcept : m_value(other.m_value), m_alive(other.m_alive) { other.m_alive = false; }
        Counted &operator=(Counted &&other) noexcept
        {
            m_value = other.m_value;
            m_alive = other.m_alive;
            other.m_alive = false;
            return *this;
        }
        ~Counted()
        {
            if (m_alive)
                ++g_destroyCount;
        }
    };

    std::vector<int> g_teardownLog;

    // distinct non-type template args -> distinct types, so A/B/C can be installed as three
    // independent contexts while sharing one moved-from-aware logging implementation
    template <int Id>
    struct LoggedContext
    {
        bool m_alive = true;
        LoggedContext() = default;
        LoggedContext(const LoggedContext &) = delete;
        LoggedContext &operator=(const LoggedContext &) = delete;
        LoggedContext(LoggedContext &&other) noexcept : m_alive(other.m_alive) { other.m_alive = false; }
        LoggedContext &operator=(LoggedContext &&other) noexcept
        {
            m_alive = other.m_alive;
            other.m_alive = false;
            return *this;
        }
        ~LoggedContext()
        {
            if (m_alive)
                g_teardownLog.push_back(Id);
        }
    };

    using ContextA = LoggedContext<1>;
    using ContextB = LoggedContext<2>;
    using ContextC = LoggedContext<3>;

    // C5: proves ~World tears down contexts AFTER the component hook sweep, not before
    bool g_contextDestroyed = false;

    // m_alive guard: without it, the by-value Set() parameter pass would copy this (no move
    // ctor exists once a destructor is user-declared), and the discarded temporary's destructor
    // would flip the flag right after Set() returns, long before ~World() runs
    struct DestroyFlagContext
    {
        bool m_alive = true;
        DestroyFlagContext() = default;
        DestroyFlagContext(const DestroyFlagContext &) = delete;
        DestroyFlagContext &operator=(const DestroyFlagContext &) = delete;
        DestroyFlagContext(DestroyFlagContext &&other) noexcept : m_alive(other.m_alive) { other.m_alive = false; }
        DestroyFlagContext &operator=(DestroyFlagContext &&other) noexcept
        {
            m_alive = other.m_alive;
            other.m_alive = false;
            return *this;
        }
        ~DestroyFlagContext()
        {
            if (m_alive)
                g_contextDestroyed = true;
        }
    };

    struct HookRecord
    {
        bool m_hookRan = false;
        bool m_sawNonNullContext = false;
        bool m_observedDestroyedFlag = true; // start true; the hook must prove it false
    };

    HookRecord g_hookRecord;

    struct ReleasesIntoContext
    {
        int m_value = 0;
    };

    void OnReleasesIntoContextRemove(World &world, Entity, void *) noexcept
    {
        g_hookRecord.m_hookRan = true;
        DestroyFlagContext *ctx = world.TryGetContext<DestroyFlagContext>();
        g_hookRecord.m_sawNonNullContext = ctx != nullptr;
        g_hookRecord.m_observedDestroyedFlag = g_contextDestroyed;
    }
}

TEST_CASE("ContextRegistry Set installs and Get retrieves by type", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(Config{42});

    REQUIRE(registry.Get<Config>().m_value == 42);
    registry.Validate();
}

TEST_CASE("ContextRegistry Has and TryGet are false/null before install, true/non-null after", "[core][ecs][context]")
{
    ContextRegistry registry;
    REQUIRE_FALSE(registry.Has<Config>());
    REQUIRE(registry.TryGet<Config>() == nullptr);

    registry.Set(Config{7});

    REQUIRE(registry.Has<Config>());
    REQUIRE(registry.TryGet<Config>() != nullptr);
    REQUIRE(registry.TryGet<Config>()->m_value == 7);
    registry.Validate();
}

TEST_CASE("ContextRegistry Set returns a reference that aliases the stored value", "[core][ecs][context]")
{
    ContextRegistry registry;
    Config *ref = registry.Set(Config{1});
    ref->m_value = 99;

    REQUIRE(registry.Get<Config>().m_value == 99);
    registry.Validate();
}

TEST_CASE("ContextRegistry Size tracks installs across distinct types", "[core][ecs][context]")
{
    ContextRegistry registry;
    REQUIRE(registry.Size() == 0);

    registry.Set(Config{1});
    REQUIRE(registry.Size() == 1);

    registry.Set(Other{2.0f});
    REQUIRE(registry.Size() == 2);
    registry.Validate();
}

TEST_CASE("ContextRegistry installs and retrieves an owning, non-trivially-copyable context", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(OwningContext{std::make_unique<int>(5), {1, 2, 3}});

    OwningContext &stored = registry.Get<OwningContext>();
    REQUIRE(*stored.m_ptr == 5);
    REQUIRE(stored.m_items.size() == 3);
    REQUIRE(stored.m_items[2] == 3);
    registry.Validate();
}

TEST_CASE("ContextRegistry const Get/TryGet see the same value as the mutable accessors", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(Config{3});

    const ContextRegistry &constRegistry = registry;
    REQUIRE(constRegistry.Get<Config>().m_value == 3);
    REQUIRE(constRegistry.TryGet<Config>() != nullptr);
    REQUIRE(constRegistry.TryGet<Config>()->m_value == 3);
    REQUIRE(constRegistry.TryGet<Other>() == nullptr);
}

TEST_CASE("ContextRegistry Init on an absent type installs normally", "[core][ecs][context]")
{
    ContextRegistry registry;
    Config *ref = registry.Init(Config{4});

    REQUIRE(registry.Has<Config>());
    REQUIRE(ref->m_value == 4);
    REQUIRE(registry.Get<Config>().m_value == 4);
    registry.Validate();
}

TEST_CASE("ContextRegistry Init on a present type is a no-op returning the existing value", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(Config{1});

    Config *ref = registry.Init(Config{999}); // discarded; existing value must survive

    REQUIRE(ref->m_value == 1);
    REQUIRE(registry.Get<Config>().m_value == 1);
    REQUIRE(registry.Size() == 1);
    registry.Validate();
}

TEST_CASE("ContextRegistry Override on an absent type installs normally", "[core][ecs][context]")
{
    ContextRegistry registry;
    Config *ref = registry.Override(Config{6});

    REQUIRE(registry.Has<Config>());
    REQUIRE(ref->m_value == 6);
    registry.Validate();
}

TEST_CASE("ContextRegistry Override on a present type replaces the value and runs the old dtor exactly once", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(Counted{1});
    g_destroyCount = 0; // ignore whatever moves happened to install the first value

    registry.Override(Counted{2});

    REQUIRE(g_destroyCount == 1);
    REQUIRE(registry.Get<Counted>().m_value == 2);
    REQUIRE(registry.Size() == 1); // replace, not a second entry
    registry.Validate();
}

TEST_CASE("ContextRegistry Set/Init/Override all return a reference aliasing what Get sees", "[core][ecs][context]")
{
    {
        ContextRegistry registry;
        Config *ref = registry.Set(Config{10});
        REQUIRE(ref == &registry.Get<Config>());
    }
    {
        ContextRegistry registry;
        Config *ref = registry.Init(Config{20});
        REQUIRE(ref == &registry.Get<Config>());
    }
    {
        ContextRegistry registry;
        Config *ref = registry.Override(Config{30});
        REQUIRE(ref == &registry.Get<Config>());
    }
}

// Set's assert-on-present has no catchable failure path in this harness (ENGINE_ASSERT calls
// ENGINE_DEBUG_BREAK with no throw; see unit 07's plan for the standing no-death-test rationale).
// The surrounding, non-asserting behavior is covered instead: a present type is observable via
// Has/TryGet before attempting a second Set, and Init/Override above cover what a caller should
// use when a clash is expected rather than a bug.
TEST_CASE("ContextRegistry Has proves presence before a caller would choose Set vs Init vs Override", "[core][ecs][context]")
{
    ContextRegistry registry;
    REQUIRE_FALSE(registry.Has<Config>());
    registry.Set(Config{1});
    REQUIRE(registry.Has<Config>()); // a second Set here would be the asserting, undocumented path
    registry.Validate();
}

TEST_CASE("ContextRegistry TearDown destroys contexts in reverse install order", "[core][ecs][context]")
{
    g_teardownLog.clear();
    ContextRegistry registry;
    registry.Set(ContextA{});
    registry.Set(ContextB{});
    registry.Set(ContextC{});

    registry.TearDown();

    REQUIRE(g_teardownLog == std::vector<int>{3, 2, 1});
    REQUIRE(registry.Size() == 0);
}

TEST_CASE("ContextRegistry TearDown on an empty registry is safe", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.TearDown();
    REQUIRE(registry.Size() == 0);
    registry.Validate();
}

TEST_CASE("ContextRegistry TearDown twice is safe: second call is a no-op, not a double-destroy", "[core][ecs][context]")
{
    g_teardownLog.clear();
    ContextRegistry registry;
    registry.Set(ContextA{});

    registry.TearDown();
    REQUIRE(g_teardownLog == std::vector<int>{1});

    registry.TearDown(); // must not touch anything already gone
    REQUIRE(g_teardownLog == std::vector<int>{1});
    registry.Validate();
}

TEST_CASE("ContextRegistry Override keeps install-order position: replacing B still tears down C, B, A", "[core][ecs][context]")
{
    g_teardownLog.clear();
    ContextRegistry registry;
    registry.Set(ContextA{});
    registry.Set(ContextB{});
    registry.Set(ContextC{});

    registry.Override(ContextB{}); // must not move B to the end of install order
    REQUIRE(g_teardownLog == std::vector<int>{2}); // the OLD B destroyed by Override itself

    g_teardownLog.clear();
    registry.TearDown();

    REQUIRE(g_teardownLog == std::vector<int>{3, 2, 1}); // still C, B, A
}

TEST_CASE("ContextRegistry IsFrozen reflects Freeze", "[core][ecs][context]")
{
    ContextRegistry registry;
    REQUIRE_FALSE(registry.IsFrozen());

    registry.Freeze();

    REQUIRE(registry.IsFrozen());
    registry.Validate();
}

// decision C: freeze gates the ABSENT path only. Override on an already-installed type must
// keep working after Freeze; this is the row most likely to be over-restricted.
TEST_CASE("ContextRegistry Override on a present type still replaces after Freeze", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(Counted{1});
    registry.Freeze();
    g_destroyCount = 0;

    registry.Override(Counted{2});

    REQUIRE(g_destroyCount == 1);
    REQUIRE(registry.Get<Counted>().m_value == 2);
    registry.Validate();
}

TEST_CASE("ContextRegistry Init on a present type is still a no-op after Freeze", "[core][ecs][context]")
{
    ContextRegistry registry;
    registry.Set(Config{1});
    registry.Freeze();

    Config *ref = registry.Init(Config{999});

    REQUIRE(ref->m_value == 1);
    REQUIRE(registry.Get<Config>().m_value == 1);
    registry.Validate();
}

// C5, the central deliverable of this unit: a component on_remove hook fired during ~World()
// must still see a live context. If teardown ran contexts first, TryGetContext would return
// null and g_contextDestroyed would already be true by the time the hook ran.
TEST_CASE("World tears down contexts after firing on_remove hooks, not before (C5)", "[core][ecs][context][c5]")
{
    g_contextDestroyed = false;
    g_hookRecord = HookRecord{};

    {
        World world;
        world.SetComponentHooks<ReleasesIntoContext>(nullptr, OnReleasesIntoContextRemove);
        world.SetContext(DestroyFlagContext{});
        world.Spawn(ReleasesIntoContext{1});
        world.Validate();
    } // ~World: hook sweep must run before m_contexts.TearDown()

    REQUIRE(g_hookRecord.m_hookRan);
    REQUIRE(g_hookRecord.m_sawNonNullContext);
    REQUIRE_FALSE(g_hookRecord.m_observedDestroyedFlag);
    REQUIRE(g_contextDestroyed); // the context itself must still eventually be torn down
}
