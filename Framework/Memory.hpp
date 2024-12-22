#pragma once

#include <memory>

#include "Profiler.hpp"

inline const char* const cpuMemoryPoolName = "CPU Memory Pool";

#ifdef RTRG_ENABLE_PROFILER
inline void* operator new(std::size_t count)
{
	auto ptr = std::malloc(count);
	TracyAllocNS(ptr, count, RTRG_PROFILER_CALLSTACK_DEPTH, cpuMemoryPoolName);
	return ptr;
}
inline void operator delete(void* ptr) noexcept
{
	TracyFreeNS(ptr, RTRG_PROFILER_CALLSTACK_DEPTH, cpuMemoryPoolName);
	std::free(ptr);
}
#endif

namespace Framework
{


} // namespace Framework