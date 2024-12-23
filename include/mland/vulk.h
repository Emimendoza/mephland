#pragma once
#include <string>
// Common header for Vulkan API among other things

#define VULKAN_HPP_ASSERT_ON_RESULT(X)
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_RAII_NO_EXCEPTIONS
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_hash.hpp>


namespace mland {
namespace vkr = vk::raii;

// The vulkan to string functions are massive and always inline so we want to wrap them
template <typename T>
std::string to_str(const T& t);

// constants that may be used in multiple places
static constexpr auto VULKAN_VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
}



