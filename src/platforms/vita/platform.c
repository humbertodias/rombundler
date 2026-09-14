#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/gxm.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <vitaGL.h>
#include <vitashark.h>

#include "platform.h"
#include "libretro.h"
#include "utils.h"
#include "config.h"
#include "ini.h"
#include "options.h"

#define VITA_APP_DIR "ux0:/data/rombundler"
#define VITA_FB_W 960
#define VITA_FB_H 544
#define VITA_EXIT_COMBO_FRAMES 45

static bool should_close = false;
static bool window_ready = false;
static SceCtrlData pad;
static int exit_combo_frames;

static void ensure_app_dir(void)
{
	sceIoMkdir("ux0:/data", 0777);
	sceIoMkdir(VITA_APP_DIR, 0777);
}

static int write_error_log(const char *msg)
{
	SceUID fd;
	char line[512];
	int n;

	ensure_app_dir();
	if (!msg)
		msg = "";
	sceClibPrintf("ROMBundler: %s\n", msg);
	n = snprintf(line, sizeof(line), "%s%s", msg, (msg[0] && msg[strlen(msg) - 1] != '\n') ? "\n" : "");
	if (n < 0)
		return 0;
	fd = sceIoOpen(VITA_APP_DIR "/error.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0)
		return 0;
	sceIoWrite(fd, line, (SceSize)strlen(line));
	sceIoClose(fd);
	sceIoSync("ux0:", 0);
	return 1;
}

void platform_debug(const char *msg)
{
	if (msg && msg[0])
		sceClibPrintf("ROMBundler: %s\n", msg);
}

int platform_boot(struct config *cfg)
{
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	memset(&pad, 0, sizeof(pad));
	exit_combo_frames = 0;

	if (ini_parse(VITA_APP_DIR "/config.ini", cfg_handler, cfg) < 0 &&
	    ini_parse("app0:/config.ini", cfg_handler, cfg) < 0)
		return 0;
	ini_parse(VITA_APP_DIR "/options.ini", opt_handler, NULL);
	ini_parse("app0:/options.ini", opt_handler, NULL);
	return 1;
}

void platform_fatal(const char *msg)
{
	write_error_log(msg);
	sceKernelDelayThread(300000);
	sceKernelExitProcess(EXIT_FAILURE);
}

FILE *platform_fopen(const char *path, const char *mode)
{
	FILE *f;

	if (!path || !path[0])
		return NULL;
	f = fopen(path, mode);
	if (f)
		return f;
	if (!strncmp(path, "ux0:", 4) && path[4] != '/') {
		char alt[512];
		snprintf(alt, sizeof(alt), "ux0:/%s", path + 4);
		return fopen(alt, mode);
	}
	return NULL;
}

const char *platform_srm_path(void)
{
	return VITA_APP_DIR "/save.srm";
}

void platform_prepare_core(const char *path)
{
	size_t n;

	if (!path || !path[0])
		return;
	n = strlen(path);
	if ((n >= 5 && !strcasecmp(path + n - 5, ".self")) ||
	    (n >= 4 && (!strcasecmp(path + n - 4, ".nro") || !strcasecmp(path + n - 4, ".dll") ||
			!strcasecmp(path + n - 4, ".vpk"))) ||
	    (n >= 3 && !strcasecmp(path + n - 3, ".so")) ||
	    (n >= 6 && !strcasecmp(path + n - 6, ".dylib")))
		die("Vita cannot load '%s' at runtime.\n"
		    "This build has a core linked into the VPK.\n"
		    "Set core = dummy in config.ini, or rebuild with -DROMBUNDLER_CORE_LIBRARY=<core.a>.",
		    path);
#ifdef ROMBUNDLER_DUMMY_CORE
	if (strcasecmp(path, "dummy") != 0)
		die("This VPK is the dummy core (color bars). config.ini core=%s does not load a core.\n"
		    "Rebuild with -DROMBUNDLER_CORE_LIBRARY=/path/to/genesis_plus_gx_libretro.a\n"
		    "and set core = dummy (or any name without .self).",
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

int platform_gl_check_proc_pointers(void)
{
	return 0;
}

int platform_gles(void)
{
	return 1;
}

int platform_use_fbo(void)
{
	return 0;
}

int platform_unpack_row_length(void)
{
	return 0;
}

void platform_draw_immediate_quad(const float quad[16])
{
	glEnable(GL_TEXTURE_2D);
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(quad[2], quad[3]);
	glVertex2f(quad[0], quad[1]);
	glTexCoord2f(quad[6], quad[7]);
	glVertex2f(quad[4], quad[5]);
	glTexCoord2f(quad[10], quad[11]);
	glVertex2f(quad[8], quad[9]);
	glTexCoord2f(quad[14], quad[15]);
	glVertex2f(quad[12], quad[13]);
	glEnd();
}

void platform_deinit(void)
{
	window_ready = false;
}

void platform_poll(void)
{
	int n = sceCtrlPeekBufferPositive(0, &pad, 1);

	if (n < 1 || pad.buttons == 0xFFFFFFFFu) {
		memset(&pad, 0, sizeof(pad));
		exit_combo_frames = 0;
		return;
	}
	if ((pad.buttons & SCE_CTRL_START) && (pad.buttons & SCE_CTRL_SELECT)) {
		if (++exit_combo_frames >= VITA_EXIT_COMBO_FRAMES)
			should_close = true;
	} else {
		exit_combo_frames = 0;
	}
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
}

void platform_swap_buffers(void)
{
	if (window_ready)
		vglSwapBuffers(GL_FALSE);
}

void platform_set_swap_interval(int interval)
{
	vglWaitVblankStart(interval > 0 ? GL_TRUE : GL_FALSE);
}

void platform_get_framebuffer_size(int *width, int *height)
{
	*width = VITA_FB_W;
	*height = VITA_FB_H;
}

void *platform_get_proc_address(const char *name)
{
	if (!name || !name[0])
		return NULL;
	return vglGetProcAddress(name);
}

static const char *find_shacc(void)
{
	static const char *paths[] = {
		"ur0:/data/libshacccg.suprx",
		"ur0:data/libshacccg.suprx",
		"ur0:/data/external/libshacccg.suprx",
	};
	SceIoStat st;
	unsigned i;

	for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		memset(&st, 0, sizeof(st));
		if (sceIoGetstat(paths[i], &st) >= 0 && st.st_size >= 1000000)
			return paths[i];
	}
	return NULL;
}

int platform_create_window(int width, int height, const char *title,
	int fullscreen, int hide_cursor, unsigned hw_context_type, int major, int minor)
{
	SceAppUtilInitParam init;
	SceAppUtilBootParam bootp;
	const char *shacc;

	(void)width;
	(void)height;
	(void)title;
	(void)fullscreen;
	(void)hide_cursor;
	(void)hw_context_type;
	(void)major;
	(void)minor;

	memset(&init, 0, sizeof(init));
	memset(&bootp, 0, sizeof(bootp));
	sceAppUtilInit(&init, &bootp);

	/* Return is "resolution fell back", not success. 0 = requested size. */
	(void)vglInitWithCustomSizes(0x200000, VITA_FB_W, VITA_FB_H,
		8 * 1024 * 1024, 0, 0, 0, SCE_GXM_MULTISAMPLE_NONE);

	shacc = find_shacc();
	if (!shacc)
		die("Missing ur0:/data/libshacccg.suprx (install with ShaRKBR33D).");
	if (shark_init(shacc) < 0)
		die("shark_init failed (need libshacccg.suprx from ShaRKBR33D).");
	vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);

	window_ready = true;
	return 1;
}

void platform_destroy_window(void)
{
	window_ready = false;
}

int platform_window_ready(void)
{
	return window_ready;
}

int platform_key_down(int key)
{
	(void)key;
	return 0;
}

int platform_mouse_button_left(void)
{
	return 0;
}

void platform_cursor_pos(double *x, double *y)
{
	*x = 0;
	*y = 0;
}

int platform_gamepad_present(int port)
{
	return port == 0;
}

int platform_gamepad_button(int port, int button)
{
	unsigned b;

	if (port != 0)
		return 0;
	b = pad.buttons;
	switch (button) {
	case RETRO_DEVICE_ID_JOYPAD_B: return !!(b & SCE_CTRL_CROSS);
	case RETRO_DEVICE_ID_JOYPAD_Y: return !!(b & SCE_CTRL_SQUARE);
	case RETRO_DEVICE_ID_JOYPAD_SELECT: return !!(b & SCE_CTRL_SELECT);
	case RETRO_DEVICE_ID_JOYPAD_START: return !!(b & SCE_CTRL_START);
	case RETRO_DEVICE_ID_JOYPAD_UP: return !!(b & SCE_CTRL_UP);
	case RETRO_DEVICE_ID_JOYPAD_DOWN: return !!(b & SCE_CTRL_DOWN);
	case RETRO_DEVICE_ID_JOYPAD_LEFT: return !!(b & SCE_CTRL_LEFT);
	case RETRO_DEVICE_ID_JOYPAD_RIGHT: return !!(b & SCE_CTRL_RIGHT);
	case RETRO_DEVICE_ID_JOYPAD_A: return !!(b & SCE_CTRL_CIRCLE);
	case RETRO_DEVICE_ID_JOYPAD_X: return !!(b & SCE_CTRL_TRIANGLE);
	case RETRO_DEVICE_ID_JOYPAD_L: return !!(b & SCE_CTRL_LTRIGGER);
	case RETRO_DEVICE_ID_JOYPAD_R: return !!(b & SCE_CTRL_RTRIGGER);
	default: return 0;
	}
}

static float stick_axis(unsigned char v)
{
	return ((float)v - 128.f) / 128.f;
}

float platform_gamepad_axis(int port, int axis)
{
	if (port != 0)
		return 0.f;
	switch (axis) {
	case 0: return stick_axis(pad.lx);
	case 1: return -stick_axis(pad.ly);
	case 2: return stick_axis(pad.rx);
	case 3: return -stick_axis(pad.ry);
	default: return 0.f;
	}
}
