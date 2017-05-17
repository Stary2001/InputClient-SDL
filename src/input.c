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
	{"CPadUp", KEY_CPAD_UP},
	{"CPadDown", KEY_CPAD_DOWN},
	{"CPadLeft", KEY_CPAD_LEFT},
	{"CPadRight", KEY_CPAD_RIGHT},
	{"CStickUp", KEY_CSTICK_UP},
	{"CStickDown", KEY_CSTICK_DOWN},
	{"CStickLeft", KEY_CSTICK_LEFT},
	{"CStickRight", KEY_CSTICK_RIGHT},
	{"HOME", KEY_HOME},
	{"Power", KEY_POWER},
	{"Power (long)", KEY_POWER_LONG}
};
#undef AXIS