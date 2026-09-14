#include <string.h>
#include <stdint.h>
#include "libretro.h"

#define WIDTH 320
#define HEIGHT 240

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static uint16_t frame[WIDTH * HEIGHT];
static unsigned tick;

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void) {}
void retro_deinit(void) {}

unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = "dummy";
	info->library_version = "1";
	info->valid_extensions = "bin";
	info->need_fullpath = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = WIDTH;
	info->geometry.base_height = HEIGHT;
	info->geometry.max_width = WIDTH;
	info->geometry.max_height = HEIGHT;
	info->geometry.aspect_ratio = 4.0f / 3.0f;
	info->timing.fps = 60.0;
	info->timing.sample_rate = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	(void)port;
	(void)device;
}

void retro_reset(void)
{
	tick = 0;
}

void retro_run(void)
{
	if (input_poll_cb)
		input_poll_cb();
	tick++;
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			uint16_t r = (uint16_t)((x + tick) & 31);
			uint16_t g = (uint16_t)((y + tick / 2) & 63);
			uint16_t b = (uint16_t)((x / 2 + y / 2) & 31);
			frame[y * WIDTH + x] = (r << 11) | (g << 5) | b;
		}
	}
	if (video_cb)
		video_cb(frame, WIDTH, HEIGHT, WIDTH * 2);
	int16_t silence[1600 * 2];
	memset(silence, 0, sizeof(silence));
	if (audio_batch_cb)
		audio_batch_cb(silence, 1600);
	(void)audio_cb;
	(void)input_state_cb;
}

size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned a, bool b, const char *c) { (void)a; (void)b; (void)c; }

bool retro_load_game(const struct retro_game_info *game)
{
	(void)game;
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
	if (environ_cb)
		environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
	return true;
}

bool retro_load_game_special(unsigned t, const struct retro_game_info *info, size_t num)
{
	(void)t;
	(void)info;
	(void)num;
	return false;
}

void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
