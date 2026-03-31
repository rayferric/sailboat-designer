#pragma once

#include "./pch.hpp"

class imgui {
public:
	imgui(GLFWwindow *glfw_window);
	~imgui();

	bool is_cursor_hovering_over() const;

	void begin();

	void end();

	void draw();
};
