/**
 * @file Schedule.hpp
 * @author sumin.park
 * @brief Anchor stages, per-system run loop, and the FixedMain accumulator sub-loop.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#pragma once
#include <engine/core/core_export.h>
#include <engine/core/ecs/TypeId.hpp>
#include <engine/core/ecs/World.hpp>
#include <engine/core/log/Assert.hpp>
#include <engine/core/schedule/ISystem.hpp>
#include <engine/core/schedule/Stages.hpp>
#include <engine/core/schedule/Time.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine
{
    class ENGINE_CORE_API Schedule
    {
    public:
        Schedule(); // seeds the anchor stages

        // dllexport forces instantiation of the implicit copy ops -> C2280 unless spelled out.
        // move deleted too: nothing moves a Schedule, and a moved-from one after Start() would
        // be a silent half-built scheduler
        Schedule(const Schedule &) = delete;
        Schedule &operator=(const Schedule &) = delete;
        Schedule(Schedule &&) = delete;
        Schedule &operator=(Schedule &&) = delete;

        template <typename StageTag>
        void AddSystem(std::unique_ptr<ISystem> system)
        {
            AddSystemToStage(TypeIdOf<StageTag>().m_seq, std::move(system));
        }

        // registers a stage plus its first edge into the constraint graph; unit 12 sorts the
        // graph in Start() rather than inserting directly into the order vector
        template <typename Tag, typename AnchorTag>
        void AddStageBefore()
        {
            InsertStage(TypeIdOf<Tag>(), TypeIdOf<AnchorTag>().m_seq, false);
        }

        template <typename Tag, typename AnchorTag>
        void AddStageAfter()
        {
            InsertStage(TypeIdOf<Tag>(), TypeIdOf<AnchorTag>().m_seq, true);
        }

        // extra edges between ALREADY-registered stages; what makes the graph a graph rather
        // than a chain (a stage after X and before Y)
        template <typename Tag, typename OtherTag>
        void ConstrainStageBefore()
        {
            ConstrainStage(TypeIdOf<Tag>(), TypeIdOf<OtherTag>().m_seq, false);
        }

        template <typename Tag, typename OtherTag>
        void ConstrainStageAfter()
        {
            ConstrainStage(TypeIdOf<Tag>(), TypeIdOf<OtherTag>().m_seq, true);
        }

        void Start(World &world);              // sorts the graph, OnStart every system, then freezes both registries
        void RunFrame(World &world, float dt);  // one frame: write Time, walk the main order
        void Stop(World &world);                // OnStop in reverse registration order

        std::size_t StageCount() const { return m_stages.size(); }
        std::size_t SystemCount() const;

        // non-fatal query counterpart to the cycle assert in Start; false on a cycle, and
        // writes "stage cycle: A -> B -> A" into outCycle when non-null (query/policy split)
        bool ValidateStageOrder(std::string *outCycle = nullptr) const;

    private:
        enum class StageDomain : uint8_t { Main, Fixed }; // fixed stages run 0..N times per frame
                                                           // inside FixedMain; a cross-domain edge
                                                           // has no meaning, so each domain sorts
                                                           // independently (decision B)

        struct StageRecord
        {
            uint32_t m_seq = 0;
            std::string_view m_name;
            bool m_isFixedMainDriver = false; // FixedMain runs a sub-loop, it holds no systems
            StageDomain m_domain = StageDomain::Main;
            std::vector<std::unique_ptr<ISystem>> m_systems;
        };

        struct StageEdge
        {
            std::size_t m_from = 0; // m_from runs BEFORE m_to
            std::size_t m_to = 0;
        };

        static constexpr float FIXED_STEP_SECONDS = 1.0f / 64.0f; // 64 Hz, section 14
        static constexpr uint32_t MAX_FIXED_STEPS_PER_FRAME = 8;  // sim budget per frame, decision D
        static constexpr float MAX_ACCUMULATED_SECONDS = 16.0f * FIXED_STEP_SECONDS; // backlog bound, 0.25 s, decision D
        static constexpr std::size_t NPOS = static_cast<std::size_t>(-1);

        std::size_t FindStageIndex(uint32_t seq) const;
        std::size_t AppendStage(TypeId id, bool isDriver, StageDomain domain);
        void InsertStage(TypeId id, uint32_t anchorSeq, bool after);
        void ConstrainStage(TypeId id, uint32_t otherSeq, bool after);
        void AddEdge(std::size_t fromIdx, std::size_t toIdx);
        bool TopoSort(StageDomain domain, std::vector<std::size_t> &outOrder, std::string *outCycle) const;
        void BuildStageOrder();
        void AddSystemToStage(uint32_t seq, std::unique_ptr<ISystem> system);
        void RunStage(World &world, StageRecord &stage);
        void RunFixedMain(World &world);

        // m_stages is append-only; the order vectors hold INDICES, never pointers, because
        // appending a stage can reallocate m_stages and dangle a stored StageRecord*
        std::vector<StageRecord> m_stages;
        std::vector<StageEdge> m_edges;
        std::vector<std::size_t> m_mainOrder; // empty until BuildStageOrder() runs in Start()
        std::vector<std::size_t> m_fixedOrder;
        float m_accumulator = 0.0f;
        bool m_started = false;
    };
}
