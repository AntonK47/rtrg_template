#pragma once

#include <cstdint>
#include <assert.h>

namespace Framework
{
	using U8 = uint8_t;
	using U16 = uint16_t;
	using U32 = uint32_t;

	using I8 = int8_t;
	using I16 = int16_t;
	using I32 = int32_t;

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