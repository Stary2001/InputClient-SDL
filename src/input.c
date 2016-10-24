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
	{"Start", KEY_START},
	{"Select", KEY_SELECT},
	{"CPadUp", KEY_CPAD_UP},
	{"CPadDown", KEY_CPAD_DOWN},
	{"CPadLeft", KEY_CPAD_LEFT},
	{"CPadRight", KEY_CPAD_RIGHT},
};
#undef AXIS