#define BIT(n) (1U<<(n))

enum
{
	KEY_A       = 0,       ///< A
	KEY_B       = 1,       ///< B
	KEY_SELECT  = 2,       ///< Select
	KEY_START   = 3,       ///< Start
	KEY_DRIGHT  = 4,       ///< D-Pad Right
	KEY_DLEFT   = 5,       ///< D-Pad Left
	KEY_DUP     = 6,       ///< D-Pad Up
	KEY_DDOWN   = 7,       ///< D-Pad Down
	KEY_R       = 8,       ///< R
	KEY_L       = 9,       ///< L
	KEY_X       = 10,      ///< X
	KEY_Y       = 11,      ///< Y
	KEY_ZL      = 12,      ///< ZL (New 3DS only)
	KEY_ZR      = 13,      ///< ZR (New 3DS only)

	KEY_CSTICK_RIGHT = 14, ///< C-Stick Right (New 3DS only)
	KEY_CSTICK_LEFT  = 15, ///< C-Stick Left (New 3DS only)
	KEY_CSTICK_UP    = 16,
	KEY_CSTICK_DOWN  = 17, ///< C-Stick Down (New 3DS only)
	KEY_CPAD_RIGHT = 18,   ///< Circle Pad Right
	KEY_CPAD_LEFT  = 19,   ///< Circle Pad Left
	KEY_CPAD_UP    = 20,   ///< Circle Pad Up
	KEY_CPAD_DOWN  = 21,   ///< Circle Pad Down
	KEY_HOME = 22,
	KEY_POWER = 23,
	KEY_POWER_LONG = 24
};

#define NUM_BUTTONS 25

struct button
{
	const char *name;
	int key;
};

extern struct button buttons[NUM_BUTTONS];