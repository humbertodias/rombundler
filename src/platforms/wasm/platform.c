#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "platform.h"
#include "libretro.h"
#include "utils.h"
#include "config.h"
#include "ini.h"
#include "options.h"

static GLFWwindow *window = NULL;
static bool should_close = false;
static void (*loop_frame)(void);
static void (*loop_cleanup)(void);

static void error_cb(int error, const char *description)
{
	(void)error;
	fprintf(stderr, "Error: %s\n", description);
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
	return "/save.srm";
}

void platform_prepare_core(const char *path)
{
	size_t n;

	if (!path || !path[0])
		return;
	n = strlen(path);
	if ((n >= 3 && !strcasecmp(path + n - 3, ".so")) ||
	    (n >= 4 && !strcasecmp(path + n - 4, ".dll")) ||
	    (n >= 3 && !strcasecmp(path + n - 3, ".js")) ||
	    (n >= 5 && !strcasecmp(path + n - 5, ".wasm")) ||
	    (n >= 6 && !strcasecmp(path + n - 6, ".dylib")))
		die("WASM cannot load '%s' at runtime.\n"
		    "This build has a core linked into the module.\n"
		    "Set core = dummy in config.ini, or rebuild with -DROMBUNDLER_CORE_LIBRARY=<core.a>.",
		    path);
#ifdef ROMBUNDLER_DUMMY_CORE
	if (strcasecmp(path, "dummy") != 0)
		die("This WASM build is the dummy core (color bars). config.ini core=%s does not load a core.\n"
		    "Rebuild with -DROMBUNDLER_CORE_LIBRARY=/path/to/core_libretro.a\n"
		    "and set core = dummy (or any name without .so/.js).",
		    path);
#endif
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

int platform_gl_entry_points_ok(void)
{
	return 1;
}

int platform_gles(void)
{
	return 1;
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
	if (ini_parse("/config.ini", cfg_handler, cfg) < 0 &&
	    ini_parse("./config.ini", cfg_handler, cfg) < 0)
		return 0;
	ini_parse("/options.ini", opt_handler, NULL);
	ini_parse("./options.ini", opt_handler, NULL);

	glfwSetErrorCallback(error_cb);
	if (!glfwInit())
		die("Failed to initialize GLFW");
	/* Joystick via HTML5 Gamepad API (glfwGetJoystick*); no glfwGetGamepadState. */
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

static void wasm_main_loop(void)
{
	if (platform_should_close()) {
		if (loop_cleanup) {
			loop_cleanup();
			loop_cleanup = NULL;
		}
		emscripten_cancel_main_loop();
		return;
	}
	if (loop_frame)
		loop_frame();
}

void platform_enter_loop(void (*frame)(void), void (*cleanup)(void))
{
	loop_frame = frame;
	loop_cleanup = cleanup;
	/* fps=0 follows display refresh; simulate_infinite_loop=1 never returns. */
	emscripten_set_main_loop(wasm_main_loop, 0, 1);
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
	(void)hw_context_type;
	(void)major;
	(void)minor;
	(void)fullscreen;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	window = glfwCreateWindow(width, height, title, NULL, NULL);
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
	return glfwJoystickPresent(port) == GLFW_TRUE;
}

/*
 * Desktop input.c passes GLFW_GAMEPAD_* indices. Emscripten's GLFW has no
 * glfwGetGamepadState; it exposes the HTML5 Standard Gamepad layout via
 * glfwGetJoystickButtons/Axes. Remap GLFW gamepad indices → HTML5.
 *
 * GLFW D-pad order is clockwise (UP, RIGHT, DOWN, LEFT), not U/D/L/R.
 */
static int wasm_html5_button(int glfw_gamepad_button)
{
	static const int map[] = {
		0, 1, 2, 3, 4, 5, /* A B X Y LB RB */
		8, 9, 16,          /* Back Start Guide */
		10, 11,            /* L3 R3 */
		12, 15, 13, 14     /* D-pad: UP RIGHT DOWN LEFT → HTML5 12/15/13/14 */
	};

	if (glfw_gamepad_button < 0 ||
	    glfw_gamepad_button >= (int)(sizeof(map) / sizeof(map[0])))
		return -1;
	return map[glfw_gamepad_button];
}

int platform_gamepad_button(int port, int button)
{
	int count = 0;
	int html5 = wasm_html5_button(button);
	const unsigned char *buttons;

	if (html5 < 0)
		return 0;
	buttons = glfwGetJoystickButtons(port, &count);
	if (!buttons || html5 >= count)
		return 0;
	return buttons[html5] == GLFW_PRESS;
}

float platform_gamepad_axis(int port, int axis)
{
	int count = 0;
	const float *axes = glfwGetJoystickAxes(port, &count);

	if (axis >= 0 && axis <= 3) {
		if (!axes || axis >= count)
			return 0.f;
		return axes[axis];
	}

	/* L2/R2: HTML5 usually puts triggers on buttons 6/7 (pressed only here). */
	if (axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER ||
	    axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER) {
		int bcount = 0;
		const unsigned char *buttons = glfwGetJoystickButtons(port, &bcount);
		int bi = (axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER) ? 6 : 7;

		if (buttons && bi < bcount && buttons[bi] == GLFW_PRESS)
			return 1.f;
		if (axes && axis < count)
			return axes[axis];
	}
	return 0.f;
}
