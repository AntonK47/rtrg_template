#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_LEFT_HANDED
#include <glm/gtx/quaternion.hpp>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Core.hpp"

namespace Framework
{
	namespace Math
	{
		// using namespace glm;

		using Quaternion = glm::quat;

		using Vector4 = glm::vec4;
		using Vector3 = glm::vec3;
		using Vector2 = glm::vec2;

		// using Matrix4x4 = glm::mat4;

		inline Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, F32 interpolationFactor)
		{
			return glm::slerp(q1, q2, interpolationFactor);
		}

		inline Vector3 Mix(const Vector3& v1, const Vector3& v2, F32 interpolationFactor)
		{
			return glm::mix(v1, v2, interpolationFactor);
		}

		struct Matrix4x4 final : public glm::mat4
		{
			Matrix4x4(){}
			Matrix4x4(const glm::mat4& m)
			{
				std::memcpy(this, &m, sizeof(glm::mat4));
			}

			operator glm::mat4() const
			{
				return *this;
			}

			static Matrix4x4 From(const Quaternion& quaternion)
			{
				return glm::toMat4(quaternion);
			}

			static Matrix4x4 Identity()
			{
				return glm::identity<glm::mat4>();
			}

			static Matrix4x4 TranslationFrom(const Vector3& translation)
			{
				return glm::translate(Matrix4x4::Identity(), translation);
			}
		};

		inline F32 Modulo(F32 x, F32 y)
		{
			return std::fmodf(x, y);
		}

		inline constexpr U32 Modulo(U32 x, U32 y)
		{
			return x % y;
		}

		inline F32 Floor(F32 x)
		{
			return std::floorf(x);
		}

		inline F32 Ceil(F32 x)
		{
			return std::ceilf(x);
		}
	} // namespace Math
} // namespace Framework