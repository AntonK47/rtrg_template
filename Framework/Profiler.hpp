#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define TracyFunction __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define TracyFunction __FUNCSIG__
#endif

#ifndef RTRG_ENABLE_PROFILER
#define RTRG_PROFILER_CALLSTACK_DEPTH 0
#ifdef TRACY_ENABLE
#undef TRACY_ENABLE
#endif
#endif

#include <tracy/Tracy.hpp>