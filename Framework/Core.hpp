#pragma once

#include <cstdint>
#include <assert.h>
#include <format>

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