CC=cc
EXTRA=
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare $(EXTRA)
LIBS=-lvulkan -lglfw -lm
INCLUDES=-Iexternal/cglm/include

main: main.c ll-headers shaders
	$(CC) $(CFLAGS) $(LIBS) main.c -o main

shaders: shaders/*.glsl
	./compile_shaders.pl

ll-headers: src/ll/*.h

.PHONY: clean

clean:
	rm main
