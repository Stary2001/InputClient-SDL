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

#include "input.h"
#include "menu.h"
#include "settings.h"

#define JOY_DEADZONE 1700
#define CPAD_BOUND 0x5d0

const char *font_path = "DejaVuSans.ttf";
TTF_Font *font;
SDL_Renderer *sdlRenderer;

// \/ input state.
int16_t circle_x = 0;
int16_t circle_y = 0;
uint32_t hid_buttons = 0xfffff000;

int8_t touching = 0;
int16_t touch_x = 0;
int16_t touch_y = 0;

int sock_fd = -1;
struct sockaddr_in sock_addr;

SDL_Surface *screen_surface;
int curr_state = 0;
int curr_item = 0; // curr menu item.
int num_items = 1;
int run = 1; // Run?
struct settings settings;
int last_keycode = 0;
int capture = 0;
int window_w = 640;
int window_h = 480;

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
	char v[12];
	uint32_t hid_state = ~hid_buttons;
	uint32_t circle_state = 0x800800;
	uint32_t touch_state = 0x2000000;

	if(circle_x != 0 || circle_y != 0) // Do circle magic. 0x5d0 is the upper/lower bound of circle pad input
	{
		uint32_t x = circle_x;
		uint32_t y = circle_y;
		x = ((x * CPAD_BOUND) / 32768) + 2048;
		y = ((y * CPAD_BOUND) / 32768) + 2048;
		circle_state = x | (y << 12);
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
    memcpy(v + 8, &circle_state, 4);
    memcpy(v + 4, &touch_state, 4);

    int i = sendto(sock_fd, v, 12, 0, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_in));
}

void set(uint32_t a, int32_t b)
{
	switch(a)
	{
		case KEY_CPAD_UP:
			if(b==0 || b==1) { circle_y = 32767 * b; }
			else { circle_y = b; }
		break;

		case KEY_CPAD_DOWN:
			if(b==0 || b==1) { circle_y = -32767 * b; }
			else { circle_y = -b; }
		break;

		case KEY_CPAD_LEFT:
			if(b==0 || b==1) { circle_x = -32767 * b; }
			else { circle_x = -b; }
		break;

		case KEY_CPAD_RIGHT:
			if(b==0 || b==1) { circle_x = 32767 * b; }
			else { circle_x = b; }
		break;

		default:
		{
			if(b)
			{
				hid_buttons |= a;
			}
			else
			{
				hid_buttons &= ~a;
			}
		}
		break;
	}
}

