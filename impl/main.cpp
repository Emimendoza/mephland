#include <mland/compositor.h>

using namespace mland;

int main() {
	Compositor<BackendType::eSDL> comp;
	comp.run();
}