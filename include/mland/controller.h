#pragma once

#include "common.h"

namespace mland {

// Special singleton class that controls the entire compositor
class Controller {
public:
	MCLASS(Controller);


	static void create(u_ptr<VInstance>&& instance_);
	static void run();
	static void stop();
	static void refreshMonitors();
	static void requestRender();

	static void waitForStop();

private:
	Controller() = delete;
};
}
