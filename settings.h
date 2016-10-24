#pragma once

enum binding_type
{
	KEY,
	BUTTON,
	HAT,
};

struct binding
{
	enum binding_type type;
	int key;
	int button;
	int hat;
};

struct axis
{
	int x_invert;
	int y_invert;
};

struct settings
{
	struct binding bindings[2][NUM_BUTTONS];
	struct axis cpad_axes[2];
	struct axis cstick_axes[2];
};

int save_settings(const char *filename, struct settings *s);
int load_settings(const char *filename, struct settings *s);