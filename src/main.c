#include <stdio.h>
#include <string.h>
#include <math.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include <sys/types.h>
#include <errno.h>

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "input.h"
#include "menu.h"
#include "settings.h"

#define JOY_DEADZONE 1700
#define CPAD_BOUND 0x5d0
#define CPP_BOUND 0x7f

#define ACCEPTING_INPUT 0
#define MENU_KEYBOARD 1
#define MENU_JOYPAD 2
#define MENU_NET 3
#define MENU_INFO 4

const char *font_path = "DejaVuSans.ttf";
TTF_Font *font;
SDL_Renderer *sdlRenderer;

// \/ input state.
int16_t circle_x = 0;
int16_t circle_y = 0;
int16_t cstick_x = 0;
int16_t cstick_y = 0;

uint32_t hid_buttons = 0xfffff000;
uint32_t special_buttons = 0;
uint32_t zlzr_state = 0;

int8_t touching = 0;
int16_t touch_x = 0;
int16_t touch_y = 0;

int sock_fd = -1;
struct sockaddr_in sock_addr;

SDL_Surface *screen_surface;
SDL_Color bg_color = {0, 0, 0, 0};
int window_w = 640;
int window_h = 480;

int curr_state = 0;
int curr_item = 0; // curr menu item.
int num_items = 1;
int run = 1; // Run?
struct settings settings;
int last_keycode = 0;
int capture = 0;

unsigned int frame_deadline = 0;

const char *settings_filename = "input.conf";

int connect_to_3ds(const char *addr)
{
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(4950);

	struct addrinfo *res;
	int r = getaddrinfo(addr, "4950", &hints, &res);
	if (r != 0)
	{
		close(sock_fd);
		return 1;
	}

	struct addrinfo *s;
	for (s = res; s != NULL; s = s->ai_next)
	{
		memcpy(&sock_addr, s->ai_addr, s->ai_addrlen);
	}
	freeaddrinfo(res);

	return 0;
}

void send_frame()
{
	if(sock_fd == -1) return;
	char v[20];
	uint32_t hid_state = ~hid_buttons;
	uint32_t circle_state = 0x7ff7ff;
	uint32_t cstick_state = 0x80800081;
	uint32_t touch_state = 0x2000000;

	if(circle_x != 0 || circle_y != 0) // Do circle magic. 0x5d0 is the upper/lower bound of circle pad input
	{
		uint32_t x = circle_x;
		uint32_t y = circle_y;
		x = ((x * CPAD_BOUND) / 32768) + 2048;
		y = ((y * CPAD_BOUND) / 32768) + 2048;
		circle_state = x | (y << 12);
	}

	if(cstick_x != 0 || cstick_y != 0 || zlzr_state != 0)
	{
		double x = cstick_x / 32768.0;
		double y = cstick_y / 32768.0;

		// We have to rotate the c-stick position 45deg. Thanks, Nintendo.
		uint32_t xx = (uint32_t)((x+y) * M_SQRT1_2 * CPP_BOUND) + 0x80;
		uint32_t yy = (uint32_t)((y-x) * M_SQRT1_2 * CPP_BOUND) + 0x80;

		cstick_state = (yy&0xff) << 24 | (xx&0xff) << 16 | (zlzr_state&0xff) << 8 | 0x81;
	}

	if(touching) // This is good enough.
	{
		uint32_t x = touch_x;
		uint32_t y = touch_y;
		x = (x * 4096) / window_w;
		y = (y * 4096) / window_h;
		touch_state = x | (y << 12) | (0x01 << 24);
	}

	memcpy(v, &hid_state, 4);
	memcpy(v + 4, &touch_state, 4);
	memcpy(v + 8, &circle_state, 4);
	memcpy(v + 12, &cstick_state, 4);
	memcpy(v + 16, &special_buttons, 4);

	int i = sendto(sock_fd, v, 20, 0, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_in));
}

uint32_t hid_values[] =
{
	BIT(0), ///< A
	BIT(1), ///< B
	BIT(2), ///< Select
	BIT(3), ///< Start
	BIT(4), ///< D-Pad Right
	BIT(5), ///< D-Pad Left
	BIT(6), ///< D-Pad Up
	BIT(7), ///< D-Pad Down
	BIT(8), ///< R
	BIT(9), ///< L
	BIT(10), ///< X
	BIT(11), //< Y
};

