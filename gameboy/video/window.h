#pragma once
#include <SFML/Graphics.hpp>
#include <gameboy.h>

#include <imgui.h>
#include <imgui-SFML.h>

#include <string>
#include <iostream>
#include <chrono>

using std::unique_ptr;

using namespace std::chrono;
using dmilliseconds = duration<double, std::milli>;

class GameBoy;
class Window {
public:
	Window(int width, int height, const std::string& name, GameBoy* _gb);
	virtual ~Window();

	void update();
	void render();

	double get_deltatime();
	bool should_close();

	Key map_key(sf::Keyboard::Key key);

public:
	unique_ptr<sf::RenderWindow> window;
	GameBoy* gb;

	system_clock clock;
	system_clock::time_point previous, now;
	dmilliseconds delta_time;
	
	sf::Clock delta_clock;

	int width, height;
	int interval = 0;
};