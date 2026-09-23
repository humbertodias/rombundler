#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include <glad/glad.h>

#include "config.h"
#include "core.h"
#include "audio.h"
#include "video.h"
#include "input.h"
#include "srm.h"
#include "utils.h"
#include "platform.h"

config g_cfg;

static unsigned g_frame;

static void app_step(void)
{
	platform_poll();
	input_poll();
	core_run();
	video_render();
	platform_swap_buffers();
	g_frame++;
	if (g_frame % 600 == 0)
		srm_save();
}

static void app_cleanup(void)
{
	srm_save();
	core_unload();
	rb_audio_deinit();
	video_deinit();
	platform_deinit();
}

static void app_loaded(void)
{
	core_load(g_cfg.core);
	core_load_game(g_cfg.rom);

	srm_load();

	platform_set_swap_interval(g_cfg.swap_interval);
	platform_enter_loop(app_step, app_cleanup);
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	cfg_defaults(&g_cfg);
	if (!platform_boot(&g_cfg))
		die("Could not parse config.ini");

#ifdef ROMBUNDLER_WASM_DYNAMIC
	platform_open_core_then(g_cfg.core, app_loaded);
#else
	app_loaded();
#endif
	return 0;
}
