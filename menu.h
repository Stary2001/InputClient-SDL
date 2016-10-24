enum menu_type
{
	NONE,
	INPUT_KB,
	INPUT_CONTROLLER,
	NET
};

struct menu
{
	enum menu_type type;
	const char *name;
};

struct menu *curr_menu;
struct menu menus[] = {
	{NONE, ""},
	{INPUT_KB, "Bindings - Keyboard"},
	{INPUT_CONTROLLER, "Bindings - Controller"},
	{NET, "Net Settings"},
};