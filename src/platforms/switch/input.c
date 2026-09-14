#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "input.h"
#include "platform.h"
#include "config.h"
#include "libretro.h"

extern config g_cfg;

#define MAX_PLAYERS 5
static int16_t state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3+1] = { 0 };
static int16_t analog_state[MAX_PLAYERS][2][2] = { 0 };
static retro_keyboard_event_t key_event = NULL;

static int16_t floatToAnalog(float v)
{
	return v * 32767.0f;
}

void input_poll(void)
{
	int i;
	int port;

	(void)key_event;
	for (port = 0; port < MAX_PLAYERS; port++) {
		memset(state[port], 0, sizeof(state[port]));
		if (!platform_gamepad_present(port) && port != 0)
			continue;
		for (i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R3; i++)
			state[port][i] = platform_gamepad_button(port, i);

		analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] =
			floatToAnalog(platform_gamepad_axis(port, 0));
		analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] =
			floatToAnalog(platform_gamepad_axis(port, 1));
		analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] =
			floatToAnalog(platform_gamepad_axis(port, 2));
		analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] =
			floatToAnalog(platform_gamepad_axis(port, 3));

		if (g_cfg.map_analog_to_dpad) {
			state[port][RETRO_DEVICE_ID_JOYPAD_LEFT] |= platform_gamepad_axis(port, 0) < -0.5f;
			state[port][RETRO_DEVICE_ID_JOYPAD_RIGHT] |= platform_gamepad_axis(port, 0) > 0.5f;
			state[port][RETRO_DEVICE_ID_JOYPAD_UP] |= platform_gamepad_axis(port, 1) < -0.5f;
			state[port][RETRO_DEVICE_ID_JOYPAD_DOWN] |= platform_gamepad_axis(port, 1) > 0.5f;
		}
	}
}

void input_set_keyboard_callback(retro_keyboard_event_t e)
{
	key_event = e;
}

int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (port >= MAX_PLAYERS)
		return 0;
	if (device == RETRO_DEVICE_JOYPAD)
		return state[port][id];
	if (device == RETRO_DEVICE_ANALOG)
		return analog_state[port][index][id];
	return 0;
}
