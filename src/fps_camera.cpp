#include "fps_camera.hpp"

glm::mat4 fps_camera::calc_view_mat() const {
	glm::mat4 tf = glm::mat4(1.0f);
	tf = glm::translate(tf, pos);
	tf = glm::rotate(tf, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
	tf = glm::rotate(tf, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
	return glm::inverse(tf);
}

glm::mat4 fps_camera::calc_proj_mat() const {
	float fov_y_deg = major_fov_deg;
	if (width > height) {
		// scale down vertical FOV when screen is horizontal
		float tan_half_fov = std::tan(glm::radians(major_fov_deg * 0.5f));
		tan_half_fov *= (float)height / (float)width;
		fov_y_deg = glm::degrees(2.0f * std::atan(tan_half_fov));
	}
	return glm::perspective(
	    glm::radians(fov_y_deg), (float)width / (float)height, 0.1f, 1000.0f
	);
}

void fps_camera::update_fps_pose_from_glfw_input(GLFWwindow *window, float dt, bool prevent_mouse_capture) {
	// if esc pressed, release cursor
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		captured_cursor = false;
	}

	// if clicked anywhere in the window, capture cursor
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !prevent_mouse_capture) {
		captured_cursor = true;
	}

	// set cursor mode based on captured state
	glfwSetInputMode(
	    window,
	    GLFW_CURSOR,
	    captured_cursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
	);

	// calculate mouse delta
	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	if (last_mouse_x == 0 && last_mouse_y == 0) {
		last_mouse_x = static_cast<float>(mouse_x);
		last_mouse_y = static_cast<float>(mouse_y);
	}
	float mouse_dx = static_cast<float>(mouse_x) - last_mouse_x;
	float mouse_dy = static_cast<float>(mouse_y) - last_mouse_y;
	last_mouse_x = static_cast<float>(mouse_x);
	last_mouse_y = static_cast<float>(mouse_y);

	// smooth mouse delta
	mouse_dx =
	    mouse_dx * (1.0f - look_smoothing) + last_mouse_dx * look_smoothing;
	mouse_dy =
	    mouse_dy * (1.0f - look_smoothing) + last_mouse_dy * look_smoothing;
	last_mouse_dx = mouse_dx;
	last_mouse_dy = mouse_dy;

	if (mouse_fix_iters_to_init > 0) {
		// skip first few iterations to allow for mouse deltas to stabilize
		mouse_fix_iters_to_init--;
		return;
	}

	// update rot and move_offset if captured
	glm::vec3 move_offset = glm::vec3(0.0f);
	if (captured_cursor) {
		// remap sensitivity from rots per screen to deg/px
		// > get monitor width
		int monitor_width, monitor_height;
		glfwGetMonitorWorkarea(
		    glfwGetPrimaryMonitor(),
		    nullptr,
		    nullptr,
		    &monitor_width,
		    &monitor_height
		);
		float unit_deg_per_px = 360.0f / monitor_width;
		float movement_scale = unit_deg_per_px * sensitivity;

		// compute current pitch/yaw
		pitch -= mouse_dy * movement_scale;
		yaw -= mouse_dx * movement_scale;
		pitch = std::clamp(pitch, -89.0f, 89.0f);

		// read WSADEQ input
		constexpr glm::vec3 right(1.0f, 0.0f, 0.0f);
		constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);
		constexpr glm::vec3 back(0.0f, 0.0f, 1.0f);
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			move_offset -= back * dt * movement_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			move_offset += back * dt * movement_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			move_offset -= right * dt * movement_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			move_offset += right * dt * movement_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
			move_offset += up * dt * movement_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
			move_offset -= up * dt * movement_speed;
		}
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
			move_offset *= movement_boost;
		}
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
			move_offset *= movement_slow;
		}

		// rotate move offset using rpy
		glm::mat4 rot =
		    glm::rotate(glm::mat4(1.0f), glm::radians(pitch), right);
		rot = glm::rotate(glm::mat4(1.0f), glm::radians(yaw), up) * rot;
		move_offset = glm::vec3(rot * glm::vec4(move_offset, 0.0f));
	}

	// smooth move_offset
	move_offset = move_offset * (1.0f - move_smoothing) +
					last_move_offset * move_smoothing;
	last_move_offset = move_offset;

	pos += move_offset;
}

bool fps_camera::is_cursor_captured() const {
	return captured_cursor;
}