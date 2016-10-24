#pragma once

enum binding_type
{
	TYPE_NONE,
	TYPE_KEY,
	TYPE_BUTTON,
	TYPE_HAT,
	TYPE_AXIS
};

struct axis
{
	int axis;
	int invert;
};

struct binding
{
	enum binding_type type;
	int key;
	int button;
	int hat;
	struct axis axis;
};

struct settings
{
	struct binding bindings[2][NUM_BUTTONS];
};

int save_settings(const char *filename, struct settings *s);
int load_settings(const char *filename, struct settings *s);