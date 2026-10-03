#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "platform.h"
#include "libretro.h"
#include "utils.h"
#include "config.h"
#include "ini.h"
#include "options.h"

#define MAX_PADS 5
#define NX_APP_DIR "sdmc:/switch/rombundler"
#define NX_ERROR_LOG NX_APP_DIR "/error.log"

static EGLDisplay s_display = EGL_NO_DISPLAY;
static EGLContext s_context = EGL_NO_CONTEXT;
static EGLSurface s_surface = EGL_NO_SURFACE;
static bool should_close = false;
static bool window_ready = false;
static PadState pads[MAX_PADS];
static bool pad_inited[MAX_PADS];

void switch_trace(const char *msg);
void switch_thread_pool_init(void);
void switch_audio_boot(void);
void switch_reserve_device_memory(void);
void switch_reserves_enable(void);
void create_window(int width, int height);

static int switch_log_fd = -1;
static pthread_mutex_t switch_log_mu = PTHREAD_MUTEX_INITIALIZER;

static void switch_log_open_unlocked(void)
{
	if (switch_log_fd >= 0)
		return;
	mkdir("sdmc:/switch", 0755);
	mkdir(NX_APP_DIR, 0755);
	switch_log_fd = open(NX_APP_DIR "/trace.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (switch_log_fd < 0)
		switch_log_fd = open("/switch/rombundler/trace.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

/* stdout/stderr share this fd. Opening the path again fails on Horizon once
 * it is already open, so every later line was dropped and the log stopped
 * at "thread pool". Ryujinx allows the second open. */
static void switch_bind_stdio(void)
{
	pthread_mutex_lock(&switch_log_mu);
	switch_log_open_unlocked();
	if (switch_log_fd >= 0) {
		dup2(switch_log_fd, STDOUT_FILENO);
		dup2(switch_log_fd, STDERR_FILENO);
	}
	pthread_mutex_unlock(&switch_log_mu);
	if (switch_log_fd >= 0) {
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
	}
}

static void deinit_egl(void)
{
	if (s_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (s_context != EGL_NO_CONTEXT)
			eglDestroyContext(s_display, s_context);
		if (s_surface != EGL_NO_SURFACE)
			eglDestroySurface(s_display, s_surface);
		eglTerminate(s_display);
	}
	s_display = EGL_NO_DISPLAY;
	s_context = EGL_NO_CONTEXT;
	s_surface = EGL_NO_SURFACE;
	window_ready = false;
}

int platform_boot(struct config *cfg)
{
	/* nxlinkStdio() redirects stderr onto a socket. Zeebx logs through stderr
	   during retro_load_game, and that write blocks forever when nothing is
	   reading the other end. */
	romfsInit();
	mkdir("sdmc:/switch", 0755);
	mkdir(NX_APP_DIR, 0755);
	switch_trace("boot");
	switch_reserve_device_memory();
	switch_thread_pool_init();
	/* The speaker thread has to exist before the game takes the heap.
	 * A thread created later comes back without a stack. */
	switch_audio_boot();
	/* Rust panics print to fd 2. Bind the fd already used by switch_trace.
	   A second open() of trace.log fails on the console. */
	switch_bind_stdio();
	switch_trace("stdio");

	padConfigureInput(MAX_PADS, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pads[0]);
	pad_inited[0] = true;
	for (int i = 1; i < MAX_PADS; i++) {
		padInitialize(&pads[i], HidNpadIdType_No1 + i);
		pad_inited[i] = true;
	}
	switch_trace("pads");

	if (ini_parse(NX_APP_DIR "/config.ini", cfg_handler, cfg) < 0 &&
	    ini_parse("romfs:/config.ini", cfg_handler, cfg) < 0)
		return 0;
	ini_parse(NX_APP_DIR "/options.ini", opt_handler, NULL);
	ini_parse("romfs:/options.ini", opt_handler, NULL);
	switch_trace("config");
	/* Mesa has to come up while the two 64 MiB blocks are still ours.
	 * After the game takes them, eglInitialize does not return and the
	 * panel stays black. */
	create_window(cfg->window_width, cfg->window_height);
	/* Mesa ran on the real heap. From here the reserved blocks may be handed
	 * to the game. */
	switch_reserves_enable();
	return 1;
}

static int write_error_log(const char *msg)
{
	FILE *log;
	int ok = 0;

	mkdir("sdmc:/switch", 0755);
	mkdir(NX_APP_DIR, 0755);
	log = fopen(NX_ERROR_LOG, "w");
	if (!log)
		log = fopen("/switch/rombundler/error.log", "w");
	if (log) {
		if (msg && msg[0]) {
			fputs(msg, log);
			if (msg[strlen(msg) - 1] != '\n')
				fputc('\n', log);
		}
		fflush(log);
		fclose(log);
		ok = 1;
	}
	/* libnx does not persist sdmc writes on fclose/exit without this. */
	fsdevCommitDevice("sdmc");
	return ok;
}

void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
	char buf[512];

	snprintf(buf, sizeof(buf),
		"crash: pc=%p lr=%p far=%p esr=0x%x\n"
		"Native exception (not ROMBundler die()).\n"
		"Atmosphere dump: sdmc:/atmosphere/crash_reports/",
		(void *)(uintptr_t)ctx->pc.x,
		(void *)(uintptr_t)ctx->lr.x,
		(void *)(uintptr_t)ctx->far.x,
		ctx->esr);
	write_error_log(buf);
}

void platform_fatal(const char *msg)
{
	int logged;

	deinit_egl();
	logged = write_error_log(msg);
	consoleInit(NULL);
	printf("ROMBundler error:\n\n%s\n\nPress + to exit.\n", msg);
	if (logged)
		printf("Wrote %s\n", NX_ERROR_LOG);
	else
		printf("Could not write %s\n", NX_ERROR_LOG);
	PadState pad;
	padInitializeDefault(&pad);
	while (appletMainLoop()) {
		padUpdate(&pad);
		if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
			break;
		consoleUpdate(NULL);
	}
	consoleExit(NULL);
	exit(EXIT_FAILURE);
}

void switch_trace(const char *msg)
{
	size_t n;

	if (!msg || !msg[0])
		return;
	pthread_mutex_lock(&switch_log_mu);
	switch_log_open_unlocked();
	if (switch_log_fd >= 0) {
		n = strlen(msg);
		if (write(switch_log_fd, msg, n) == (ssize_t)n)
			write(switch_log_fd, "\n", 1);
		fsdevCommitDevice("sdmc");
	}
	pthread_mutex_unlock(&switch_log_mu);
}

void platform_debug(const char *msg)
{
	if (!msg || !msg[0])
		return;
	/* stderr is the same file. Writing it again commits sdmc while a worker
	 * may already hold that lock, and the process stops here with a black screen. */
	switch_trace(msg);
}

FILE *platform_fopen(const char *path, const char *mode)
{
	FILE *f;

	if (!path || !path[0])
		return NULL;
	f = fopen(path, mode);
	if (f)
		return f;
	if (!strncmp(path, "sdmc:", 5))
		return fopen(path + 5, mode);
	return NULL;
}

const char *platform_srm_path(void)
{
	return NX_APP_DIR "/save.srm";
}

void platform_prepare_core(const char *path)
{
	size_t n;

	if (!path || !path[0])
		return;
	n = strlen(path);
	if ((n >= 4 && (!strcasecmp(path + n - 4, ".nro") || !strcasecmp(path + n - 4, ".dll"))) ||
	    (n >= 3 && !strcasecmp(path + n - 3, ".so")) ||
	    (n >= 6 && !strcasecmp(path + n - 6, ".dylib")))
		die("Switch cannot load '%s' at runtime (RetroArch .nro/.so cores are not supported).\n"
		    "This build has a core linked into the NRO.\n"
		    "Set core = dummy in config.ini, or rebuild with -DROMBUNDLER_CORE_LIBRARY=<core.a>.",
		    path);
#ifdef ROMBUNDLER_DUMMY_CORE
	if (strcasecmp(path, "dummy") != 0)
		die("This NRO is the dummy core (color bars). config.ini core=%s does not load a core.\n"
		    "Rebuild with -DROMBUNDLER_CORE_LIBRARY=/path/to/genesis_plus_gx_libretro.a\n"
		    "and set core = dummy (or any name without .nro).",
		    path);
#endif
}

int platform_gl_enable_texture_2d(void)
{
	return 0;
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
	/* Mesa libglad links entry points; treat as always present after load. */
	return 1;
}

int platform_gles(void)
{
	return 0;
}

int platform_gl_packed_uploads(void)
{
	/* The window is an OpenGL 4.3 core context. 565 and GL_BGRA are not valid there. */
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

void platform_deinit(void)
{
	deinit_egl();
	romfsExit();
}

void platform_poll(void)
{
	if (!appletMainLoop())
		should_close = true;
	for (int i = 0; i < MAX_PADS; i++) {
		if (pad_inited[i])
			padUpdate(&pads[i]);
	}
	u64 keys = padGetButtons(&pads[0]);
	if ((keys & HidNpadButton_Plus) && (keys & HidNpadButton_Minus))
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
}

void platform_swap_buffers(void)
{
	if (s_display != EGL_NO_DISPLAY && s_surface != EGL_NO_SURFACE)
		eglSwapBuffers(s_display, s_surface);
}

void platform_set_swap_interval(int interval)
{
	(void)interval;
	/* A frame that misses the vblank waits for the next one, so a game that
	 * is a little under 60 becomes 30. Present as soon as the frame is ready. */
	if (s_display != EGL_NO_DISPLAY)
		eglSwapInterval(s_display, 0);
}

void platform_get_framebuffer_size(int *width, int *height)
{
	u32 w = 1280, h = 720;

	nwindowGetDimensions(nwindowGetDefault(), &w, &h);
	if (!w || !h) {
		w = 1280;
		h = 720;
	}
	*width = (int)w;
	*height = (int)h;
}

void *_glapi_get_proc_address(const char *name);

void *platform_get_proc_address(const char *name)
{
	void *p;

	if (!name || !name[0])
		return NULL;
	p = (void *)eglGetProcAddress(name);
	if (p)
		return p;
	/* EGL 1.4 does not resolve core GL; mesa glapi does. */
	return _glapi_get_proc_address(name);
}

/* Contiguous blocks the loader asks for after Mesa is up. The general heap
 * then has no 4 MiB hole left (objects), so they are taken while it is empty.
 * Two 64 MiB: guest heap, then a second buffer before the JIT. Two 8 MiB:
 * surfaces and the page table. 4 MiB objects, 1 MiB stack. The 8 MiB GPU
 * transfer memory is separate.
 */
#define SWITCH_NV_TMEM 0x800000u
#define SWITCH_HEAP (64u * 1024u * 1024u)

struct switch_block {
	size_t size;
	void *ptr;
};

static struct switch_block switch_blocks[] = {
	{ 1u * 1024u * 1024u, NULL },
	{ 4u * 1024u * 1024u, NULL },
	{ 8u * 1024u * 1024u, NULL },
	{ 8u * 1024u * 1024u, NULL },
	{ SWITCH_HEAP, NULL },
	{ SWITCH_HEAP, NULL },
};
static int switch_blocks_live;
static int switch_heap_handed;
static void *switch_nv_tmem;

void *__real_malloc(size_t size);
void *__real_calloc(size_t nmemb, size_t size);
int __real_posix_memalign(void **memptr, size_t alignment, size_t size);
void *__real_aligned_alloc(size_t alignment, size_t size);

void switch_reserves_enable(void)
{
	switch_blocks_live = 1;
	switch_trace("reserves live");
}

static void *switch_take_block(size_t size)
{
	size_t i;

	if (!switch_blocks_live || !size)
		return NULL;
	/* File reads and Mesa must not consume the objects/stack/surfaces
	 * blocks. Those exist for the map that follows the guest heap. */
	if (size != SWITCH_HEAP && !switch_heap_handed)
		return NULL;
	for (i = 0; i < sizeof(switch_blocks) / sizeof(switch_blocks[0]); i++) {
		void *p;
		char line[48];

		if (!switch_blocks[i].ptr || switch_blocks[i].size != size)
			continue;
		p = switch_blocks[i].ptr;
		switch_blocks[i].ptr = NULL;
		if (size == SWITCH_HEAP)
			switch_heap_handed = 1;
		snprintf(line, sizeof(line), "handed %llu", (unsigned long long)size);
		switch_trace(line);
		return p;
	}
	return NULL;
}

void *__wrap_malloc(size_t size)
{
	void *p = switch_take_block(size);

	if (p)
		return p;
	return __real_malloc(size);
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
	size_t bytes = 0;
	void *p;

	if (nmemb == 1)
		bytes = size;
	else if (size == 1)
		bytes = nmemb;
	p = switch_take_block(bytes);
	if (p) {
		memset(p, 0, bytes);
		return p;
	}
	return __real_calloc(nmemb, size);
}

int __wrap_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *p;

	if (memptr && alignment && alignment <= 0x1000 &&
	    (alignment & (alignment - 1)) == 0 &&
	    (p = switch_take_block(size))) {
		*memptr = p;
		return 0;
	}
	return __real_posix_memalign(memptr, alignment, size);
}

void *__wrap_aligned_alloc(size_t alignment, size_t size)
{
	void *p;

	if (alignment && alignment <= 0x1000 &&
	    (alignment & (alignment - 1)) == 0 &&
	    (p = switch_take_block(size)))
		return p;
	return __real_aligned_alloc(alignment, size);
}

Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm)
{
	void *buf = switch_nv_tmem;

	if (buf) {
		switch_nv_tmem = NULL;
		*out_size = SWITCH_NV_TMEM;
		return tmemCreateFromMemory(t, buf, SWITCH_NV_TMEM, perm);
	}
	*out_size = SWITCH_NV_TMEM;
	return tmemCreate(t, SWITCH_NV_TMEM, perm);
}

void switch_reserve_device_memory(void)
{
	u64 total = 0, used = 0;
	char line[128];
	AppletType kind;

	switch_nv_tmem = memalign(0x1000, SWITCH_NV_TMEM);
	switch_trace(switch_nv_tmem ? "nv tmem 8388608" : "nv tmem failed");
	for (size_t i = 0; i < sizeof(switch_blocks) / sizeof(switch_blocks[0]); i++) {
		switch_blocks[i].ptr = memalign(0x1000, switch_blocks[i].size);
		if (!switch_blocks[i].ptr)
			break;
	}
	for (size_t i = 0; i < sizeof(switch_blocks) / sizeof(switch_blocks[0]); i++) {
		if (switch_blocks[i].ptr)
			continue;
		svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
		svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
		kind = appletGetAppletType();
		snprintf(line, sizeof(line), "reserve failed %llu mem %llu/%llu applet %u",
			(unsigned long long)switch_blocks[i].size,
			(unsigned long long)used, (unsigned long long)total, (unsigned)kind);
		switch_trace(line);
		if (kind == AppletType_LibraryApplet || kind == AppletType_OverlayApplet)
			die("Sem %llu bytes livres para o jogo (mem %llu/%llu).\nSegure R ao abrir um jogo e lance o ROMBundler por ele.",
				(unsigned long long)switch_blocks[i].size,
				(unsigned long long)used, (unsigned long long)total);
		die("Sem %llu bytes livres para o jogo (mem %llu/%llu, applet %u).",
			(unsigned long long)switch_blocks[i].size,
			(unsigned long long)used, (unsigned long long)total, (unsigned)kind);
	}
	switch_trace("reserves ok");
}

int platform_create_window(int width, int height, const char *title,
	int fullscreen, int hide_cursor, unsigned hw_context_type, int major, int minor)
{
	(void)width;
	(void)height;
	(void)title;
	(void)fullscreen;
	(void)hide_cursor;
	(void)hw_context_type;
	(void)major;
	(void)minor;

	u64 mem_total = 0, mem_used = 0;

	if (s_context != EGL_NO_CONTEXT)
		return 1;

	NWindow *win = nwindowGetDefault();
	/* 720p on the handheld and on the TV. A 1080p surface when the applet
	 * reports docked mode never flips on the real panel; the compositor
	 * scales this one. Ryujinx shows either size. */
	nwindowSetDimensions(win, 1280, 720);

	svcGetInfo(&mem_total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&mem_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);

	switch_trace("egl");
	s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (s_display == EGL_NO_DISPLAY)
		die("eglGetDisplay failed");
	if (!eglInitialize(s_display, NULL, NULL))
		die("eglInitialize failed (egl 0x%x, mem %llu/%llu)",
			eglGetError(),
			(unsigned long long)mem_used, (unsigned long long)mem_total);
	switch_trace("egl ok");
	if (!eglBindAPI(EGL_OPENGL_API))
		die("eglBindAPI failed");

	EGLint fb_attr[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLConfig config;
	EGLint ncfg = 0;
	if (!eglChooseConfig(s_display, fb_attr, &config, 1, &ncfg) || ncfg < 1)
		die("eglChooseConfig failed");

	s_surface = eglCreateWindowSurface(s_display, config, win, NULL);
	if (s_surface == EGL_NO_SURFACE)
		die("eglCreateWindowSurface failed");

	EGLint ctx_attr[] = {
		EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
		EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
		EGL_CONTEXT_MINOR_VERSION_KHR, 3,
		EGL_NONE
	};
	s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctx_attr);
	if (s_context == EGL_NO_CONTEXT)
		die("eglCreateContext failed (need Mesa OpenGL 4.3)");
	if (!eglMakeCurrent(s_display, s_surface, s_surface, s_context))
		die("eglMakeCurrent failed");

	window_ready = true;
	return 1;
}

void platform_destroy_window(void)
{
	deinit_egl();
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
	if (port < 0 || port >= MAX_PADS || !pad_inited[port])
		return 0;
	return padIsConnected(&pads[port]);
}

static u64 pad_buttons(int port)
{
	if (port < 0 || port >= MAX_PADS || !pad_inited[port])
		return 0;
	return padGetButtons(&pads[port]);
}

int platform_gamepad_button(int port, int button)
{
	u64 k = pad_buttons(port);
	switch (button) {
	case RETRO_DEVICE_ID_JOYPAD_B: return !!(k & HidNpadButton_B);
	case RETRO_DEVICE_ID_JOYPAD_Y: return !!(k & HidNpadButton_Y);
	case RETRO_DEVICE_ID_JOYPAD_SELECT: return !!(k & HidNpadButton_Minus);
	case RETRO_DEVICE_ID_JOYPAD_START: return !!(k & HidNpadButton_Plus);
	case RETRO_DEVICE_ID_JOYPAD_UP: return !!(k & HidNpadButton_Up);
	case RETRO_DEVICE_ID_JOYPAD_DOWN: return !!(k & HidNpadButton_Down);
	case RETRO_DEVICE_ID_JOYPAD_LEFT: return !!(k & HidNpadButton_Left);
	case RETRO_DEVICE_ID_JOYPAD_RIGHT: return !!(k & HidNpadButton_Right);
	case RETRO_DEVICE_ID_JOYPAD_A: return !!(k & HidNpadButton_A);
	case RETRO_DEVICE_ID_JOYPAD_X: return !!(k & HidNpadButton_X);
	case RETRO_DEVICE_ID_JOYPAD_L: return !!(k & HidNpadButton_L);
	case RETRO_DEVICE_ID_JOYPAD_R: return !!(k & HidNpadButton_R);
	case RETRO_DEVICE_ID_JOYPAD_L2: return !!(k & HidNpadButton_ZL);
	case RETRO_DEVICE_ID_JOYPAD_R2: return !!(k & HidNpadButton_ZR);
	case RETRO_DEVICE_ID_JOYPAD_L3: return !!(k & HidNpadButton_StickL);
	case RETRO_DEVICE_ID_JOYPAD_R3: return !!(k & HidNpadButton_StickR);
	default: return 0;
	}
}

float platform_gamepad_axis(int port, int axis)
{
	if (port < 0 || port >= MAX_PADS || !pad_inited[port])
		return 0.f;
	HidAnalogStickState l = padGetStickPos(&pads[port], 0);
	HidAnalogStickState r = padGetStickPos(&pads[port], 1);
	const float scale = 1.f / 32767.f;
	switch (axis) {
	case 0: return l.x * scale;
	case 1: return -(l.y * scale);
	case 2: return r.x * scale;
	case 3: return -(r.y * scale);
	default: return 0.f;
	}
}
