#pragma once
#include <SFML/Graphics.hpp>

#include <string>
#include <iostream>
#include <chrono>

using std::unique_ptr;

using namespace std::chrono;
using dmilliseconds = duration<double, std::milli>;

class Window {
public:
	Window(int width, int height, const std::string& name);
	virtual ~Window();

	void update();
	void render(sf::Sprite& texture);

	double get_deltatime();

	bool should_close();

public:
	unique_ptr<sf::RenderWindow> window;
	
	system_clock clock;
	system_clock::time_point previous, now;
	dmilliseconds delta_time;
	
	int width, height;
};