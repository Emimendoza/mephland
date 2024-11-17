#pragma once
#include <atomic>
#include <cstdint>
#include <ostream>
namespace mland::globals {
	// Doesn't need initialization

	extern float queuePriority;

	extern std::atomic<uint32_t> bufferCount;

	extern std::ostream debug;
	extern std::ostream info;
	extern std::ostream warn;
	extern std::ostream error;
}