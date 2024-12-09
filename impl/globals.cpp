#include "mland/globals.h"
#include "mland/drm_backend.h"
#include "mland/mstate.h"
using namespace mland;

class NullBuffer final : public std::streambuf {
public:
	int overflow(const int c) override { return c; }
};

static NullBuffer nullBuffer{};

std::atomic<uint32_t> globals::bufferCount = 3;

std::atomic_flag _details::msgMutex = ATOMIC_FLAG_INIT;

std::ostream globals::debug{&nullBuffer};
std::ostream globals::info{&nullBuffer};
std::ostream globals::warn{&nullBuffer};
std::ostream globals::error{&nullBuffer};

MState globals::CompositorState;
