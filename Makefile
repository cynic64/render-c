main: main.c swapchain.h
	cc main.c -lvulkan -lglfw -o bin/main
