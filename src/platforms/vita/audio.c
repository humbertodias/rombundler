#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <psp2/audioout.h>

#include "audio.h"
#include "utils.h"

#define BUFSIZE (1024 * 8)
#define OUT_RATE 48000
#define OUT_SAMPLES 512

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

static struct {
	int rate;
	int port;
	size_t tmpbuf_ptr;
	uint8_t tmpbuf[BUFSIZE];
	int16_t out[OUT_SAMPLES * 2];
	bool ready;
} vita_audio;

static void resample_stereo16(const int16_t *in, size_t in_frames, int in_rate,
	int16_t *out, size_t out_frames)
{
	if (in_rate == OUT_RATE) {
		memcpy(out, in, out_frames * 4);
		return;
	}
	for (size_t i = 0; i < out_frames; i++) {
		size_t src = (size_t)((i * (uint64_t)in_rate) / OUT_RATE);
		if (src >= in_frames)
			src = in_frames - 1;
		out[i * 2] = in[src * 2];
		out[i * 2 + 1] = in[src * 2 + 1];
	}
}

void rb_audio_init(int rate)
{
	int vols[2];

	memset(&vita_audio, 0, sizeof(vita_audio));
	vita_audio.rate = rate > 0 ? rate : OUT_RATE;
	vita_audio.port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, OUT_SAMPLES,
		OUT_RATE, SCE_AUDIO_OUT_MODE_STEREO);
	if (vita_audio.port < 0)
		return;
	vols[0] = SCE_AUDIO_VOLUME_0DB;
	vols[1] = SCE_AUDIO_VOLUME_0DB;
	sceAudioOutSetVolume(vita_audio.port,
		SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vols);
	vita_audio.ready = true;
}

static size_t fill_internal_buf(const void *buf, size_t size)
{
	size_t read_size = MIN(BUFSIZE - vita_audio.tmpbuf_ptr, size);
	memcpy(vita_audio.tmpbuf + vita_audio.tmpbuf_ptr, buf, read_size);
	vita_audio.tmpbuf_ptr += read_size;
	return read_size;
}

static size_t audio_write(const void *buf_, unsigned size)
{
	if (!vita_audio.ready || !buf_ || !size)
		return size;

	const uint8_t *buf = (const uint8_t *)buf_;

	while (size) {
		size_t rc = fill_internal_buf(buf, size);
		buf += rc;
		size -= (unsigned)rc;
		if (vita_audio.tmpbuf_ptr != BUFSIZE)
			break;

		size_t in_frames = BUFSIZE / 4;
		resample_stereo16((const int16_t *)vita_audio.tmpbuf, in_frames, vita_audio.rate,
			vita_audio.out, OUT_SAMPLES);
		sceAudioOutOutput(vita_audio.port, vita_audio.out);
		vita_audio.tmpbuf_ptr = 0;
	}

	return (size_t)(buf_ ? 1 : 0) + (size_t)size;
}

void rb_audio_deinit(void)
{
	if (!vita_audio.ready)
		return;
	sceAudioOutReleasePort(vita_audio.port);
	memset(&vita_audio, 0, sizeof(vita_audio));
}

void rb_audio_sample(int16_t left, int16_t right)
{
	int16_t buf[2] = { left, right };
	audio_write(buf, 4);
}

size_t rb_audio_sample_batch(const int16_t *data, size_t frames)
{
	audio_write(data, (unsigned)(frames * 4));
	return frames;
}