uint32_t special_values[] =
{
	BIT(0), // HOME
	BIT(1), // Power
	BIT(2)  // Power(long)
};

/**
 * Manipulate CircleY, CircleX, CppX, CppY and HID Buttons according to supplied input
 */
void set(uint32_t button, int32_t value)
{
	switch(button)
	{
		case KEY_CPAD_UP:
			if(value == 0 || value == 1) { circle_y = 32767 * value; }
			else { circle_y = value; }
		break;

		case KEY_CPAD_DOWN:
			if(value == 0 || value == 1) { circle_y = -32767 * value; }
			else { circle_y = -value; }
		break;

		case KEY_CPAD_LEFT:
			if(value == 0 || value == 1) { circle_x = -32767 * value; }
			else { circle_x = -value; }
		break;

		case KEY_CPAD_RIGHT:
			if(value == 0 || value == 1) { circle_x = 32767 * value; }
			else { circle_x = value; }
		break;

		case KEY_CSTICK_UP:
			if(value == 0 || value == 1) { cstick_y = 32767 * value; }
			else { cstick_y = value; }
		break;

		case KEY_CSTICK_DOWN:
			if(value == 0 || value == 1) { cstick_y = -32767 * value; }
			else { cstick_y = value; }
		break;

		case KEY_CSTICK_LEFT:
			if(value == 0 || value == 1) { cstick_x = -32767 * value; }
			else { cstick_x = value; }
		break;

		case KEY_CSTICK_RIGHT:
			if(value == 0 || value == 1) { cstick_x = 32767 * value; }
			else { cstick_x = value; }
		break;

		case KEY_ZL:
		case KEY_ZR:
		{
			uint32_t b;
			if(button == KEY_ZR) b = 0x2;
			if(button == KEY_ZL) b = 0x4;

			if(value)
			{
				zlzr_state |= b;
			}
			else
			{
				zlzr_state &= ~b;
			}
		}
		break;

		case KEY_HOME:
		case KEY_POWER:
		case KEY_POWER_LONG:
		{
			button -= KEY_HOME;
			if(value)
			{
				special_buttons |= special_values[button];
			}
			else
			{
				special_buttons &= ~special_values[button];
			}
		}
		break;

		default:
		{
			if(value)
			{
				hid_buttons |= hid_values[button];
			}
			else
			{
				hid_buttons &= ~hid_values[button];
			}
		}
		break;
	}
}

void draw_text(const char *t, SDL_Color c, int x, int y, int *w, int *h)
{
	if(strlen(t) == 0) return;

	SDL_Rect rect1, rect2;
	SDL_Surface* text_surface = TTF_RenderText_Shaded(font, t, c, bg_color);
	rect1.x = 0;
	rect1.y = 0;
	rect2.x = x;
	rect2.y = y;
	rect1.w = rect2.w = text_surface->w;
	rect1.h = rect2.h = text_surface->h;
	SDL_BlitSurface(text_surface, &rect1, screen_surface, &rect2);
	SDL_FreeSurface(text_surface);

	if(w != NULL)
	{
		*w = rect1.w;
	}
	if(h != NULL)
	{
		*h = rect1.h;
	}
}

