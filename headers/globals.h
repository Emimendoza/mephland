#pragma once
#include <atomic>
#include <cstdint>
#include <ostream>

namespace mland {
struct MState;
}

namespace mland::globals {
// Doesn't need initialization
extern std::atomic<uint32_t> bufferCount;
extern MState CompositorState;

extern std::ostream debug;
extern std::ostream info;
extern std::ostream warn;
extern std::ostream error;
}
