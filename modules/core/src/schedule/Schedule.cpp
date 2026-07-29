/**
 * @file Schedule.cpp
 * @author sumin.park
 * @brief Anchor stages, per-system run loop, and the FixedMain accumulator sub-loop.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include <engine/core/schedule/Schedule.hpp>

#include <engine/core/log/Log.hpp>

#include <algorithm>
#include <functional>
#include <string>

namespace engine
{
    Schedule::Schedule()
    {
        // anchors are seed edges in the constraint graph, not a hardcoded array (section 14, A4):
        // Input -> FixedMain -> Update -> PostUpdate -> Last, all in the Main domain
        const std::size_t input = AppendStage(TypeIdOf<stages::Input>(), false, StageDomain::Main);
        const std::size_t fixedMain = AppendStage(TypeIdOf<stages::FixedMain>(), true, StageDomain::Main);
        const std::size_t update = AppendStage(TypeIdOf<stages::Update>(), false, StageDomain::Main);
        const std::size_t postUpdate = AppendStage(TypeIdOf<stages::PostUpdate>(), false, StageDomain::Main);
        const std::size_t last = AppendStage(TypeIdOf<stages::Last>(), false, StageDomain::Main);

        AddEdge(input, fixedMain);
        AddEdge(fixedMain, update);
        AddEdge(update, postUpdate);
        AddEdge(postUpdate, last);

        AppendStage(TypeIdOf<stages::FixedUpdate>(), false, StageDomain::Fixed); // sole fixed node, no edges
    }

    std::size_t Schedule::FindStageIndex(uint32_t seq) const
    {
        for (std::size_t i = 0; i < m_stages.size(); ++i)
        {
            if (m_stages[i].m_seq == seq)
                return i;
        }
        return NPOS;
    }

    std::size_t Schedule::AppendStage(TypeId id, bool isDriver, StageDomain domain)
    {
        StageRecord record;
        record.m_seq = id.m_seq;
        record.m_name = id.m_name;
        record.m_isFixedMainDriver = isDriver;
        record.m_domain = domain;
        m_stages.push_back(std::move(record));
        return m_stages.size() - 1;
    }

    void Schedule::AddEdge(std::size_t fromIdx, std::size_t toIdx)
    {
        // dedup: Kahn counts in-degree, so a duplicated edge would make its target permanently
        // unreachable and manufacture a phantom cycle
        for (const StageEdge &edge : m_edges)
        {
            if (edge.m_from == fromIdx && edge.m_to == toIdx)
                return;
        }
        m_edges.push_back(StageEdge{fromIdx, toIdx});
    }

    void Schedule::InsertStage(TypeId id, uint32_t anchorSeq, bool after)
    {
        ENGINE_ASSERT(!m_started, "Schedule::InsertStage: cannot insert a stage after Start()");
        ENGINE_ASSERT(FindStageIndex(id.m_seq) == NPOS, "Schedule::InsertStage: stage already registered");

        const std::size_t anchorIdx = FindStageIndex(anchorSeq);
        ENGINE_ASSERT(anchorIdx != NPOS, "Schedule::InsertStage: anchor stage not registered");

        const std::size_t newIdx = AppendStage(id, false, m_stages[anchorIdx].m_domain);

        // splice, not a bare extra edge: redirect the anchor's existing edges on the insertion
        // side through the new stage first, so it lands immediately adjacent to the anchor with
        // the anchor's prior neighbors now hanging off the new stage instead. Without this,
        // "after Update" would only mean "sometime after Update", tying with Update's other
        // existing successors (e.g. PostUpdate) instead of landing directly before them
        if (after)
        {
            for (StageEdge &edge : m_edges)
            {
                if (edge.m_from == anchorIdx)
                    edge.m_from = newIdx;
            }
            AddEdge(anchorIdx, newIdx);
        }
        else
        {
            for (StageEdge &edge : m_edges)
            {
                if (edge.m_to == anchorIdx)
                    edge.m_to = newIdx;
            }
            AddEdge(newIdx, anchorIdx);
        }
    }

    void Schedule::ConstrainStage(TypeId id, uint32_t otherSeq, bool after)
    {
        ENGINE_ASSERT(!m_started, "Schedule::ConstrainStage: cannot add a constraint after Start()");

        const std::size_t stageIdx = FindStageIndex(id.m_seq);
        ENGINE_ASSERT(stageIdx != NPOS, "Schedule::ConstrainStage: stage not registered");
        const std::size_t otherIdx = FindStageIndex(otherSeq);
        ENGINE_ASSERT(otherIdx != NPOS, "Schedule::ConstrainStage: other stage not registered");
        ENGINE_ASSERT(m_stages[stageIdx].m_domain == m_stages[otherIdx].m_domain,
                      "Schedule::ConstrainStage: stages are in different domains (Main vs Fixed)");

        if (after)
            AddEdge(otherIdx, stageIdx); // stageIdx runs after otherIdx
        else
            AddEdge(stageIdx, otherIdx); // stageIdx runs before otherIdx
    }

    void Schedule::AddSystemToStage(uint32_t seq, std::unique_ptr<ISystem> system)
    {
        ENGINE_ASSERT(!m_started, "Schedule::AddSystem: cannot register a system after Start()");

        const std::size_t idx = FindStageIndex(seq);
        ENGINE_ASSERT(idx != NPOS, "Schedule::AddSystem: stage not registered");
        ENGINE_ASSERT(!m_stages[idx].m_isFixedMainDriver,
                      "FixedMain is a driver and holds no systems; register into FixedUpdate");

        m_stages[idx].m_systems.push_back(std::move(system));
    }

    std::size_t Schedule::SystemCount() const
    {
        std::size_t count = 0;
        for (const StageRecord &stage : m_stages)
            count += stage.m_systems.size();
        return count;
    }

    // Kahn's algorithm, ready-scan in ascending insertion index: m_stages is append-only and
    // never shrinks, so index is already a unique, stable registration key (same reasoning as
    // Query::m_scannedArchetypeCount, docs/ECS_TDD.md section 10). Ascending-index scan IS the
    // deterministic tiebreak; index is already unique so name comparison is never reached.
    // On a cycle: outOrder still gets every domain node (unreached ones appended by ascending
    // index as a fallback) so Release keeps running every stage exactly once, wrong order only.
    bool Schedule::TopoSort(StageDomain domain, std::vector<std::size_t> &outOrder, std::string *outCycle) const
    {
        outOrder.clear();

        std::vector<std::size_t> nodes;
        for (std::size_t i = 0; i < m_stages.size(); ++i)
        {
            if (m_stages[i].m_domain == domain)
                nodes.push_back(i);
        }

        std::vector<int> inDegree(m_stages.size(), 0);
        for (const StageEdge &edge : m_edges)
        {
            if (m_stages[edge.m_from].m_domain == domain && m_stages[edge.m_to].m_domain == domain)
                ++inDegree[edge.m_to];
        }

        std::vector<bool> emitted(m_stages.size(), false);
        while (outOrder.size() < nodes.size())
        {
            std::size_t pick = NPOS;
            for (std::size_t idx : nodes)
            {
                if (!emitted[idx] && inDegree[idx] == 0)
                {
                    pick = idx;
                    break;
                }
            }
            if (pick == NPOS)
                break; // cycle: nothing left is ready

            emitted[pick] = true;
            outOrder.push_back(pick);
            for (const StageEdge &edge : m_edges)
            {
                if (edge.m_from == pick && m_stages[edge.m_to].m_domain == domain)
                    --inDegree[edge.m_to];
            }
        }

        if (outOrder.size() == nodes.size())
            return true;

        // cycle path diagnostic: DFS the unemitted subgraph, first back edge names the cycle
        if (outCycle != nullptr)
        {
            std::vector<int> state(m_stages.size(), 0); // 0 unvisited, 1 on-stack, 2 done
            std::vector<std::size_t> path;
            std::size_t cycleStart = NPOS;

            std::function<bool(std::size_t)> dfs = [&](std::size_t node) -> bool
            {
                state[node] = 1;
                path.push_back(node);
                for (const StageEdge &edge : m_edges)
                {
                    if (edge.m_from != node || emitted[edge.m_to] || m_stages[edge.m_to].m_domain != domain)
                        continue;
                    if (state[edge.m_to] == 1)
                    {
                        cycleStart = edge.m_to;
                        return true;
                    }
                    if (state[edge.m_to] == 0 && dfs(edge.m_to))
                        return true;
                }
                state[node] = 2;
                path.pop_back();
                return false;
            };

            for (std::size_t start : nodes)
            {
                if (emitted[start] || state[start] != 0)
                    continue;
                if (dfs(start))
                    break;
            }

            if (cycleStart != NPOS)
            {
                auto it = std::find(path.begin(), path.end(), cycleStart);
                std::string text = "stage cycle: ";
                for (auto p = it; p != path.end(); ++p)
                {
                    text += std::string(m_stages[*p].m_name);
                    text += " -> ";
                }
                text += std::string(m_stages[cycleStart].m_name);
                *outCycle = text;
            }
        }

        // fallback order: append the unreached nodes by ascending index, still a complete
        // permutation of the domain
        for (std::size_t idx : nodes)
        {
            if (!emitted[idx])
            {
                outOrder.push_back(idx);
                emitted[idx] = true;
            }
        }

        return false;
    }

    void Schedule::BuildStageOrder()
    {
        std::string mainCycle;
        std::string fixedCycle;
        const bool mainOk = TopoSort(StageDomain::Main, m_mainOrder, &mainCycle);
        const bool fixedOk = TopoSort(StageDomain::Fixed, m_fixedOrder, &fixedCycle);

        // logged outside the assert: ENGINE_ASSERT compiles to nothing in Release, so the log
        // is the only diagnostic an operator gets there (section 14: "the diagnostic is the feature")
        if (!mainOk)
            ENGINE_LOG_ERROR("{}", mainCycle);
        if (!fixedOk)
            ENGINE_LOG_ERROR("{}", fixedCycle);

        ENGINE_ASSERT(mainOk, "Schedule::BuildStageOrder: cycle in the Main stage graph");
        ENGINE_ASSERT(fixedOk, "Schedule::BuildStageOrder: cycle in the Fixed stage graph");
    }

    bool Schedule::ValidateStageOrder(std::string *outCycle) const
    {
        std::vector<std::size_t> order;
        std::string cycle;
        if (!TopoSort(StageDomain::Main, order, &cycle))
        {
            if (outCycle != nullptr)
                *outCycle = cycle;
            return false;
        }
        if (!TopoSort(StageDomain::Fixed, order, &cycle))
        {
            if (outCycle != nullptr)
                *outCycle = cycle;
            return false;
        }
        return true;
    }

    void Schedule::RunStage(World &world, StageRecord &stage)
    {
        for (std::unique_ptr<ISystem> &system : stage.m_systems)
        {
            world.AdvanceTick(); // BEFORE Update
            const uint32_t thisRun = world.CurrentTick();
            system->Update(world);
            world.FlushCommands(); // per system, not per stage (section 14)
            system->m_lastRunTick = thisRun; // AFTER Update; writing it before would destroy
                                              // the value Changed<T> compares against
        }
    }

    void Schedule::RunFrame(World &world, float dt)
    {
        ENGINE_ASSERT(m_started, "Schedule::RunFrame: call Start() first");

        Time &time = world.GetContext<Time>();
        time.m_delta = dt;
        time.m_elapsed += static_cast<double>(dt);
        ++time.m_frameCount;

        for (std::size_t index : m_mainOrder)
        {
            if (m_stages[index].m_isFixedMainDriver)
                RunFixedMain(world);
            else
                RunStage(world, m_stages[index]);
        }
    }

    void Schedule::RunFixedMain(World &world)
    {
        m_accumulator += world.GetContext<Time>().m_delta; // read before the guard goes up

        // hard bound on backlog; this is what stops the spiral, NOT the per-frame step cap.
        // clamp, never zero: a one-off hitch drains over the next frames instead of teleporting
        // the sim. bound > per-frame budget, or the loop drains it every frame and clamp == drop
        if (m_accumulator > MAX_ACCUMULATED_SECONDS)
            m_accumulator = MAX_ACCUMULATED_SECONDS;

        FixedTime &fixed = world.GetContext<FixedTime>();

        uint32_t steps = 0;
        world.SetInFixedMain(true);
        while (m_accumulator >= FIXED_STEP_SECONDS && steps < MAX_FIXED_STEPS_PER_FRAME)
        {
            fixed.m_delta = FIXED_STEP_SECONDS;
            fixed.m_elapsed += static_cast<double>(FIXED_STEP_SECONDS);
            fixed.m_stepIndex = steps;

            for (std::size_t index : m_fixedOrder) // contained stages run as one atomic block
                RunStage(world, m_stages[index]);

            m_accumulator -= FIXED_STEP_SECONDS;
            ++steps;
        }
        world.SetInFixedMain(false);
    }

    void Schedule::Start(World &world)
    {
        ENGINE_ASSERT(!m_started, "Schedule::Start: already started");

        BuildStageOrder(); // sort before the OnStart walks below, which read m_mainOrder/m_fixedOrder

        world.InitContext(Time{});       // Init, not Set: a game may have installed its own
        world.InitContext(FixedTime{});
        world.SetFixedForbiddenContext(TypeIdOf<Time>().m_seq);

        for (std::size_t index : m_mainOrder)
        {
            for (std::unique_ptr<ISystem> &system : m_stages[index].m_systems)
            {
                system->OnStart(world);
                world.FlushCommands();
            }
        }
        for (std::size_t index : m_fixedOrder)
        {
            for (std::unique_ptr<ISystem> &system : m_stages[index].m_systems)
            {
                system->OnStart(world);
                world.FlushCommands();
            }
        }

        world.FreezeComponents(); // this call is the init phase sections 8 and 13 were waiting for
        world.FreezeContexts();
        m_started = true;
    }

    void Schedule::Stop(World &world)
    {
        ENGINE_ASSERT(m_started, "Schedule::Stop: not started");

        for (auto it = m_fixedOrder.rbegin(); it != m_fixedOrder.rend(); ++it)
        {
            auto &systems = m_stages[*it].m_systems;
            for (auto sysIt = systems.rbegin(); sysIt != systems.rend(); ++sysIt)
                (*sysIt)->OnStop(world);
        }
        for (auto it = m_mainOrder.rbegin(); it != m_mainOrder.rend(); ++it)
        {
            auto &systems = m_stages[*it].m_systems;
            for (auto sysIt = systems.rbegin(); sysIt != systems.rend(); ++sysIt)
                (*sysIt)->OnStop(world);
        }

        m_started = false;
    }
}
