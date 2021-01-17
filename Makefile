CC=cc
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare

triangle: examples/triangle.c src/ll/swapchain.h
	$(CC) $(CFLAGS) examples/triangle.c -lvulkan -lglfw -o triangle

.PHONY: clean

clean:
	rm triangle
