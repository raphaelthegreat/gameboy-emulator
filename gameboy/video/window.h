#pragma once
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_filesystem.h>
#include <GL/GLU.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl2.h>

#include <string>
#include <iostream>
#include <chrono>

using namespace std::chrono;
using dmilliseconds = duration<double, std::milli>;

class Window {
public:
	Window(int width, int height, const std::string& name);
	virtual ~Window();

	void update();
	void render(SDL_Texture* texture);

	double get_deltatime();

	bool should_close();

public:
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_GLContext context;
	SDL_Event event;
	
	system_clock clock;
	system_clock::time_point previous, now;
	dmilliseconds delta_time;
	
	int width, height;
	bool should_quit = false;
};