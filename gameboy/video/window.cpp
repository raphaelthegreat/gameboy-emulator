#include "window.h"

Window::Window(int _width, int _height, const std::string& name)
{
	width = _width; height = _height;
	
	window = std::make_unique<sf::RenderWindow>(sf::VideoMode(width, height), name);
	window->setFramerateLimit(60);
}

Window::~Window()
{
}

void Window::update()
{
	sf::Event event;
	while (window->pollEvent(event))
	{
		// Request for closing the window
		if (event.type == sf::Event::Closed)
			window->close();
	}

	auto now = clock.now();
	auto diff = now - previous;

	delta_time = duration_cast<dmilliseconds>(diff);
	previous = now;
}

void Window::render(sf::Sprite& viewport)
{
	window->clear();
	window->draw(viewport);
	window->display();
}

double Window::get_deltatime()
{
	return delta_time.count();
}

bool Window::should_close()
{
	return !window->isOpen();
}

