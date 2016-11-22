OBJS=obj/settings.o obj/input.o obj/main.o
HEADERS=src/input.h src/settings.h src/menu.h

$(info Checking current platform)
UNAME = $(shell uname -s)

ifeq ($(UNAME),Linux)
    $(info Current platform is $(UNAME). OK...)
    DIRS=/usr/include/SDL2
endif

ifeq ($(UNAME),Darwin)
    $(info Current platform is $(UNAME). OK...)
    DIRS=/usr/local/include/SDL2
    LIBS=-L/usr/local/lib
endif

all: input

input: $(OBJS) $(HEADERS)
	gcc $(OBJS) $(LIBS) -lSDL2 -lSDL2_ttf -o input -g

obj/%.o: src/%.c
	gcc -c $< -o $@ -g -I$(DIRS)

clean:
	rm obj/*
