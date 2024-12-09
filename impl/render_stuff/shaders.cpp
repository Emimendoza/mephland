#include "mland/vshaders.h"

#include <cstdint>

using namespace mland;

static constexpr uint8_t bytes_vert[] = {
#embed "vertex.vert.spv"
};
const VShader mland::VERT_SHADER = {
	.len = sizeof(bytes_vert),
	.bytes = bytes_vert
};

static constexpr uint8_t bytes_frag[] = {
#embed "fragment.frag.spv"
};

const VShader mland::FRAG_SHADER = {
	.len = sizeof(bytes_frag),
	.bytes = bytes_frag
};