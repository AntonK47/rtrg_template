#pragma once

#include "VolkUtils.hpp"
#include "Math.hpp"

#define IM_VEC2_CLASS_EXTRA                                                     \
        constexpr ImVec2(const Framework::Math::Vector2& f) : x(f.x), y(f.y) {}                   \
        operator Framework::Math::Vector2() const { return Framework::Math::Vector2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                                     \
        constexpr ImVec4(const Framework::Math::Vector4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {}   \
        operator Framework::Math::Vector4() const { return Framework::Math::Vector4(x,y,z,w); }


#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>