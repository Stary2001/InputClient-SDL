#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL.h>
#include "input.h"
#include "settings.h"

int save_settings(const char *filename, struct settings *s)
{	
	FILE *f = fopen(filename, "w");
	if(f == NULL) { return 1; }

	for(int i = 0; i < NUM_BUTTONS; i++)
	{
		const char *c = SDL_GetKeyName(s->bindings[0][i].key);
		if(c == NULL || strlen(c) == 0)
		{
			c = "none";
		}

		fprintf(f, "kb%s=%s\n", buttons[i].name, c);
	}

	for(int i = 0; i < NUM_BUTTONS; i++)
	{
		fprintf(f, "pad%s=", buttons[i].name);
		switch(s->bindings[1][i].type)
		{
			case TYPE_BUTTON:
				fprintf(f, "button%i\n", s->bindings[1][i].button);
			break;
			case TYPE_HAT:
				fprintf(f, "hat%i\n", s->bindings[1][i].hat);
			break;
			case TYPE_AXIS:
				fprintf(f, "axis%s%i\n", s->bindings[1][i].axis.invert ? "-" : "+", s->bindings[1][i].axis.axis);
			break;
			default:
				fprintf(f, "none\n");
			break;
		}
	}

	if(s->ip_len != 0)
	{
		fprintf(f, "ip=%s\n", s->ip);
	}

	fprintf(f, "frame_ms=%d\n", s->frame_ms);

	fclose(f);

	return 0;
}

int find_key(const char *key_name)
{
	for(int i = 0; i < NUM_BUTTONS; i++)
	{
		if(strcmp(buttons[i].name, key_name) == 0)
		{
			return i;
		}
	}

	return -1;
}

int load_settings(const char *filename, struct settings *s)
{
	char line_buff[256];

	FILE *f = fopen(filename, "r");
	if(f == NULL) { return 1; }

	while(fgets(line_buff, 256, f))
	{
		char *k = strchr(line_buff, '=');
		char *b = strchr(line_buff, '\r');
		if (b == NULL) b = strchr(line_buff, '\n'); // this accounts for \r\n line endings (windows style)

		if(k == NULL) continue;
		*k = 0;
		k++;
		if(b != NULL) *b = 0;

		char *name = line_buff;

		int key_index;
		if(strncmp(name, "kb", 2) == 0)
		{
			name += 2;
			key_index = find_key(name);
			if(key_index == -1) continue;

			s->bindings[0][key_index].type = TYPE_KEY;
			s->bindings[0][key_index].key = SDL_GetKeyFromName(k);
			if(s->bindings[0][key_index].key == SDLK_UNKNOWN)
			{
				s->bindings[0][key_index].type = TYPE_NONE;
			}
		}
		else if(strncmp(name, "pad", 3) == 0)
		{
			name += 3;
			key_index = find_key(name);
			if(key_index == -1) continue;

			if(strncmp(k, "button", 6) == 0)
			{
				k += 6;
				s->bindings[1][key_index].type = TYPE_BUTTON;
				s->bindings[1][key_index].button = atoi(k);
			}
			else if(strncmp(k, "hat", 3) == 0)
			{
				k += 3;
				s->bindings[1][key_index].type = TYPE_HAT;
				s->bindings[1][key_index].hat = atoi(k);
			}
			else if(strncmp(k, "axis", 4) == 0)
			{
				k += 4;
				s->bindings[1][key_index].type = TYPE_AXIS;
				s->bindings[1][key_index].axis.invert = *k++ == '+' ? 0 : 1;
				s->bindings[1][key_index].axis.axis = atoi(k);
			}
		}
		else if(strncmp(name, "ip", 2) == 0)
		{
			strncpy(s->ip, k, 15);
			s->ip_len = strlen(k);
		}
		else if(strncmp(name, "frame_ms", 8) == 0)
		{
			s->frame_ms = atoi(k);
		}
	}

	fclose(f);

	return 0;
}
