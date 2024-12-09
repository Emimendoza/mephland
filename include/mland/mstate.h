#pragma once
#include <atomic>

namespace mland {
// Represents the state of the compositor
struct MState {
	std::atomic<uint32_t> windowCount{0};
};
}
