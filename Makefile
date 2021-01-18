CC=cc
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare
LIBS=-lvulkan -lglfw
INCLUDES=-Iexternal/cglm/include

square: examples/square.c src/ll/swapchain.h square-vs square-fs
	$(CC) $(CFLAGS) $(LIBS) examples/square.c -o square

square-vs: shaders/square/shader.vs.glsl
	glslc -fshader-stage=vertex -o shaders/square/shader.vs.spv shaders/square/shader.vs.glsl
square-fs: shaders/square/shader.fs.glsl
	glslc -fshader-stage=fragment -o shaders/square/shader.fs.spv shaders/square/shader.fs.glsl

.PHONY: clean

clean:
	rm square
