#pragma once

#include "Core.hpp"

#include <array>
#include <mutex>

namespace Framework
{
	template <typename T, int size>
	struct Pool
	{
		std::array<U32, size> freeIndices;
		U32 nextFreeIndex{ 0 };
		std::array<T, size> entries;

		mutable std::mutex mutex;

		Pool()
		{
			for (U32 i = 0; i < size; i++)
			{
				freeIndices[i] = i;
			}
		}

		U32 Allocate()
		{
			std::lock_guard(mutex);
			assert(nextFreeIndex < size);
			const auto index = freeIndices[nextFreeIndex];
			nextFreeIndex++;
		}

		void Free(U32 index)
		{
			std::lock_guard(mutex);
			assert(nextFreeIndex > 0);
			nextFreeIndex--;
			freeIndices[nextFreeIndex] = index;
		}

		T& operator[](U32 index)
		{
			return entries[index];
		}

		const T& operator[](U32 index) const
		{
			return entries[index];
		}

		bool IsEmpty() const
		{
			return nextFreeIndex == 0;
		}
	};
} // namespace Framework