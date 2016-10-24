OBJS=settings.o input.o main.o
HEADERS=input.h settings.h menu.h

all: input

input: $(OBJS) $(HEADERS)
	gcc $(OBJS) -lSDL2 -lSDL2_ttf -o input -g

%.o: %.c
	gcc -c $< -o $@ -g
