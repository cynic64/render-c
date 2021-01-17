CC=cc
CFLAGS=-O3 -Wall -Wextra -pedantic -Wno-sign-compare
main: main.c swapchain.h
	$(CC) $(CFLAGS) main.c -lvulkan -lglfw -o bin/main
