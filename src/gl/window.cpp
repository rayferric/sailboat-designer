#include "./window.hpp"

class timer {
public:
	timer() {
		last_update_time = std::chrono::steady_clock::now();
	}
	float dt() {
		std::chrono::time_point now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - last_update_time).count();
		last_update_time = now;
		return dt;
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> last_update_time;
};

//////////

void GLAPIENTRY opengl_error_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *userParam
) {
	// ignore non-significant codes
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
		return;
	}

	printf("OpenGL Debug Message (%d): %s\n", id, message);
	printf("Source: %d, Type: %d, Severity: %d\n", source, type, severity);
}

//////////

window::window() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

window::~window() {
	glfwTerminate();
}

void window::open(uint32_t w, uint32_t h, const std::string &title) {
	// create window
	glfw_window = glfwCreateWindow(w, h, title.c_str(), NULL, NULL);
	if (glfw_window == NULL) {
		glfwTerminate();
		throw std::runtime_error("glfwCreateWindow failed");
	}
	glfwMakeContextCurrent(glfw_window);

	// load gl
	if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
		throw std::runtime_error("gladLoadGL failed");
	}

	// set error callback
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(opengl_error_callback, nullptr);
	glDebugMessageControl(
	    GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE
	);

	// add this user ptr
	glfwSetWindowUserPointer(glfw_window, this);
}

void window::run_loop(const window::loop_info &info) {
	cur_loop_info = info;

	// hook window resize -> viewport resize
	int w, h;
	glfwGetFramebufferSize(glfw_window, &w, &h);
	window::fbsz_cb(glfw_window, w, h);
	glfwSetFramebufferSizeCallback(glfw_window, window::fbsz_cb);

	// timer for on_update(dt)
	timer t;

	// main loop
	while (!glfwWindowShouldClose(glfw_window)) {
		glfwPollEvents();
		if (cur_loop_info.on_update) {
			cur_loop_info.on_update(t.dt());
		}

		if (cur_loop_info.on_draw) {
			cur_loop_info.on_draw();
		}
		glfwSwapBuffers(glfw_window);
	}
}

// static
void window::fbsz_cb(GLFWwindow *glfw_window, int width, int height) {
	window *this_ptr =
	    static_cast<window *>(glfwGetWindowUserPointer(glfw_window));
	if (this_ptr->cur_loop_info.on_resize) {
		this_ptr->cur_loop_info.on_resize(width, height);
	}
}
