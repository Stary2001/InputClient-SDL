#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include "input.h"
#include "menu.h"
#include "settings.h"

#define JOY_DEADZONE 1000
#define CPAD_BOUND 0x5d0

const char *font_path = "/usr/share/fonts/TTF/DejaVuSans.ttf";
TTF_Font *font;
SDL_Renderer *sdlRenderer;

// \/ input state.
int16_t circle_x = 0;
int16_t circle_y = 0;
uint32_t hid_buttons = 0xfffff000;

int8_t touching = 0;
int16_t touch_x = 0;
int16_t touch_y = 0;

int sock_fd = 0;
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

    struct hostent *server;
    server = gethostbyname(addr);
    if(server == NULL)
    {
    	return 0;
    }
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    memcpy(&sock_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    sock_addr.sin_port = htons(4950);

    return 1;
}

void send_frame()
{
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

void set(uint32_t a, uint32_t b)
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

void draw_text(const char *t, SDL_Color c, int x, int y, int *w, int *h)
{
	if(strlen(t) == 0) return;

	SDL_Rect rekt1, rekt2;
	SDL_Surface* text_surface = TTF_RenderText_Solid(font, t, c);
	rekt1.x = 0;
	rekt1.x = 0;
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

			char buff[32];
			const char *button_text;
			if(menus[curr_state].type == INPUT_KB)
			{
				button_text = SDL_GetKeyName(settings.bindings[0][i].key);
			}
			else
			{
				const char *prefix = NULL;
				switch(settings.bindings[1][i].type)
				{
					case BUTTON:
						prefix = "Button ";
					break;
					case HAT:
						prefix = "Hat ";
					break;
				}
				if(prefix != NULL)
				{
					sprintf(buff, "%s %i", prefix, settings.bindings[1][i].key);
				}
				button_text = buff;
			}

			draw_text(SDL_GetKeyName(settings.bindings[0][i].key), c, w, (i+2) * h, NULL, NULL);
		}
	}
}

int process_input(SDL_Event *ev)
{
	switch(ev->type)
	{
		case SDL_QUIT:
			run = 0;
		break;

		case SDL_JOYAXISMOTION:
		{
			int16_t v = ev->jaxis.value;
			if(abs(v) < JOY_DEADZONE)
			{
				v = 0;
			}

			if(ev->jaxis.axis == 0)
			{
				circle_x = v;
			}
			else if(ev->jaxis.axis == 1)
			{
				if(v == -32768)
				{
					v++; // -32768 when negated is......itself?
				}
				circle_y = -v; // Y is inverted.
			}
			else
			{
				printf("unk axis %i val %i\n", ev->jaxis.axis, ev->jaxis.value);
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
				if(b->type == BUTTON && b->button == ev->jbutton.button)
				{
					set(b->key, ev->type == SDL_JOYBUTTONDOWN);
				}
			}
		}
		break;

		case SDL_JOYHATMOTION:
		{
			uint8_t v = ev->jhat.value;
			for(int i = 0; i < NUM_BUTTONS; i++)
			{
				if(settings.bindings[1][i].type == HAT)
				{
					set(settings.bindings[1][i].key, v & settings.bindings[1][i].hat);
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

void process_menu(SDL_Event *ev)
{
	switch(ev->type)
	{
		case SDL_KEYDOWN:
		if(capture)
		{
			settings.bindings[0][curr_item].key = ev->key.keysym.sym;
			capture = 0;

			save_settings(settings_filename, &settings);

			break;
		}

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
				settings.bindings[0][curr_item].key = SDLK_UNKNOWN;
			break;
		}
	}
}

int main(int argc, char ** argv)
{
	if(argc < 2)
	{
		printf("usage: %s [ip]\n", argv[0]);
		return 1;
	}

	if(!connect_to_3ds(argv[1]))
	{
		printf("failed to connect to '%s'!\n", argv[1]);
		return 1;
	}

	if(!load_settings(settings_filename	, &settings))
	{
		// first time setup blahblah
	}

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	printf("%i joysticks detected..\n", SDL_NumJoysticks());
	SDL_JoystickEventState(SDL_ENABLE);

	SDL_Window *win = SDL_CreateWindow("sdl thing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_w, window_h, 0);
	screen_surface = SDL_GetWindowSurface(win);
	TTF_Init();
	font = TTF_OpenFont(font_path, 16);
	
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
			else if(ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
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
					process_menu(&ev);
				break;
			}
		}

		send_frame();

		if(dirty)
		{
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

	TTF_Quit();
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
}
