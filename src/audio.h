#ifndef ROMBUNDLER_AUDIO_H
#define ROMBUNDLER_AUDIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void rb_audio_init(int frequency);
void rb_audio_deinit(void);
void rb_audio_sample(int16_t left, int16_t right);
size_t rb_audio_sample_batch(const int16_t *data, size_t frames);

#endif
