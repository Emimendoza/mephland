#include <csignal>

#include "mland/controller.h"
#include "mland/vinstance.h"
#include "mland/vdisplay.h"
#include "mland/wayland_server.h"
#include "mland/interfaces/compositor.h"
using namespace mland;

namespace {
std::mutex displayMutex{};
vec<s_ptr<VDisplay>> displays{};
u_ptr<WLServer> server{};
u_ptr<VInstance> instance;
}

void Controller::create(u_ptr<VInstance>&& instance_) {
	MDEBUG << "Controller created" << endl;
	displays.clear();
	instance = std::move(instance_);
	server = std::make_unique<WLServer>();
}

void Controller::refreshMonitors() {
	MDEBUG << "Refreshing monitors" << endl;
	{
		std::lock_guard lock(displayMutex);
		vec<s_ptr<VDisplay>> newDisplays;
		for (const auto &display: displays) {
			if (display->isGood())
				newDisplays.push_back(display);
		}
		displays = std::move(newDisplays);
	}


	for (const auto& dev : instance->refreshDevices()) {
		auto& device = instance->getDevice(dev);
		auto monitors = device.updateMonitors();
		for (auto& monitor : monitors) {
			if (!monitor->isGood())
				continue;
			monitor->bindToWayland(*server);
			std::lock_guard lock(displayMutex);
			displays.push_back(monitor);
		}
	}
}

// TODO: Move this somewhere else

static WLServer* pServer = nullptr;

static void signalHandler(int signal) {
	MCLASS(SigH);
	MINFO << "Caught signal " << signal << endl;
	if (signal == SIGINT || signal == SIGTERM) {
		MINFO << "Stopping server" << endl;
		if (pServer) {
			pServer->stop();
		}
	}
}

void Controller::run() {
	MDEBUG << "Running controller" << endl;
	pServer = server.get();
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	interfaces::Compositor compositor(server->getDisplay());

	refreshMonitors();
	VDisplay::setMaxTimeBetweenFrames(std::chrono::milliseconds(50));
	MDEBUG << "Starting server" << endl;
	while (!server->stopped_.test()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	server->waitForStop();
}

void Controller::requestRender() {
	MDEBUG << "Requesting render" << endl;
	VDisplay::requestRender();
}

void Controller::stop() {
	MDEBUG << "Stopping controller" << endl;
	while(!instance) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	server->stop();
	std::lock_guard lock(displayMutex);
	displays.clear();
	server->waitForStop();
}