#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "input.h"
#include "config.h"
#include "libretro.h"

extern config g_cfg;

struct keymap {
	unsigned k;  // GLFW key
	unsigned rk; // libretro key
};

struct keymap kbd2joy_binds[] = {
	{ GLFW_KEY_X, RETRO_DEVICE_ID_JOYPAD_A },
	{ GLFW_KEY_Z, RETRO_DEVICE_ID_JOYPAD_B },
	{ GLFW_KEY_A, RETRO_DEVICE_ID_JOYPAD_Y },
	{ GLFW_KEY_S, RETRO_DEVICE_ID_JOYPAD_X },
	{ GLFW_KEY_UP, RETRO_DEVICE_ID_JOYPAD_UP },
	{ GLFW_KEY_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ GLFW_KEY_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ GLFW_KEY_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ GLFW_KEY_ENTER, RETRO_DEVICE_ID_JOYPAD_START },
	{ GLFW_KEY_RIGHT_SHIFT, RETRO_DEVICE_ID_JOYPAD_SELECT },

	{ 0, 0 }
};

struct keymap kbd_binds[] = {
	{ GLFW_KEY_BACKSPACE, RETROK_BACKSPACE },
	{ GLFW_KEY_TAB, RETROK_TAB },
	// { GLFW_KEY_CLEAR, RETROK_CLEAR },
	{ GLFW_KEY_ENTER, RETROK_RETURN },
	{ GLFW_KEY_PAUSE, RETROK_PAUSE },
	{ GLFW_KEY_ESCAPE, RETROK_ESCAPE },
	{ GLFW_KEY_SPACE, RETROK_SPACE },
	// { GLFW_KEY_EXCLAIM, RETROK_EXCLAIM },
	// { GLFW_KEY_QUOTEDBL, RETROK_QUOTEDBL },
	// { GLFW_KEY_HASH, RETROK_HASH },
	// { GLFW_KEY_DOLLAR, RETROK_DOLLAR },
	// { GLFW_KEY_AMPERSAND, RETROK_AMPERSAND },
	// { GLFW_KEY_QUOTE, RETROK_QUOTE },
	// { GLFW_KEY_LEFTPAREN, RETROK_LEFTPAREN },
	// { GLFW_KEY_RIGHTPAREN, RETROK_RIGHTPAREN },
	// { GLFW_KEY_ASTERISK , RETROK_ASTERISK  },
	// { GLFW_KEY_PLUS, RETROK_PLUS },
	{ GLFW_KEY_COMMA, RETROK_COMMA },
	{ GLFW_KEY_MINUS, RETROK_MINUS },
	{ GLFW_KEY_PERIOD, RETROK_PERIOD },
	{ GLFW_KEY_SLASH, RETROK_SLASH },
	{ GLFW_KEY_0, RETROK_0 },
	{ GLFW_KEY_1, RETROK_1 },
	{ GLFW_KEY_2, RETROK_2 },
	{ GLFW_KEY_3, RETROK_3 },
	{ GLFW_KEY_4, RETROK_4 },
	{ GLFW_KEY_5, RETROK_5 },
	{ GLFW_KEY_6, RETROK_6 },
	{ GLFW_KEY_7, RETROK_7 },
	{ GLFW_KEY_8, RETROK_8 },
	{ GLFW_KEY_9, RETROK_9 },
	{ GLFW_KEY_COMMA, RETROK_COLON },
	{ GLFW_KEY_SEMICOLON, RETROK_SEMICOLON },
	{ GLFW_KEY_MINUS, RETROK_LESS },
	{ GLFW_KEY_EQUAL, RETROK_EQUALS },
	// { GLFW_KEY_GREATER, RETROK_GREATER },
	// { GLFW_KEY_QUESTION , RETROK_QUESTION },
	// { GLFW_KEY_AT, RETROK_AT },
	{ GLFW_KEY_LEFT_BRACKET, RETROK_LEFTBRACKET },
	{ GLFW_KEY_BACKSLASH, RETROK_BACKSLASH },
	{ GLFW_KEY_RIGHT_BRACKET, RETROK_RIGHTBRACKET },
	// { GLFW_KEY_CARET, RETROK_CARET },
	// { GLFW_KEY_UNDERSCORE, RETROK_UNDERSCORE },
	{ GLFW_KEY_GRAVE_ACCENT, RETROK_BACKQUOTE },
	{ GLFW_KEY_A, RETROK_a },
	{ GLFW_KEY_B, RETROK_b },
	{ GLFW_KEY_C, RETROK_c },
	{ GLFW_KEY_D, RETROK_d },
	{ GLFW_KEY_E, RETROK_e },
	{ GLFW_KEY_F, RETROK_f },
	{ GLFW_KEY_G, RETROK_g },
	{ GLFW_KEY_H, RETROK_h },
	{ GLFW_KEY_I, RETROK_i },
	{ GLFW_KEY_J, RETROK_j },
	{ GLFW_KEY_K, RETROK_k },
	{ GLFW_KEY_L, RETROK_l },
	{ GLFW_KEY_M, RETROK_m },
	{ GLFW_KEY_N, RETROK_n },
	{ GLFW_KEY_O, RETROK_o },
	{ GLFW_KEY_P, RETROK_p },
	{ GLFW_KEY_Q, RETROK_q },
	{ GLFW_KEY_R, RETROK_r },
	{ GLFW_KEY_S, RETROK_s },
	{ GLFW_KEY_T, RETROK_t },
	{ GLFW_KEY_U, RETROK_u },
	{ GLFW_KEY_V, RETROK_v },
	{ GLFW_KEY_W, RETROK_w },
	{ GLFW_KEY_X, RETROK_x },
	{ GLFW_KEY_Y, RETROK_y },
	{ GLFW_KEY_Z, RETROK_z },
	{ GLFW_KEY_LEFT_BRACKET, RETROK_LEFTBRACE },
	// { GLFW_KEY_BAR, RETROK_BAR },
	{ GLFW_KEY_RIGHT_BRACKET, RETROK_RIGHTBRACE },
    // { GLFW_KEY_TILDE, RETROK_TILDE },
	{ GLFW_KEY_DELETE, RETROK_DELETE },

