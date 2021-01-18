CC=cc
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare
LIBS=-lvulkan -lglfw
INCLUDES=-Iexternal/cglm/include

square: examples/square.c src/ll/swapchain.h shaders
	$(CC) $(CFLAGS) $(LIBS) examples/square.c -o square

shaders: shaders/square/shader.fs.glsl shaders/square/shader.vs.glsl shaders/uniform/shader.fs.glsl shaders/uniform/shader.vs.glsl
	./compile_shaders.pl

.PHONY: clean

clean:
	rm square
