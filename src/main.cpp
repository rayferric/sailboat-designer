#include "./pch.hpp"

#include "./window.hpp"

int main() {
	window window;
	window.open(800, 600, "Sailboat Designer");

	// init on_draw
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.08, 0.08, 0.1, 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// init on_update
	std::chrono::time_point last_update_time = std::chrono::steady_clock::now();
	float elapsed_ms = 0.0f;

	// clang-format off
	window.run_loop({
		.on_draw = [&]() {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		},
		.on_update = [&]() {
			std::chrono::time_point now = std::chrono::steady_clock::now();
			float dt = std::chrono::duration<float>(
				now - last_update_time
			).count();
			last_update_time = now;

			// ...
		},
	    .on_resize = [&](uint32_t w, uint32_t h) {
			glViewport(0, 0, w, h);
		}
	});
	// clang-format on

	return 0;
}
