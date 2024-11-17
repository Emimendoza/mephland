#pragma once
#include <cstdint>

namespace mland{
	struct VShader {
		const std::size_t len{};
		const void* bytes{};
	};
	extern const VShader VERT_SHADER;
	extern const VShader FRAG_SHADER;
}