void update_screen()
{
	SDL_Color font_color = {255, 255, 255, 0};
	SDL_Color highlight_color = {255, 0, 0, 0};
	if(capture)
	{
		highlight_color.r = 255;
		highlight_color.g = 255;
		highlight_color.b = 0;
		highlight_color.a = 0;
	}

	int h;
	int w;

	draw_text(menus[curr_state].name, font_color, 0, 0, NULL, &h);
	if(menus[curr_state].type == INPUT_KB || menus[curr_state].type == INPUT_CONTROLLER) // bind.
	{
		num_items = NUM_BUTTONS;
		for(int i = 0; i < NUM_BUTTONS; i++)
		{
			SDL_Color c = font_color;
			if(curr_item == i)
			{
				c = highlight_color;
			}
			char name[32];
			strcpy(name, buttons[i].name);
			strcat(name, " - ");
			draw_text(name, c, 0, (i+2) * h, &w, NULL);

			int bind_index = menus[curr_state].type == INPUT_KB ? 0 : 1;
			const char *button_text;
			char buff[32];

			if(settings.bindings[bind_index][i].type == TYPE_NONE)
			{
				button_text = "None";
			}
			else if(settings.bindings[bind_index][i].type == TYPE_KEY)
			{
				button_text = SDL_GetKeyName(settings.bindings[bind_index][i].key);
			}
			else
			{
				const char *prefix = NULL;
				const char *prefix_2 = "";
				int *p = NULL;

				switch(settings.bindings[bind_index][i].type)
				{
					case TYPE_BUTTON:
						prefix = "Button ";
						p = &settings.bindings[bind_index][i].button;
					break;

					case TYPE_HAT:
						prefix = "Hat ";
						p = &settings.bindings[bind_index][i].hat;
					break;

					case TYPE_AXIS:
						prefix = "Axis ";
						prefix_2 = settings.bindings[bind_index][i].axis.invert ? "-" : "+";
						p = &settings.bindings[bind_index][i].axis.axis;
					break;
				}

				button_text = buff;
			}

			draw_text(button_text, c, w, (i+2) * h, NULL, NULL);
		}
	}
	else if(menus[curr_state].type == NET)
	{
		num_items = 2;
		SDL_Color c = curr_item == 0 ? highlight_color : font_color;

		char buff[64];

		const char *s = settings.ip;
		if(settings.ip_len == 0) { s = "None"; }
		draw_text("IP: ", c, 0, h, &w, NULL);
		draw_text(s, c, w, h, NULL, NULL);

		c = curr_item == 1 ? highlight_color : font_color;

		sprintf(buff, "Frame frequency: every %i ms", settings.frame_ms);
		draw_text(buff, c, 0, h * 2, NULL, NULL);
	}
	else if(menus[curr_state].type == INFO)
	{
		int num_lines = 5;
		const char *info_text[] = {"Keybindings: ",
								"F1 = Bindings (Keyboard)",
								"F2 = Bindings (Controller)",
								"F3 = Network Settings",
								"Esc = Back (or quit)"};
		int y = h;

		for(int i = 0; i < num_lines; i++)
		{
			draw_text(info_text[i], font_color, 0, y, NULL, &h);
			y += h;
		}
	}
}

/**
 * Process event and set the variables for sending information to the 3DS.
 *
 * @arg SDL_Event *ev
 */
void process_input(SDL_Event *ev)
{
	switch(ev->type)
	{
		case SDL_JOYAXISMOTION:
		{
			uint8_t b = ev->jaxis.axis;
			int16_t v = ev->jaxis.value;
			if(abs(v) < JOY_DEADZONE)
			{
				v = 0;
			}

			if(v == -32768)
			{
				v++;
			}

			for(int i = 0; i < NUM_BUTTONS; i++)
			{
				struct binding *b = &settings.bindings[1][i];
				if(b->type == TYPE_AXIS && b->axis.axis == ev->jaxis.axis)
				{
					if (b->axis.invert == (v < 0))
					{
						set(buttons[i].key, abs(v));
					}
				}
			}
		}
		break;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		{
			uint8_t b = ev->jbutton.button;
			for(int i = 0; i < NUM_BUTTONS; i++)
			{
				struct binding *b = &settings.bindings[1][i];
				if(b->type == TYPE_BUTTON && b->button == ev->jbutton.button)
				{
					set(buttons[i].key, ev->type == SDL_JOYBUTTONDOWN);
				}
			}
		}
		break;

		case SDL_JOYHATMOTION:
		{
			uint8_t v = ev->jhat.value;
			for(int i = 0; i < NUM_BUTTONS; i++)
			{
				if(settings.bindings[1][i].type == TYPE_HAT)
				{
					set(buttons[i].key, v & settings.bindings[1][i].hat);
				}
			}
		}
		break;

		case SDL_MOUSEBUTTONDOWN:
			touching = 1;
			touch_x = ev->button.x;
			touch_y = ev->button.y;
		break;

		case SDL_MOUSEMOTION:
			touch_x = ev->motion.x;
			touch_y = ev->motion.y;
		break;

		case SDL_MOUSEBUTTONUP:
			touching = 0;
			touch_x = 0;
			touch_y = 0;
		break;

		case SDL_KEYDOWN:
		case SDL_KEYUP:
			for(int i = 0; i < NUM_BUTTONS; i++)
			{
				if(settings.bindings[0][i].key == ev->key.keysym.sym)
				{
					set(buttons[i].key, ev->type == SDL_KEYDOWN);
				}
			}
		break;
	}
}

