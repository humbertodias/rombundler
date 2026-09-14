#ifndef ROMBUNDLER_CORE_LOAD_H
#define ROMBUNDLER_CORE_LOAD_H

#include <stdint.h>

void retro_init(void);
void retro_deinit(void);
unsigned retro_api_version(void);
void retro_get_system_info(struct retro_system_info *info);
void retro_get_system_av_info(struct retro_system_av_info *info);
void retro_set_controller_port_device(unsigned port, unsigned device);
void retro_reset(void);
void retro_run(void);
bool retro_load_game(const struct retro_game_info *game);
void retro_unload_game(void);
void *retro_get_memory_data(unsigned id);
size_t retro_get_memory_size(unsigned id);
void retro_set_environment(retro_environment_t cb);
void retro_set_video_refresh(retro_video_refresh_t cb);
void retro_set_input_poll(retro_input_poll_t cb);
void retro_set_input_state(retro_input_state_t cb);
void retro_set_audio_sample(retro_audio_sample_t cb);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

#define load_lib(L) ((void *)(uintptr_t)1)
#define load_sym(V, S) do { V = S; } while (0)
#define close_lib(L) ((void)(L))
#define load_retro_sym(S) load_sym(core.S, S)

#endif
