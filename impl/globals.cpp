#include "globals.h"

using namespace mland;

class NullBuffer final : public std::streambuf {
public:
	int overflow(const int c) override { return c; }
};

static NullBuffer nullBuffer{};

float globals::queuePriority = 1.0f;

std::atomic<uint32_t> globals::bufferCount = 3;

std::ostream globals::debug(&nullBuffer);
std::ostream globals::info(&nullBuffer);
std::ostream globals::warn(&nullBuffer);
std::ostream globals::error(&nullBuffer);

