#pragma once

#include <cstdint>
#include <assert.h>
#include <format>

#define TRACY_NO_SAMPLING
#include <tracy/Tracy.hpp>

inline void* operator new(std::size_t count)
{
	auto ptr = malloc(count);
	TracyAllocS(ptr, count, 30);
	return ptr;
}
inline void operator delete(void* ptr) noexcept
{
	TracyFreeS(ptr, 30);
	free(ptr);
}

template <typename... Args>
std::string runtime_format(std::string_view rt_fmt_str, Args&&... args)
{
	return std::vformat(rt_fmt_str, std::make_format_args(args...));
}

namespace Framework
{
	using U8 = uint8_t;
	using U16 = uint16_t;
	using U32 = uint32_t;
	using U64 = uint64_t;

	using I8 = int8_t;
	using I16 = int16_t;
	using I32 = int32_t;
	using I64 = int64_t;

	using Float = float;

	template <typename T1, typename T2>
	inline constexpr T1 As(T2 x)
	{
		assert(false);
		return T1{};
	}

	template <>
	inline constexpr U32 As<U32, Float>(Float x)
	{
		return static_cast<U32>(x);
	}
} // namespace Framework