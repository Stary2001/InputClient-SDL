#include "input.h"

#define AXIS -1
struct button buttons[NUM_BUTTONS] =
{
	{"A", KEY_A},
	{"B", KEY_B},
	{"X", KEY_X},
	{"Y", KEY_Y},
	{"Up", KEY_DUP},
	{"Down", KEY_DDOWN},
	{"Left", KEY_DLEFT},
	{"Right", KEY_DRIGHT},
	{"L", KEY_L},
	{"R", KEY_R},
	{"ZL", KEY_ZL},
	{"ZR", KEY_ZR},
	{"Start", KEY_START},
	{"Select", KEY_SELECT},
	{"Circle Pad Up", KEY_CPAD_UP},
	{"Circle Pad Down", KEY_CPAD_DOWN},
	{"Circle Pad Left", KEY_CPAD_LEFT},
	{"Circle Pad Right", KEY_CPAD_RIGHT},
	{"C-Stick Up", KEY_CSTICK_UP},
	{"C-Stick Down", KEY_CSTICK_DOWN},
	{"C-Stick Left", KEY_CSTICK_LEFT},
	{"C-Stick Right", KEY_CSTICK_RIGHT},
	{"HOME", KEY_HOME},
	{"Power", KEY_POWER},
	{"Power (long)", KEY_POWER_LONG}
};
#undef AXIS