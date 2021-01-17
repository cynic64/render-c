CC=cc
CFLAGS=-O3
main: main.c swapchain.h
	$(CC) $(CFLAGS) main.c -lvulkan -lglfw -o bin/main
