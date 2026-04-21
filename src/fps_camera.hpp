#pragma once

#include "./pch.hpp"

#include "./gl/window.hpp"

// control with WSADEQ + SHIFT/CTRL
class fps_camera {
public:
	float major_fov_deg = 90.0f;

	uint32_t width = 1;
	uint32_t height = 1;

	glm::vec3 pos = glm::vec3(0.0f);
	float pitch = 0; // degrees
	float yaw = 0;   // degrees

	float movement_speed = 10.0f; // units per second
	float movement_boost = 3.0f;
	float movement_slow = 0.1f;
	// number of full rots per full monitor width of mouse movement
	float sensitivity = 0.25f;

	float look_smoothing = 0.7f;
	float move_smoothing = 0.9f;

	void update_fps_pose_from_glfw_input(GLFWwindow *glfw_window, float dt, bool allow_mouse_capture = true);

	bool is_cursor_captured() const;

	glm::mat4 calc_view_mat() const;

	glm::mat4 calc_proj_mat() const;

private:
	uint32_t mouse_fix_iters_to_init = 2;
	float last_mouse_x = 0.0f;
	float last_mouse_y = 0.0f;
	float last_mouse_dx = 0.0f;
	float last_mouse_dy = 0.0f;
	glm::vec3 last_move_offset = glm::vec3(0.0f);

	bool captured_cursor_on_first_iter = false;
	bool captured_cursor = false;
};
