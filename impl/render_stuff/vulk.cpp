#include <string>

#include "mland/vulk.h"
#include "mland/common.h"

template <typename T>
std::string mland::to_str(const T& t) {
	return vk::to_string(t);
}

template std::string mland::to_str<vk::Result>(const vk::Result& t);
template std::string mland::to_str<vk::Format>(const vk::Format& t);
template std::string mland::to_str<vk::ColorSpaceKHR>(const vk::ColorSpaceKHR& t);
template std::string mland::to_str<vk::PresentModeKHR>(const vk::PresentModeKHR& t);
template std::string mland::to_str<vk::Flags<vk::ImageUsageFlagBits>>(const vk::Flags<vk::ImageUsageFlagBits>& t);
template std::string mland::to_str<vk::QueueFlags>(const vk::QueueFlags& t);







