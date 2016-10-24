#include <stdio.h>
#include <SDL2/SDL.h>
#include "input.h"
#include "settings.h"

int save_settings(const char *filename, struct settings *s)
{
	char line_buff[256];
	
	FILE *f = fopen(filename, "w");
	if(f == NULL) { return 1; }

	for(int i = 0; i < NUM_BUTTONS; i++)
	{
		fprintf(f, "kb%s=%s\n", buttons[i].name, SDL_GetKeyName(s->bindings[0][i].key));
	}

	for(int i = 0; i < NUM_BUTTONS; i++)
	{
		fprintf(f, "pad%s=", buttons[i].name);
		switch(s->bindings[1][i].type)
		{
			case BUTTON:
				fprintf(f, "button%i\n", s->bindings[1][i].button);
			break;
			case HAT:
				fprintf(f, "hat%i\n", s->bindings[1][i].hat);
			break;
			default:
				fprintf(f, "none\n");
			break;
		}
	}
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
	while(fgets(line_buff, 256, f))
	{
		char *k = strchr(line_buff, '=');
		char *b = strchr(line_buff, '\n');
		if(k == NULL) continue;
		if(b == NULL) continue;
		*k = 0;
		k++;
		*b = 0;

		char *name = line_buff;

		int key_index;
		if(strncmp(name, "kb", 2) == 0)
		{
			name += 2;
			key_index = find_key(name);
			if(key_index == -1) continue;

			s->bindings[0][key_index].type = KEY;
			s->bindings[0][key_index].key = SDL_GetKeyFromName(k);
		}
		else if(strncmp(name, "pad", 3) == 0)
		{
			name += 3;
			key_index = find_key(name);
			if(key_index == -1) continue;

			if(strncmp(k, "button", 6) == 0)
			{
				k += 6;
				s->bindings[1][key_index].type = BUTTON;
				s->bindings[1][key_index].button = atoi(k);
			}
			else if(strncmp(k, "hat", 3) == 0)
			{
				k += 3;
				s->bindings[1][key_index].type = HAT;
				s->bindings[1][key_index].hat = atoi(k);
			}
		}
	}

	fclose(f);

	return 1;
}