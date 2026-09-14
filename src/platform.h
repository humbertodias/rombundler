#ifndef ROMBUNDLER_PLATFORM_H
#define ROMBUNDLER_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct config;

int platform_boot(struct config *cfg);
void platform_deinit(void);
void platform_fatal(const char *msg);
void platform_debug(const char *msg);
FILE *platform_fopen(const char *path, const char *mode);
const char *platform_srm_path(void);
void platform_prepare_core(const char *path);
int platform_gl_enable_texture_2d(void);
int platform_use_glsl_shaders(void);
int platform_has_audio(void);
/* Non-zero if required GL entry points are usable after gladLoadGLLoader.
 * Desktop verifies glad function pointers; other ports link GL symbols. */
int platform_gl_entry_points_ok(void);

int platform_gles(void);
int platform_use_fbo(void);
int platform_unpack_row_length(void);
void platform_draw_immediate_quad(const float quad[16]);

void platform_poll(void);
/* Runs frame until should_close. Calls cleanup once before returning (or
 * before cancelling the browser main loop on WebAssembly). */
void platform_enter_loop(void (*frame)(void), void (*cleanup)(void));
bool platform_should_close(void);
void platform_set_should_close(bool close);
void platform_swap_buffers(void);
void platform_set_swap_interval(int interval);
void platform_get_framebuffer_size(int *width, int *height);
void *platform_get_proc_address(const char *name);
int platform_create_window(int width, int height, const char *title,
	int fullscreen, int hide_cursor, unsigned hw_context_type, int major, int minor);
void platform_destroy_window(void);
int platform_window_ready(void);

int platform_key_down(int key);
int platform_mouse_button_left(void);
void platform_cursor_pos(double *x, double *y);

int platform_gamepad_present(int port);
int platform_gamepad_button(int port, int button);
float platform_gamepad_axis(int port, int axis);

#endif
