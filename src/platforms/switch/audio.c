#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <switch.h>

#include "audio.h"
#include "utils.h"

#define BUFSIZE (1024 * 8)
#define NUMBUFFERS 4
#define OUT_RATE 48000

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

typedef struct {
	AudioOutBuffer buf;
	void *data;
} nx_abuf;

static struct {
	int rate;
	size_t tmpbuf_ptr;
	uint8_t tmpbuf[BUFSIZE];
	nx_abuf slots[NUMBUFFERS];
	u32 slot_bytes;
	int next;
	bool ready;
} nx_audio;

static u32 align_0x1000(u32 v)
{
	return (v + 0xfff) & ~0xfffu;
}

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
	memset(&nx_audio, 0, sizeof(nx_audio));
	nx_audio.rate = rate > 0 ? rate : OUT_RATE;

	Result rc = audoutInitialize();
	if (R_FAILED(rc))
		die("audoutInitialize failed");
	rc = audoutStartAudioOut();
	if (R_FAILED(rc))
		die("audoutStartAudioOut failed");

	nx_audio.slot_bytes = align_0x1000(BUFSIZE * 2);
	for (int i = 0; i < NUMBUFFERS; i++) {
		nx_audio.slots[i].data = memalign(0x1000, nx_audio.slot_bytes);
		if (!nx_audio.slots[i].data)
			die("Could not init audio buffers");
		memset(nx_audio.slots[i].data, 0, nx_audio.slot_bytes);
		nx_audio.slots[i].buf.next = NULL;
		nx_audio.slots[i].buf.buffer = nx_audio.slots[i].data;
		nx_audio.slots[i].buf.buffer_size = nx_audio.slot_bytes;
		nx_audio.slots[i].buf.data_size = 0;
		nx_audio.slots[i].buf.data_offset = 0;
	}
	nx_audio.ready = true;
}

static size_t fill_internal_buf(const void *buf, size_t size)
{
	size_t read_size = MIN(BUFSIZE - nx_audio.tmpbuf_ptr, size);
	memcpy(nx_audio.tmpbuf + nx_audio.tmpbuf_ptr, buf, read_size);
	nx_audio.tmpbuf_ptr += read_size;
	return read_size;
}

size_t audio_write(const void *buf_, unsigned size)
{
	if (!nx_audio.ready || !buf_ || !size)
		return size;

	const uint8_t *buf = (const uint8_t *)buf_;
	size_t written = 0;

	while (size) {
		size_t rc = fill_internal_buf(buf, size);
		written += rc;
		buf += rc;
		size -= rc;
		if (nx_audio.tmpbuf_ptr != BUFSIZE)
			break;

		size_t in_frames = BUFSIZE / 4;
		size_t out_frames = (in_frames * (size_t)OUT_RATE) / (size_t)nx_audio.rate;
		if (out_frames < 1)
			out_frames = 1;
		u32 data_size = (u32)(out_frames * 4);
		if (data_size > nx_audio.slot_bytes)
			data_size = nx_audio.slot_bytes;

		nx_abuf *slot = &nx_audio.slots[nx_audio.next];
		resample_stereo16((const int16_t *)nx_audio.tmpbuf, in_frames, nx_audio.rate,
			(int16_t *)slot->data, data_size / 4);
		slot->buf.data_size = data_size;

		AudioOutBuffer *released = NULL;
		audoutPlayBuffer(&slot->buf, &released);
		nx_audio.next = (nx_audio.next + 1) % NUMBUFFERS;
		nx_audio.tmpbuf_ptr = 0;
	}

	return written;
}

void rb_audio_deinit(void)
{
	if (!nx_audio.ready)
		return;
	audoutStopAudioOut();
	audoutExit();
	for (int i = 0; i < NUMBUFFERS; i++)
		free(nx_audio.slots[i].data);
	memset(&nx_audio, 0, sizeof(nx_audio));
}

void rb_audio_sample(int16_t left, int16_t right)
{
	int16_t buf[2] = { left, right };
	audio_write(buf, 4);
}

size_t rb_audio_sample_batch(const int16_t *data, size_t frames)
{
	return audio_write(data, frames * 4);
}
