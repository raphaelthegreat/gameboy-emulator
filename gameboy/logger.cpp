#include "logger.h"

Logger::Logger()
{
    auto_scroll = true;
    clear();
}

void Logger::clear()
{
    buf.clear();

    line_offsets.clear();
    line_offsets.push_back(0);
}

void Logger::log(const char* fmt, ...)
{
    int old_size = buf.size();
    va_list args;

    va_start(args, fmt);
    buf.appendfv(fmt, args);
    va_end(args);

    for (int new_size = buf.size(); old_size < new_size; old_size++)
        if (buf[old_size] == '\n')
            line_offsets.push_back(old_size + 1);
}

void Logger::draw(const std::string& title)
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin(title.c_str());

    // Options menu
    if (ImGui::BeginPopup("Options")) {
        ImGui::Checkbox("Auto-scroll", &auto_scroll);
        ImGui::EndPopup();
    }

    // Main window
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");

    ImGui::SameLine();
    bool clear = ImGui::Button("Clear");

    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");

    ImGui::SameLine();
    filter.Draw("Filter", -100.0f);

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (clear) this->clear();
    if (copy) ImGui::LogToClipboard();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* buffer = buf.begin();
    const char* buf_end = buf.end();

    if (filter.IsActive()) {
        for (int line_no = 0; line_no < line_offsets.size(); line_no++) {
            const char* line_start = buffer + line_offsets[line_no];
            const char* line_end = (line_no + 1 < line_offsets.size()) ? (buffer + line_offsets[line_no + 1] - 1) : buf_end;
            if (filter.PassFilter(line_start, line_end))
                ImGui::TextUnformatted(line_start, line_end);
        }
    }
    else {
        ImGuiListClipper clipper;
        clipper.Begin(line_offsets.size());

        while (clipper.Step()) {
            for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
                const char* line_start = buffer + line_offsets[line_no];
                const char* line_end = (line_no + 1 < line_offsets.size()) ? (buffer + line_offsets[line_no + 1] - 1) : buf_end;
                ImGui::TextUnformatted(line_start, line_end);
            }
        }
        clipper.End();
    }

    ImGui::PopStyleVar();

    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}