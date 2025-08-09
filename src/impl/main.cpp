#include <mland/sdl3_backend.h>
#include <mland/renderer.h>

using namespace mland;

int main() {
	shared_backend backend = std::make_shared<sdl3_backend>();
	renderer renderer(backend, true);
	renderer.refresh_displays();
	return 0;
}
