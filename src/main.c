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

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	cfg_defaults(&g_cfg);
	if (!platform_boot(&g_cfg))
		die("Could not parse config.ini");

	core_load(g_cfg.core);
	core_load_game(g_cfg.rom);

	srm_load();

	platform_set_swap_interval(g_cfg.swap_interval);

	unsigned frame = 0;
	while (!platform_should_close()) {
		platform_poll();
		input_poll();
		core_run();
		video_render();
		platform_swap_buffers();
		frame++;
		if (frame % 600 == 0)
			srm_save();
	}

	srm_save();
	core_unload();
	rb_audio_deinit();
	video_deinit();

	platform_deinit();
	return 0;
}