void set_binding(int i, int item, int type, int val, int inv)
{
	settings.bindings[i][item].type = type;
	switch(type)
	{
		case TYPE_KEY:
			settings.bindings[i][curr_item].key = val;
		break;
		case TYPE_BUTTON:
			settings.bindings[i][curr_item].button = val;
		break;
		case TYPE_HAT:
			settings.bindings[i][curr_item].hat = val;
		break;
		case TYPE_AXIS:
			settings.bindings[i][curr_item].axis.axis = val;
			settings.bindings[i][curr_item].axis.invert = inv;
		break;
	}

	capture = 0;
	save_settings(settings_filename, &settings);
}

/**
 * Handles menu inputs and starting capture
 */
void process_menu(SDL_Event *ev, int curr_menu)
{
	if(ev->type == SDL_KEYDOWN)
	{
		if(capture && curr_menu == 0) // Keyboard only.
		{
			set_binding(curr_menu, curr_item, TYPE_KEY, ev->key.keysym.sym, 0);
		}
		else if(capture && curr_menu == 2) // Net.
		{
			SDL_Keycode key = ev->key.keysym.sym;
			if(curr_item == 0) // IP
			{
				if(key == SDLK_RETURN)
				{
					capture = 0;
					save_settings(settings_filename, &settings);
				}
				else
				{
					char *s = settings.ip + settings.ip_len;

					char c = 0;
					if(key >= SDLK_0 && key <= SDLK_9)
					{
						c = '0' + key - SDLK_0;
					}
					else if(key == SDLK_PERIOD)
					{
						c = '.';
					}

					if(key == SDLK_BACKSPACE && settings.ip_len != 0)
					{
						settings.ip_len--;
						*(s - 1) = 0;
					}
					else if(settings.ip_len != 15)
					{
						*s++ = c;
						*s = 0;
						settings.ip_len++;
					}
				}
			}
			else // Frame delay
			{
				switch(key)
				{
					case SDLK_UP:
						settings.frame_ms += 5;
					break;
					case SDLK_DOWN:
						settings.frame_ms -= 5;
					break;
					case SDLK_LEFT:
						settings.frame_ms -= 10;
					break;
					case SDLK_RIGHT:
						settings.frame_ms += 10;
					break;
					case SDLK_RETURN:
						capture = 0;
						save_settings(settings_filename, &settings);
					break;
				}
			}
		}
		else
		{
			switch(ev->key.keysym.sym)
			{
				case SDLK_UP:
					if(curr_item != 0) curr_item--;
				break;

				case SDLK_DOWN:
					if(curr_item != num_items-1) curr_item++;
				break;

				case SDLK_LEFT:
					curr_item -= 5;
					if(curr_item < 0) curr_item = 0;
				break;

				case SDLK_RIGHT:
					curr_item += 5;
					if(curr_item > num_items-1) curr_item = num_items-1;
				break;

				case SDLK_RETURN:
					capture = 1;
					if(curr_menu == 0 || curr_menu == 1) settings.bindings[curr_menu][curr_item].type = TYPE_NONE;
				break;
			}
		}
	}
	else if(curr_menu == 1)
	{
		if(ev->type == SDL_JOYBUTTONDOWN && capture)
		{
			set_binding(curr_menu, curr_item, TYPE_BUTTON, ev->jbutton.button, 0);
		}
		else if(ev->type == SDL_JOYAXISMOTION && capture)
		{
			uint8_t b = ev->jaxis.axis;
			int16_t v = ev->jaxis.value;
			if(abs(v) < JOY_DEADZONE * 4)
			{
				return;
			}

			set_binding(curr_menu, curr_item, TYPE_AXIS, ev->jaxis.axis, v < 0);

		}
		else if(ev->type == SDL_JOYHATMOTION && capture)
		{
			set_binding(curr_menu, curr_item, TYPE_HAT, ev->jhat.value, 0);
		}
	}
}

