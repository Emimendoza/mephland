#pragma once

#include "common.h"

namespace mland {
class Controller {
	struct impl;
	u_ptr<impl> p;

public:
	MCLASS(Controller);
	Controller(u_ptr<VInstance>& instance);
	Controller(Controller&& other) noexcept = default;
	Controller(const Controller&) = delete;
	~Controller();

	void run();

private:
	void refreshMonitors();
};
}
