#pragma once
#include <engine/core/memory/IAllocator.hpp>
#include <engine/core/memory/Memory.hpp>

#include <cstddef>

namespace engine
{
    // fixed-size recycled blocks, intrusive free-lis
    class ENGINE_CORE_API PoolAllocator final : public IAllocator
    {
    public:
        PoolAllocator(std::size_t blockSize, std::size_t blockCount,
                      std::size_t align = alignof(std::max_align_t), MemoryTag tag = MemoryTag::General);
        ~PoolAllocator() override;

        PoolAllocator(const PoolAllocator &) = delete;
        PoolAllocator &operator=(const PoolAllocator &) = delete;

        // size/align must fit the configured block, else nullptr. nullptr on exhaustion too
        void *Allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) override;
        void Deallocate(void *ptr) override;

        std::size_t BlockSize() const { return m_blockSize; }
        std::size_t BlockCount() const { return m_blockCount; }
        std::size_t FreeCount() const { return m_freeCount; }

    private:
        std::byte *m_buffer = nullptr;
        std::size_t m_blockSize = 0;
        std::size_t m_blockCount = 0;
        std::size_t m_align = 0;
        void *m_freeList = nullptr;
        std::size_t m_freeCount = 0;
    };
}
