#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <switch.h>

#include "audio.h"
#include "utils.h"

/* The device plays 48000 Hz. The core delivers its own rate, and this
 * resamples up to the device. Each block is 1024
 * stereo frames, 4096 bytes, a multiple of the 0x1000 alignment audout
 * requires. data_size stays equal to buffer_size: a short payload inside a
 * larger buffer is played as the whole buffer, and the extra silence
 * stretches the music. */
#define OUT_RATE 48000
#define OUT_FRAMES 1024
#define OUT_BYTES (OUT_FRAMES * 4)
#define NUMBUFFERS 4
#define IN_CAP 8192

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif

typedef struct {
	AudioOutBuffer buf;
	void *data;
} nx_abuf;

static struct {
	int rate;
	int16_t in[IN_CAP * 2];
	size_t in_read;
	size_t in_frames;
	uint64_t in_base;
	uint64_t out_pos;
	uint32_t acc;
	nx_abuf slots[NUMBUFFERS];
	int next;
	int queued;
	bool ready;
} nx_audio;

/* Resample and audout run on their own core. Doing that on the game thread
 * made every note wait for a service call, and the music fell behind the
 * picture. The thread is started at boot: after the game is loaded there is
 * no stack left to create one. */
static pthread_mutex_t audio_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t audio_cv = PTHREAD_COND_INITIALIZER;
static Thread audio_thr;
static int audio_threaded;
static int audio_stop;

void switch_bind_thread_pointer(void);
void switch_trace(const char *msg);

static int ready_to_emit(void)
{
	uint64_t last;

	if (!nx_audio.in_frames)
		return 0;
	last = (nx_audio.out_pos + OUT_FRAMES - 1) * (uint64_t)nx_audio.rate / OUT_RATE;
	if (last < nx_audio.in_base)
		return 1;
	return nx_audio.in_frames > (size_t)(last - nx_audio.in_base);
}

static void reap(void)
{
	/* One call returns every buffer the speaker has finished. Waiting here
	 * froze the game until that audio had played, so the next notes started
	 * late and the music dragged. */
	for (int n = 0; n < NUMBUFFERS; n++) {
		AudioOutBuffer *released = NULL;
		u32 count = 0;

		if (R_FAILED(audoutGetReleasedAudioOutBuffer(&released, &count)) || !count)
			break;
		nx_audio.queued -= (int)count;
		if (nx_audio.queued < 0)
			nx_audio.queued = 0;
	}
}

/* Fills the next slot from the ring. Caller holds the ring lock. No audout
 * calls: those block, and the game thread must not wait on them. */
static int fill_block(void)
{
	nx_abuf *slot;
	int16_t *dst;
	size_t idx;
	size_t drop;
	uint32_t acc;

	if (nx_audio.queued >= NUMBUFFERS || !ready_to_emit())
		return 0;

	slot = &nx_audio.slots[nx_audio.next];
	dst = slot->data;
	idx = nx_audio.in_read;
	acc = nx_audio.acc;

	for (size_t i = 0; i < OUT_FRAMES; i++) {
		dst[i * 2] = nx_audio.in[idx * 2];
		dst[i * 2 + 1] = nx_audio.in[idx * 2 + 1];
		acc += (uint32_t)nx_audio.rate;
		if (acc >= OUT_RATE) {
			idx++;
			if (idx >= IN_CAP)
				idx = 0;
			acc -= OUT_RATE;
		}
	}

	drop = idx >= nx_audio.in_read
		? idx - nx_audio.in_read
		: IN_CAP - nx_audio.in_read + idx;
	if (drop > nx_audio.in_frames)
		drop = nx_audio.in_frames;
	nx_audio.in_read = idx;
	nx_audio.in_frames -= drop;
	nx_audio.in_base += drop;
	nx_audio.acc = acc;
	nx_audio.out_pos += OUT_FRAMES;
	return 1;
}

static int submit_block(void)
{
	nx_abuf *slot = &nx_audio.slots[nx_audio.next];

	slot->buf.data_size = OUT_BYTES;
	if (R_FAILED(audoutAppendAudioOutBuffer(&slot->buf)))
		return 0;
	nx_audio.queued++;
	nx_audio.next = (nx_audio.next + 1) % NUMBUFFERS;
	return 1;
}

static int emit_block(void)
{
	reap();
	if (!fill_block())
		return 0;
	return submit_block();
}

static void audio_lock(void)
{
	if (audio_threaded)
		pthread_mutex_lock(&audio_mu);
}

static void audio_unlock(void)
{
	if (audio_threaded)
		pthread_mutex_unlock(&audio_mu);
}

static size_t audio_write(const int16_t *samples, size_t frames)
{
	size_t accepted = 0;

	if (!nx_audio.ready || !samples || !frames)
		return frames;

	audio_lock();
	while (frames) {
		size_t space = IN_CAP - nx_audio.in_frames;
		size_t n;

		if (!space) {
			if (audio_threaded || !ready_to_emit() || !emit_block())
				break;
			continue;
		}
		n = frames < space ? frames : space;
		{
			size_t at = nx_audio.in_read + nx_audio.in_frames;
			size_t first;

			if (at >= IN_CAP)
				at -= IN_CAP;
			first = n;
			if (at + first > IN_CAP)
				first = IN_CAP - at;
			memcpy(nx_audio.in + at * 2, samples, first * 4);
			if (n > first)
				memcpy(nx_audio.in, samples + first * 2, (n - first) * 4);
		}
		nx_audio.in_frames += n;
		samples += n * 2;
		frames -= n;
		accepted += n;
		if (audio_threaded)
			continue;
		while (ready_to_emit()) {
			if (!emit_block())
				break;
		}
	}
	if (audio_threaded && accepted)
		pthread_cond_signal(&audio_cv);
	audio_unlock();
	return accepted;
}

