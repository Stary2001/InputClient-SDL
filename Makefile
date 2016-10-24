OBJS=obj/settings.o obj/input.o obj/main.o
HEADERS=src/input.h src/settings.h src/menu.h
DIRS=/usr/include/SDL2

all: input

input: $(OBJS) $(HEADERS)
	gcc $(OBJS) -lSDL2 -lSDL2_ttf -o input -g

obj/%.o: src/%.c
	gcc -c $< -o $@ -g -I$(DIRS)

clean:
	rm obj/*
