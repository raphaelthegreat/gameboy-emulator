#include "window.h"

using sf::Keyboard;

Window::Window(int _width, int _height, const std::string& name, GameBoy* _gb)
{
	width = _width; height = _height;
	gb = _gb;

	window = std::make_unique<sf::RenderWindow>(sf::VideoMode(width, height), name);
	window->setFramerateLimit(60);

	sf::Image icon;
	icon.loadFromFile("../assets/icon.png");

	window->setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
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
		else if (event.type == sf::Event::KeyPressed) {
			Keyboard::Key code = event.key.code;
			if (code == Keyboard::A || code == Keyboard::B ||
				code == Keyboard::RControl || code == Keyboard::Q ||
				code == Keyboard::Up || code == Keyboard::Down ||
				code == Keyboard::Left || code == Keyboard::Right) {
				gb->joypad.key_pressed(map_key(code));
			}
		}
		else if (event.type == sf::Event::KeyReleased) {
			Keyboard::Key code = event.key.code;
			if (code == Keyboard::A || code == Keyboard::B ||
				code == Keyboard::RControl || code == Keyboard::Q ||
				code == Keyboard::Up || code == Keyboard::Down ||
				code == Keyboard::Left || code == Keyboard::Right) {
				gb->joypad.key_released(map_key(code));
			}
		}
	}

	auto now = clock.now();
	auto diff = now - previous;

	delta_time = duration_cast<dmilliseconds>(diff);
	previous = now;

	if (interval >= 100) {
		std::string title = "Gameboy Emualator FPS: " + std::to_string((int)(1000.0f / get_deltatime()));
		window->setTitle(title);
		interval = 0;
	}

	interval++;
}

void Window::render(sf::Sprite& viewport)
{
	window->clear(sf::Color::Black);
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

Key Window::map_key(sf::Keyboard::Key key)
{
	if (key == sf::Keyboard::A)
		return Key_A;
	if (key == sf::Keyboard::B)
		return Key_B;
	if (key == sf::Keyboard::RControl)
		return Key_Select;
	if (key == sf::Keyboard::Q)
		return Key_Start;
	if (key == sf::Keyboard::Up)
		return Key_Up;
	if (key == sf::Keyboard::Down)
		return Key_Down;
	if (key == sf::Keyboard::Left)
		return Key_Left;
	if (key == sf::Keyboard::Right)
		return Key_Right;
}

