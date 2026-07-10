#pragma once
#include <engine/core/memory/IAllocator.hpp>
#include <engine/core/memory/Memory.hpp>

#include <cstddef>

namespace engine
{
    class ENGINE_CORE_API LinearAllocator final : public IAllocator
    {
    public:
        explicit LinearAllocator(std::size_t bytes, MemoryTag tag = MemoryTag::Scratch);
        ~LinearAllocator() override;

        LinearAllocator(const LinearAllocator &) = delete;
        LinearAllocator &operator=(const LinearAllocator &) = delete;

        void *Allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) override;
        void Deallocate(void *ptr) override;

        void Reset();

        std::size_t Capacity() const { return m_capacity; }
        std::size_t Used() const { return m_offset; }

    private:
        std::byte *m_buffer = nullptr;
        std::size_t m_capacity = 0;
        std::size_t m_offset = 0;
    };
}