void draw_text(const char *t, SDL_Color c, int x, int y, int *w, int *h)
{
	if(strlen(t) == 0) return;

	SDL_Rect rekt1, rekt2;
	SDL_Surface* text_surface = TTF_RenderText_Solid(font, t, c);
	rekt1.x = 0;
	rekt1.y = 0;
	rekt2.x = x;
	rekt2.y = y;
	rekt1.w = rekt2.w = text_surface->w;
	rekt1.h = rekt2.h = text_surface->h;
	SDL_BlitSurface(text_surface, &rekt1, screen_surface, &rekt2);
	SDL_FreeSurface(text_surface);

	if(w != NULL)
	{
		*w = rekt1.w;
	}
	if(h != NULL)
	{
		*h = rekt1.h;
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

				if(prefix != NULL && p != NULL)
				{
					sprintf(buff, "%s %s%i", prefix, prefix_2, *p);
				}
				button_text = buff;
			}

			draw_text(button_text, c, w, (i+2) * h, NULL, NULL);
		}
	}
	else if(menus[curr_state].type == NET)
	{
		const char *s = settings.ip;
		if(s == NULL) { s = "None"; }
		draw_text("IP: ", font_color, 0, h, &w, NULL);
		draw_text(s, font_color, w, h, NULL, NULL);
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

void process_menu(SDL_Event *ev, int i)
{
	if(ev->type == SDL_KEYDOWN)
	{
		if(capture && i == 0) // Keyboard only.
		{
			set_binding(i, curr_item, TYPE_KEY, ev->key.keysym.sym, 0);
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

				case SDLK_RETURN:
					capture = 1;
					settings.bindings[i][curr_item].type = TYPE_NONE;
				break;
			}
		}
	}
	else if(i == 1)
	{
		if(ev->type == SDL_JOYBUTTONDOWN && capture)
		{
			set_binding(i, curr_item, TYPE_BUTTON, ev->jbutton.button, 0);
		}
		else if(ev->type == SDL_JOYAXISMOTION && capture)
		{
			uint8_t b = ev->jaxis.axis;
			int16_t v = ev->jaxis.value;
			if(abs(v) < JOY_DEADZONE * 4)
			{
				return;
			}

			set_binding(i, curr_item, TYPE_AXIS, ev->jaxis.axis, v < 0);

		}
		else if(ev->type == SDL_JOYHATMOTION && capture)
		{
			set_binding(i, curr_item, TYPE_HAT, ev->jhat.value, 0);
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

	int settings_fail = load_settings(settings_filename, &settings);
	
	if(settings.ip != NULL && connect_to_3ds(settings.ip))
	{
		printf("failed to connect to '%s'!\n", settings.ip);
		return 1;
	}

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	printf("%i joysticks detected..\n", SDL_NumJoysticks());
	SDL_Joystick *joy = NULL;

	if(SDL_NumJoysticks() > 0)
	{
		joy = SDL_JoystickOpen(0);
		printf("opened joystick %x\n", joy);
	}

	SDL_JoystickEventState(SDL_ENABLE);

	SDL_Window *win = SDL_CreateWindow("sdl thing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_w, window_h, 0);
	screen_surface = SDL_GetWindowSurface(win);
	TTF_Init();
	font = TTF_OpenFont(font_path, 16);
	if(font == NULL)
	{
		printf("couldn't load font..\n");
		return 1;
	}
	
	if(settings_fail)
	{
		curr_state = 4;
		update_screen();
		SDL_UpdateWindowSurface(win);
	}

	int i = 0;
	int dirty = 0;
	SDL_Color bg = {0, 0, 0, 0};

	SDL_Event ev;

	while(run)
	{
		while(SDL_PollEvent(&ev))
		{
			if(ev.type == SDL_KEYDOWN)
			{
				if(ev.key.keysym.sym == SDLK_ESCAPE)
				{
					if(curr_state == 0)
					{
						run = 0;
						break;
					}
					else
					{
						curr_state = 0;
					}
				}
				else if(ev.key.keysym.sym == SDLK_F1)
				{
					curr_state = 1;
				}
				else if(ev.key.keysym.sym == SDLK_F2)
				{
					curr_state = 2;
				}
				else if(ev.key.keysym.sym == SDLK_F3)
				{
					curr_state = 3;
				}
			}
			else if(ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE || ev.type == SDL_QUIT)
			{
				run = 0;
				break;
			}

			dirty = 1;
			switch(curr_state)
			{
				case 0:
					process_input(&ev);
				break;

				case 1:
				case 2:
					process_menu(&ev, curr_state-1);
				break;
			}
		}

		if(dirty)
		{
			send_frame();
			SDL_Rect rekt;
			rekt.x = 0;
			rekt.y = 0;
			rekt.w = window_w;
			rekt.h = window_h;
			SDL_FillRect(screen_surface, &rekt, SDL_MapRGB(screen_surface->format, bg.r, bg.g, bg.b));
			update_screen();
			SDL_UpdateWindowSurface(win);

			dirty = 0;
		}
	}

	if(joy != NULL)
	{
		SDL_JoystickClose(joy);
	}

die:
	TTF_Quit();
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
#if defined(WIN32) || defined(_WIN32)
	WSACleanup();
#endif
	return 0;
}
