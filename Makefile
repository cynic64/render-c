CC=cc
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare
LIBS=-lvulkan -lglfw -lm
INCLUDES=-Iexternal/cglm/include

all: square uniform

square: examples/square.c ll-headers shaders
	$(CC) $(CFLAGS) $(LIBS) examples/square.c -o square

uniform: examples/uniform.c ll-headers shaders
	$(CC) $(CFLAGS) $(LIBS) examples/uniform.c -o uniform

shaders: shaders/square/shader.fs.glsl shaders/square/shader.vs.glsl shaders/uniform/shader.fs.glsl shaders/uniform/shader.vs.glsl
	./compile_shaders.pl

ll-headers: src/ll/*.h

.PHONY: clean

clean:
	rm square