int main(int argc, char *argv[])
{
#if defined(WIN32) || defined(_WIN32)
	WSADATA wsaData;
	int winsock_res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (winsock_res != 0)
	{
		printf("winsock startup failed!\n");
		return 1;
	}
#endif

	for (int i = 0; i < NUM_BUTTONS; i++)
	{
		settings.bindings[0][i].type = TYPE_NONE;
		settings.bindings[1][i].type = TYPE_NONE;
	}
	settings.frame_ms = 50; // Default.

	int settings_fail = load_settings(settings_filename, &settings);
	if(settings_fail)
	{
		save_settings(settings_filename, &settings);
	}

	printf("Settings IP: %s \n", settings.ip);

	if(settings.ip_len != 0 && connect_to_3ds(settings.ip))
	{
		printf("Failed to connect to '%s'!\n", settings.ip);
		return 1;
	}

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	printf("Detected %i joysticks. \n", SDL_NumJoysticks());
	SDL_Joystick *joy = NULL;

	if(SDL_NumJoysticks() > 0)
	{
		joy = SDL_JoystickOpen(0);
		printf("Opened joystick 0: %x\n", joy);
	}

	SDL_JoystickEventState(SDL_ENABLE);

	SDL_Window *win = SDL_CreateWindow("InputRedirectSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_w, window_h, 0);
	screen_surface = SDL_GetWindowSurface(win);
	TTF_Init();
	font = TTF_OpenFont(font_path, 14);

	if(font == NULL)
	{
		printf("Could not load font. Ensure %s is in the same directory as the executable. \n", font_path);
		return 1;
	}

	if(settings_fail)
	{
		curr_state = MENU_INFO;
		update_screen();
		SDL_UpdateWindowSurface(win);
	}

	int dirty = 0;
	SDL_Event ev;

	while(run)
	{
		while(SDL_PollEvent(&ev))
		{
			if(ev.type == SDL_KEYDOWN)
			{
				if(ev.key.keysym.sym == SDLK_ESCAPE)
				{
					if(curr_state == ACCEPTING_INPUT)
					{
						run = 0;
						break;
					}
					else
					{
						curr_state = ACCEPTING_INPUT;
					}
				}
				else if(ev.key.keysym.sym == SDLK_F1)
				{
					curr_item = 0;
					curr_state = MENU_KEYBOARD;
				}
				else if(ev.key.keysym.sym == SDLK_F2)
				{
					curr_item = 0;
					curr_state = MENU_JOYPAD;
				}
				else if(ev.key.keysym.sym == SDLK_F3)
				{
					curr_item = 0;
					curr_state = MENU_NET;
				}
			}
			else if((ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) || ev.type == SDL_QUIT)
			{
				run = 0;
				break;
			}

			if(ev.type != SDL_MOUSEMOTION)
				dirty = 1;

			switch(curr_state)
			{
				case 0:
					process_input(&ev);
				break;

				case 1:
				case 2:
				case 3:
					process_menu(&ev, curr_state-1);
				break;
			}
		}

		unsigned int curr_time = SDL_GetTicks();
		if(dirty || frame_deadline <= curr_time)
		{
			frame_deadline = curr_time + settings.frame_ms;
			send_frame();

			SDL_Rect rect;
			rect.x = 0;
			rect.y = 0;
			rect.w = window_w;
			rect.h = window_h;
			SDL_FillRect(screen_surface, &rect, SDL_MapRGB(screen_surface->format, bg_color.r, bg_color.g, bg_color.b));
			update_screen();
			SDL_UpdateWindowSurface(win);

			dirty = 0;
		}
	}

	if(joy != NULL)
	{
		printf("Closing joysticks.\n");
		SDL_JoystickClose(joy);
	}

die:
	printf("Gracefully exiting.\n");
	TTF_Quit();
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
#if defined(WIN32) || defined(_WIN32)
	WSACleanup();
#endif
	return 0;
}