	{ GLFW_KEY_KP_0, RETROK_KP0 },
	{ GLFW_KEY_KP_1, RETROK_KP1 },
	{ GLFW_KEY_KP_2, RETROK_KP2 },
	{ GLFW_KEY_KP_3, RETROK_KP3 },
	{ GLFW_KEY_KP_4, RETROK_KP4 },
	{ GLFW_KEY_KP_5, RETROK_KP5 },
	{ GLFW_KEY_KP_6, RETROK_KP6 },
	{ GLFW_KEY_KP_7, RETROK_KP7 },
	{ GLFW_KEY_KP_8, RETROK_KP8 },
	{ GLFW_KEY_KP_9, RETROK_KP9 },
	{ GLFW_KEY_KP_DECIMAL, RETROK_KP_PERIOD },
	{ GLFW_KEY_KP_DIVIDE, RETROK_KP_DIVIDE },
	{ GLFW_KEY_KP_MULTIPLY, RETROK_KP_MULTIPLY },
	{ GLFW_KEY_KP_SUBTRACT, RETROK_KP_MINUS },
	{ GLFW_KEY_KP_ADD, RETROK_KP_PLUS },
	{ GLFW_KEY_KP_ENTER, RETROK_KP_ENTER },
	{ GLFW_KEY_KP_EQUAL, RETROK_KP_EQUALS },

