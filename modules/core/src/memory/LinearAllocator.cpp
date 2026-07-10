#include <engine/core/memory/LinearAllocator.hpp>
#include "Align.hpp"

#include <cstdint>

namespace engine
{
    LinearAllocator::LinearAllocator(std::size_t bytes, MemoryTag tag)
        : m_capacity(bytes)
    {
        m_buffer = static_cast<std::byte *>(AllocateRaw(bytes, alignof(std::max_align_t), AllocSite{tag}));
        if (!m_buffer)
            m_capacity = 0; // alloc failed -> every Allocate() nullptrs
    }

    LinearAllocator::~LinearAllocator()
    {
        DeallocateRaw(m_buffer);
    }

    void *LinearAllocator::Allocate(std::size_t size, std::size_t align)
    {
        const auto cur = reinterpret_cast<std::uintptr_t>(m_buffer + m_offset);
        const auto aligned = AlignUp(cur, align);
        const std::size_t padding = static_cast<std::size_t>(aligned - cur);

        // subtraction form -> no m_offset+padding+size overflow
        const std::size_t avail = m_capacity - m_offset;
        if (padding > avail || size > avail - padding)
            return nullptr; // exhausted

        m_offset += padding + size;
        return reinterpret_cast<void *>(aligned);
    }

    void LinearAllocator::Deallocate(void * /*ptr*/)
    {
        // no-op: individual frees not supported, use Reset().
    }

    void LinearAllocator::Reset()
    {
        m_offset = 0;
    }
}
