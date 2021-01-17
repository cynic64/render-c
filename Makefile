CC=cc
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare
LIBS=-lvulkan -lglfw
INCLUDES=-Iexternal/cglm/include

triangle: examples/triangle.c src/ll/swapchain.h triangle-vs triangle-fs
	$(CC) $(CFLAGS) $(LIBS) examples/triangle.c -o triangle

triangle-vs: shaders/triangle/shader.vs.glsl
	glslc -fshader-stage=vertex -o shaders/triangle/shader.vs.spv shaders/triangle/shader.vs.glsl
triangle-fs: shaders/triangle/shader.fs.glsl
	glslc -fshader-stage=fragment -o shaders/triangle/shader.fs.spv shaders/triangle/shader.fs.glsl

.PHONY: clean

clean:
	rm triangle