	{ GLFW_KEY_UP, RETROK_UP },
	{ GLFW_KEY_DOWN, RETROK_DOWN },
	{ GLFW_KEY_RIGHT, RETROK_RIGHT },
	{ GLFW_KEY_LEFT, RETROK_LEFT },
	{ GLFW_KEY_INSERT, RETROK_INSERT },
	{ GLFW_KEY_HOME, RETROK_HOME },
	{ GLFW_KEY_END, RETROK_END },
	{ GLFW_KEY_PAGE_UP, RETROK_PAGEUP },
	{ GLFW_KEY_PAGE_DOWN, RETROK_PAGEDOWN },

	{ GLFW_KEY_F1, RETROK_F1 },
	{ GLFW_KEY_F2, RETROK_F2 },
	{ GLFW_KEY_F3, RETROK_F3 },
	{ GLFW_KEY_F4, RETROK_F4 },
	{ GLFW_KEY_F5, RETROK_F5 },
	{ GLFW_KEY_F6, RETROK_F6 },
	{ GLFW_KEY_F7, RETROK_F7 },
	{ GLFW_KEY_F8, RETROK_F8 },
	{ GLFW_KEY_F9, RETROK_F9 },
	{ GLFW_KEY_F10, RETROK_F10 },
	{ GLFW_KEY_F11, RETROK_F11 },
	{ GLFW_KEY_F12, RETROK_F12 },
	{ GLFW_KEY_F13, RETROK_F13 },
	{ GLFW_KEY_F14, RETROK_F14 },
	{ GLFW_KEY_F15, RETROK_F15 },

	{ GLFW_KEY_NUM_LOCK, RETROK_NUMLOCK },
	{ GLFW_KEY_CAPS_LOCK, RETROK_CAPSLOCK },
	{ GLFW_KEY_SCROLL_LOCK, RETROK_SCROLLOCK },
	{ GLFW_KEY_RIGHT_SHIFT, RETROK_RSHIFT },
	{ GLFW_KEY_LEFT_SHIFT, RETROK_LSHIFT },
	{ GLFW_KEY_RIGHT_CONTROL, RETROK_RCTRL },
	{ GLFW_KEY_LEFT_CONTROL, RETROK_LCTRL },
	{ GLFW_KEY_RIGHT_ALT, RETROK_RALT },
	{ GLFW_KEY_LEFT_ALT, RETROK_LALT },
	// { GLFW_KEY_RIGHT_META, RETROK_RMETA },
	// { GLFW_KEY_LEFT_META, RETROK_LMETA },
	{ GLFW_KEY_LEFT_SUPER, RETROK_LSUPER },
	{ GLFW_KEY_RIGHT_SUPER, RETROK_RSUPER },
	// { GLFW_KEY_MODE, RETROK_MODE },
	// { GLFW_KEY_COMPOSE, RETROK_COMPOSE },

	// { GLFW_KEY_HELP, RETROK_HELP },
	{ GLFW_KEY_PRINT_SCREEN, RETROK_PRINT },
	// { GLFW_KEY_SYSREQ, RETROK_SYSREQ },
	// { GLFW_KEY_BREAK, RETROK_BREAK },
	{ GLFW_KEY_MENU, RETROK_MENU },
	// { GLFW_KEY_POWER, RETROK_POWER },
	// { GLFW_KEY_EURO, RETROK_EURO },
	// { GLFW_KEY_UNDO, RETROK_UNDO },
	// { GLFW_KEY_OEM_102, RETROK_OEM_102 },

	{ 0, 0 }
};

struct keymap joy_binds[] = {
	{ GLFW_GAMEPAD_BUTTON_B, RETRO_DEVICE_ID_JOYPAD_A },
	{ GLFW_GAMEPAD_BUTTON_A, RETRO_DEVICE_ID_JOYPAD_B },
	{ GLFW_GAMEPAD_BUTTON_X, RETRO_DEVICE_ID_JOYPAD_Y },
	{ GLFW_GAMEPAD_BUTTON_Y, RETRO_DEVICE_ID_JOYPAD_X },
	{ GLFW_GAMEPAD_BUTTON_DPAD_UP, RETRO_DEVICE_ID_JOYPAD_UP },
	{ GLFW_GAMEPAD_BUTTON_DPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ GLFW_GAMEPAD_BUTTON_DPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ GLFW_GAMEPAD_BUTTON_START, RETRO_DEVICE_ID_JOYPAD_START },
	{ GLFW_GAMEPAD_BUTTON_BACK, RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, RETRO_DEVICE_ID_JOYPAD_L },
	{ GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, RETRO_DEVICE_ID_JOYPAD_R },
	{ GLFW_GAMEPAD_BUTTON_LEFT_THUMB, RETRO_DEVICE_ID_JOYPAD_L3 },
	{ GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, RETRO_DEVICE_ID_JOYPAD_R3 },