static int audio_open_device(void)
{
	int i;
	Result rc = audoutInitialize();

	if (R_FAILED(rc))
		return 0;
	rc = audoutStartAudioOut();
	if (R_FAILED(rc)) {
		audoutExit();
		return 0;
	}
	for (i = 0; i < NUMBUFFERS; i++) {
		nx_audio.slots[i].data = memalign(0x1000, OUT_BYTES);
		if (!nx_audio.slots[i].data) {
			audoutStopAudioOut();
			audoutExit();
			return 0;
		}
		memset(nx_audio.slots[i].data, 0, OUT_BYTES);
		nx_audio.slots[i].buf.next = NULL;
		nx_audio.slots[i].buf.buffer = nx_audio.slots[i].data;
		nx_audio.slots[i].buf.buffer_size = OUT_BYTES;
		nx_audio.slots[i].buf.data_size = OUT_BYTES;
		nx_audio.slots[i].buf.data_offset = 0;
	}
	return 1;
}

static void audio_close_device(void)
{
	int i;

	audoutStopAudioOut();
	audoutExit();
	for (i = 0; i < NUMBUFFERS; i++)
		free(nx_audio.slots[i].data);
}

static void audio_main(void *arg)
{
	(void)arg;
	switch_bind_thread_pointer();
	if (!audio_open_device()) {
		pthread_mutex_lock(&audio_mu);
		audio_stop = 1;
		pthread_cond_broadcast(&audio_cv);
		pthread_mutex_unlock(&audio_mu);
		switch_trace("audio device failed");
		return;
	}
	pthread_mutex_lock(&audio_mu);
	nx_audio.rate = 44100;
	nx_audio.ready = true;
	pthread_cond_broadcast(&audio_cv);
	for (;;) {
		int filled;

		pthread_mutex_unlock(&audio_mu);
		reap();
		pthread_mutex_lock(&audio_mu);
		while (!audio_stop && !ready_to_emit())
			pthread_cond_wait(&audio_cv, &audio_mu);
		if (audio_stop && !ready_to_emit())
			break;
		if (nx_audio.queued >= NUMBUFFERS) {
			AudioOutBuffer *released = NULL;
			u32 count = 0;

			/* The speaker still has every slot. Wait without the ring
			 * lock so the game can keep handing samples over. */
			pthread_mutex_unlock(&audio_mu);
			if (R_SUCCEEDED(audoutWaitPlayFinish(&released, &count, UINT64_MAX)) && count) {
				if ((int)count > nx_audio.queued)
					nx_audio.queued = 0;
				else
					nx_audio.queued -= (int)count;
			}
			pthread_mutex_lock(&audio_mu);
			continue;
		}
		filled = fill_block();
		pthread_mutex_unlock(&audio_mu);
		if (filled)
			submit_block();
		pthread_mutex_lock(&audio_mu);
	}
	nx_audio.ready = false;
	pthread_mutex_unlock(&audio_mu);
	audio_close_device();
}

void switch_audio_boot(void)
{
	u32 game;
	int core = 2;
	Result rc;

	if (audio_threaded)
		return;
	game = svcGetCurrentProcessorNumber();
	if (game == 2)
		core = 1;
	/* Slightly above the main thread so a full speaker queue is drained
	 * without waiting for the next game frame. */
	rc = threadCreate(&audio_thr, audio_main, NULL, NULL, 128 * 1024, 0x2B, core);
	if (R_FAILED(rc)) {
		switch_trace("audio thread failed");
		return;
	}
	rc = threadStart(&audio_thr);
	if (R_FAILED(rc)) {
		threadClose(&audio_thr);
		switch_trace("audio thread failed");
		return;
	}
	audio_threaded = 1;
	switch_trace("audio thread");
}

void rb_audio_init(int rate)
{
	if (audio_threaded) {
		pthread_mutex_lock(&audio_mu);
		while (!nx_audio.ready && !audio_stop)
			pthread_cond_wait(&audio_cv, &audio_mu);
		if (!nx_audio.ready) {
			pthread_mutex_unlock(&audio_mu);
			die("audoutInitialize failed");
		}
		nx_audio.rate = rate > 0 ? rate : OUT_RATE;
		pthread_mutex_unlock(&audio_mu);
		return;
	}

	memset(&nx_audio, 0, sizeof(nx_audio));
	nx_audio.rate = rate > 0 ? rate : OUT_RATE;
	if (!audio_open_device())
		die("audoutInitialize failed");
	nx_audio.ready = true;
}

void rb_audio_deinit(void)
{
	if (audio_threaded) {
		pthread_mutex_lock(&audio_mu);
		audio_stop = 1;
		pthread_cond_broadcast(&audio_cv);
		pthread_mutex_unlock(&audio_mu);
		threadWaitForExit(&audio_thr);
		threadClose(&audio_thr);
		audio_threaded = 0;
		memset(&nx_audio, 0, sizeof(nx_audio));
		return;
	}
	if (!nx_audio.ready)
		return;
	audio_close_device();
	memset(&nx_audio, 0, sizeof(nx_audio));
}

void rb_audio_sample(int16_t left, int16_t right)
{
	int16_t buf[2] = { left, right };
	audio_write(buf, 1);
}

size_t rb_audio_sample_batch(const int16_t *data, size_t frames)
{
	return audio_write(data, frames);
}
