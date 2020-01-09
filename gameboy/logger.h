#pragma once
#include <imgui.h>
#include <vector>
#include <string>

class Logger {
public:
    Logger();

    void clear();
    void log(const char* fmt, ...);
    void draw(const std::string& title = "Logger");

public:
    ImGuiTextBuffer buf;
    ImGuiTextFilter filter;

    std::vector<int> line_offsets;
    bool auto_scroll;
};