	{ 0, 0 }
};

#define MAX_PLAYERS 5
static unsigned state[MAX_PLAYERS][RETRO_DEVICE_ID_JOYPAD_R3+1] = { 0 };
static retro_keyboard_event_t key_event = NULL;
extern GLFWwindow *window;

void input_poll(void) {
	int i;

	if (key_event)
	{
		for (i = 0; kbd_binds[i].k || kbd_binds[i].rk; ++i)
		{
			bool pressed = glfwGetKey(window, kbd_binds[i].k) == GLFW_PRESS;
			key_event(pressed, kbd_binds[i].rk, 0, 0);
		}
	}
	else
	{
		for (i = 0; kbd2joy_binds[i].k || kbd2joy_binds[i].rk; ++i)
			state[0][kbd2joy_binds[i].rk] = glfwGetKey(window, kbd2joy_binds[i].k) == GLFW_PRESS;

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);
	}

	int port;
	for (port = 0; port < MAX_PLAYERS; port++)
		if (glfwJoystickIsGamepad(port))
		{
			GLFWgamepadstate pad;
			if (glfwGetGamepadState(port, &pad))
				for (i = 0; i < 11; i++)
					state[port][joy_binds[i].rk] = pad.buttons[joy_binds[i].k];

			int count;
			const float *axes = glfwGetJoystickAxes(port, &count);
			if (g_cfg.map_analog_to_dpad)
			{
				if (count >= 2)
				{
					state[port][RETRO_DEVICE_ID_JOYPAD_LEFT] = axes[GLFW_GAMEPAD_AXIS_LEFT_X] < -0.5;
					state[port][RETRO_DEVICE_ID_JOYPAD_RIGHT] = axes[GLFW_GAMEPAD_AXIS_LEFT_X] > 0.5;
					state[port][RETRO_DEVICE_ID_JOYPAD_UP] = axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5;
					state[port][RETRO_DEVICE_ID_JOYPAD_DOWN] = axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5;
				}
			}

			state[port][RETRO_DEVICE_ID_JOYPAD_L2] = axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.5;
			state[port][RETRO_DEVICE_ID_JOYPAD_R2] = axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5;
		}
}

void input_set_keyboard_callback(retro_keyboard_event_t e)
{
	key_event = e;
}

static double oldx = 0;
static double oldy = 0;

int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port < MAX_PLAYERS && device == RETRO_DEVICE_JOYPAD)
		return state[port][id];

	if (device == RETRO_DEVICE_MOUSE) {
		double x = 0;
		double y = 0;
		glfwGetCursorPos(window, &x, &y);
		if (id == RETRO_DEVICE_ID_MOUSE_X)
		{
			int16_t d = x - oldx;
			oldx = x;
			return d;
		}
		if (id == RETRO_DEVICE_ID_MOUSE_Y)
		{
			int16_t d = y - oldy;
			oldy = y;
			return d;
		}
		if (id == RETRO_DEVICE_ID_MOUSE_LEFT && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
			return 1;
	}

	if (device == RETRO_DEVICE_KEYBOARD)
		for (int i = 0; kbd_binds[i].k || kbd_binds[i].rk; ++i)
			if (id == kbd_binds[i].rk && window && glfwGetKey(window, kbd_binds[i].k) == GLFW_PRESS)
				return 1;

	return 0;
}
