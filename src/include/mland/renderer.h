#pragma once
#include "backend.h"
#include <memory>

namespace vk {
struct Instance;
}

namespace mland {
class renderer {
public:
	renderer(const shared_backend& backend, bool validation_layers = false);
	~renderer();

	void refresh_displays();

	[[nodiscard]]
	vk::Instance get_instance() const;

private:
	struct impl;
	std::unique_ptr<impl> _p{};
};
}