#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "libretro.h"
#include "mappings.h"
#include "utils.h"
#include "config.h"
#include "ini.h"
#include "options.h"

static GLFWwindow *window = NULL;
static bool should_close = false;

static void error_cb(int error, const char *description)
{
	(void)error;
	fprintf(stderr, "Error: %s\n", description);
}

static void joystick_callback(int jid, int event)
{
	if (event == GLFW_CONNECTED)
		printf("%s %s\n", glfwGetGamepadName(jid), glfwGetJoystickGUID(jid));
	else if (event == GLFW_DISCONNECTED)
		printf("Joypad %d disconnected\n", jid);
}

void platform_fatal(const char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	fflush(stderr);
	exit(EXIT_FAILURE);
}

void platform_debug(const char *msg)
{
	if (!msg)
		return;
	fputs(msg, stderr);
	fputc('\n', stderr);
	fflush(stderr);
}

FILE *platform_fopen(const char *path, const char *mode)
{
	if (!path || !path[0])
		return NULL;
	return fopen(path, mode);
}

const char *platform_srm_path(void)
{
	return "./save.srm";
}

void platform_prepare_core(const char *path)
{
	(void)path;
}

int platform_gl_enable_texture_2d(void)
{
	return 1;
}

int platform_use_glsl_shaders(void)
{
	return 1;
}

int platform_has_audio(void)
{
	return 1;
}

int platform_gl_check_proc_pointers(void)
{
	return 1;
}

int platform_gles(void)
{
	return 0;
}

int platform_use_fbo(void)
{
	return 1;
}

int platform_unpack_row_length(void)
{
	return 1;
}

void platform_draw_immediate_quad(const float quad[16])
{
	(void)quad;
}

int platform_boot(struct config *cfg)
{
	if (ini_parse("./config.ini", cfg_handler, cfg) < 0)
		return 0;
	ini_parse("./options.ini", opt_handler, NULL);

	glfwSetErrorCallback(error_cb);
	if (!glfwInit())
		die("Failed to initialize GLFW");
	if (!glfwUpdateGamepadMappings(mappings))
		die("Failed to load mappings");
	else
		printf("Updated mappings\n");
	glfwSetJoystickCallback(joystick_callback);
	return 1;
}

void platform_deinit(void)
{
	glfwTerminate();
}

void platform_poll(void)
{
	glfwPollEvents();
	if (window && glfwWindowShouldClose(window))
		should_close = true;
}

void platform_enter_loop(void (*frame)(void), void (*cleanup)(void))
{
	while (!platform_should_close())
		frame();
	if (cleanup)
		cleanup();
}

bool platform_should_close(void)
{
	return should_close;
}

void platform_set_should_close(bool close)
{
	should_close = close;
	if (window)
		glfwSetWindowShouldClose(window, close);
}

void platform_swap_buffers(void)
{
	if (window)
		glfwSwapBuffers(window);
}

void platform_set_swap_interval(int interval)
{
	glfwSwapInterval(interval);
}

void platform_get_framebuffer_size(int *width, int *height)
{
	if (window)
		glfwGetFramebufferSize(window, width, height);
	else {
		*width = 0;
		*height = 0;
	}
}

void *platform_get_proc_address(const char *name)
{
	return (void *)glfwGetProcAddress(name);
}

int platform_create_window(int width, int height, const char *title,
	int fullscreen, int hide_cursor, unsigned hw_context_type, int major, int minor)
{
	if (hw_context_type == RETRO_HW_CONTEXT_OPENGL_CORE || major >= 3) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	} else {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	}

	switch (hw_context_type) {
	case RETRO_HW_CONTEXT_OPENGL_CORE:
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		break;
	case RETRO_HW_CONTEXT_OPENGLES2:
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
		break;
	case RETRO_HW_CONTEXT_OPENGL:
		if (major >= 3)
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
		break;
	default:
		die("Unsupported hw context %i. (only OPENGL, OPENGL_CORE and OPENGLES2 supported)", hw_context_type);
	}

	GLFWmonitor *monitor = NULL;
	if (fullscreen) {
		int count;
		monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *modes = glfwGetVideoModes(monitor, &count);
		const GLFWvidmode mode = modes[count - 1];
		width = mode.width;
		height = mode.height;
	}

	window = glfwCreateWindow(width, height, title, monitor, NULL);
	if (!window)
		die("Failed to create window.");

	if (hide_cursor)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	return 1;
}

void platform_destroy_window(void)
{
	if (window) {
		glfwDestroyWindow(window);
		window = NULL;
	}
}

int platform_window_ready(void)
{
	return window != NULL;
}

int platform_key_down(int key)
{
	return window && glfwGetKey(window, key) == GLFW_PRESS;
}

int platform_mouse_button_left(void)
{
	return window && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
}

void platform_cursor_pos(double *x, double *y)
{
	if (window)
		glfwGetCursorPos(window, x, y);
	else {
		*x = 0;
		*y = 0;
	}
}

int platform_gamepad_present(int port)
{
	return glfwJoystickIsGamepad(port);
}

int platform_gamepad_button(int port, int button)
{
	GLFWgamepadstate pad;
	if (!glfwGetGamepadState(port, &pad))
		return 0;
	return pad.buttons[button];
}

float platform_gamepad_axis(int port, int axis)
{
	GLFWgamepadstate pad;
	if (!glfwGetGamepadState(port, &pad))
		return 0.f;
	return pad.axes[axis];
}
