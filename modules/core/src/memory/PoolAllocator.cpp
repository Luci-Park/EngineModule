#include <engine/core/memory/PoolAllocator.hpp>
#include "Align.hpp"

#include <algorithm>

namespace engine
{
    PoolAllocator::PoolAllocator(std::size_t blockSize, std::size_t blockCount, std::size_t align, MemoryTag tag)
        : m_blockCount(blockCount)
    {
        // blocks hold an intrusive next-ptr -> align must be >= alignof(void*).
        m_align = std::max(align, alignof(void*));
        m_blockSize = AlignUp((blockSize > sizeof(void*)) ? blockSize : sizeof(void*), m_align);

        m_buffer = static_cast<std::byte*>(AllocateRaw(m_blockSize * m_blockCount, m_align, AllocSite{tag}));
        if (!m_buffer)
        {
            m_blockCount = 0;
            return;
        }

        for (std::size_t i = 0; i < m_blockCount; ++i)
        {
            void* block = m_buffer + i * m_blockSize;
            *reinterpret_cast<void**>(block) = m_freeList;
            m_freeList = block;
        }
        m_freeCount = m_blockCount;
    }

    PoolAllocator::~PoolAllocator()
    {
        DeallocateRaw(m_buffer);
    }

    void* PoolAllocator::Allocate(std::size_t size, std::size_t align)
    {
        if (size > m_blockSize || align > m_align) return nullptr;  // doesn't fit
        if (!m_freeList) return nullptr;                            // exhausted

        void* block = m_freeList;
        m_freeList = *reinterpret_cast<void**>(block);
        --m_freeCount;
        return block;
    }

    void PoolAllocator::Deallocate(void* ptr)
    {
        if (!ptr) return;
        *reinterpret_cast<void**>(ptr) = m_freeList;
        m_freeList = ptr;
        ++m_freeCount;
    }
}
