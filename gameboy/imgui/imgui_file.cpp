#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <malloc.h>
#include <stdio.h>
#endif

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <imgui.h>
#include <imgui_file.h>

struct InputTextCallback_UserData {
    std::string*          Str;
    ImGuiInputTextCallback  ChainCallback;
    void*                   ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        std::string* str = user_data->Str;
        IM_ASSERT(data->Buf == reinterpret_cast<const char *>(str->c_str()));
        
        str->resize(data->BufTextLen);       
        data->Buf = (char*)str->c_str();
    }
    else if (user_data->ChainCallback) {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    
    return 0;
}

static bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    
    return ImGui::InputText(label, const_cast<char*>(reinterpret_cast<const char*>(str->c_str())), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

bool Sorter::operator()(const File& a, const File& b) const 
{
    switch (name) {
    
    case SORT_DOWN:
        return a.filename < b.filename;
    case SORT_UP:
        return a.filename > b.filename;
    }

    switch (size) {
    case SORT_DOWN:
        return a.size < b.size;
    case SORT_UP:
        return a.size > b.size;
    }

    switch (date) {
    case SORT_DOWN:
        return a.dateTimeTimeT < b.dateTimeTimeT;
    case SORT_UP:
        return a.dateTimeTimeT > b.dateTimeTimeT;
    }

    return false;
}

FileDialog::FileDialog(uint64_t flags) : title("File Dialog"), flags(flags) 
{
    set_to_current_path();
}

#ifdef _WIN32
void FileDialog::fill_roots() 
{
    DWORD drivesMask = GetLogicalDrives();
    
    for (TCHAR drive = 'A'; drive <= 'Z'; drive++, drivesMask >>= 1) {
        if (!(drivesMask & 1)) continue;
        
        BOOL success = FALSE;
        TCHAR rootPath[4];
        TCHAR volumeName[MAX_PATH + 1];
        
        rootPath[0] = drive;
        rootPath[1] = ':';
        rootPath[2] = '\\';
        rootPath[3] = '\0';
        
        success = GetVolumeInformation(rootPath, volumeName, MAX_PATH + 1, NULL, NULL, NULL, NULL, 0);
        if (!success) continue;

#ifdef UNICODE
        int needed;
        LPSTR str;

        needed = WideCharToMultiByte(CP_UTF8, 0, rootPath, -1, NULL, 0, NULL, NULL);
        if (needed <= 0) continue;
        
        str = (LPSTR)_malloca(needed);
        WideCharToMultiByte(CP_UTF8, 0, rootPath, -1, str, needed, NULL, NULL);
        
        std::string root = str;
        _freea(str);

        needed = WideCharToMultiByte(CP_UTF8, 0, volumeName, -1, NULL, 0, NULL, NULL);
        
        if (needed <= 0) continue;
        str = (LPSTR)_malloca(needed);
        
        WideCharToMultiByte(CP_UTF8, 0, volumeName, -1, str, needed, NULL, NULL);
        std::string label = root + " (" + str + ")";
        _freea(str);
#else
        std::string root = rootName;
        std::string label = root + " (" + volumeName + ")";
#endif
        roots.push_back({root, label});
    }
}
#else
void Widgets::FileDialog::fill_roots() { roots.push_back({MAKEU8(u8"/"), "(root)"}); }
#endif

void FileDialog::set_to_current_path() 
{
    current_path = std::filesystem::current_path();
    nuke_cache();
}

void FileDialog::open_dialog() 
{
    ImGui::OpenPopup(title.c_str());
    
    selected_dirs.clear();
    sorter.name = SORT_DOWN;
    sorter.size = UNSORTED;
    sorter.date = UNSORTED;
    
    new_file = std::string(u8"");
}

bool FileDialog::draw() 
{
    bool done = false;
    
    if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        if (cache_dirty) {
            
            fill_roots();
            
            try {
                if (!std::filesystem::exists(current_path)) set_to_current_path();
                if (!std::filesystem::is_directory(current_path)) set_to_current_path();
            } catch (...) {
                set_to_current_path();
            }
            
            space_info = std::filesystem::space(current_path);
            
            for (auto& p : std::filesystem::directory_iterator(current_path)) {
                
                if (p.is_directory()) {
                    directories.push_back(p.path().filename().u8string());
                } 
                else {               
                    try {
                        auto status = p.status();
                        auto lastWrite = std::filesystem::last_write_time(p);
                        
                        auto timeSinceEpoch = lastWrite.time_since_epoch();
                        auto count = timeSinceEpoch.count();
                        
                        std::time_t dateTime = count / 100000000;
                        const std::tm* converted = std::localtime(&dateTime);
                        
                        std::ostringstream formatted;
                        formatted << std::put_time(converted, "%c");
                        
                        files.push_back(
                            {p.path().filename().u8string(), std::filesystem::file_size(p), formatted.str(), dateTime});
                    } 
                    catch (...) { ; }
                }
            }
            
            if (sorter.name != UNSORTED || sorter.size != UNSORTED || sorter.date != UNSORTED) {
                std::sort(files.begin(), files.end(), sorter);
            }
            
            std::sort(directories.begin(), directories.end());
            cache_dirty = false;
        }

        bool goHome = false;
        bool goUp = false;
        
        std::string goDown = std::string(u8"");
        File* selected = nullptr;

        if (ImGui::Button("Home")) goHome = true;
        ImGui::SameLine();
        
        ImGui::Text(reinterpret_cast<const char*>(current_path.u8string().c_str()));      
        {
            ImGui::BeginChild("Directories", ImVec2(250, 350), true, ImGuiWindowFlags_HorizontalScrollbar);
            
            if (ImGui::TreeNode(("Roots"))) {
                for (auto& p : roots) {
                    if (ImGui::Selectable(p.label.c_str(), false, 0, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                        goDown = p.root;
                    }
                }
                
                ImGui::TreePop();
            }
            
            if (ImGui::TreeNodeEx(("Directories"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Selectable("..", false, 0, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                    goUp = true;
                }
                for (auto& p : directories) {
                    if (ImGui::Selectable(reinterpret_cast<const char*>(p.c_str()), false, 0, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                        goDown = p;
                    }
                }
                
                ImGui::TreePop();
            }
            
            ImGui::EndChild();
        }
        
        ImGui::SameLine();
        {
            std::string header;
            
            ImGui::BeginChild(("Files"), ImVec2(500, 350), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::Columns(3);
            
            switch (sorter.name) {
                case UNSORTED:
                    header = ("  File");
                    break;
                case SORT_DOWN:
                    header = ("v File");
                    break;
                case SORT_UP:
                    header = ("^ File");
                    break;
            }
            
            if (ImGui::Selectable(header.c_str())) {
                switch (sorter.name) {
                    case UNSORTED:
                        sorter.name = SORT_DOWN;
                        break;
                    case SORT_DOWN:
                        sorter.name = SORT_UP;
                        break;
                    case SORT_UP:
                        sorter.name = UNSORTED;
                        break;
                }
                sorter.size = UNSORTED;
                sorter.date = UNSORTED;
                nuke_cache();
            }
            
            ImGui::NextColumn();
            switch (sorter.size) {
                case UNSORTED:
                    header = ("  Size");
                    break;
                case SORT_DOWN:
                    header = ("v Size");
                    break;
                case SORT_UP:
                    header = ("^ Size");
                    break;
            }
            
            if (ImGui::Selectable(header.c_str())) {
                switch (sorter.size) {
                    case UNSORTED:
                        sorter.size = SORT_DOWN;
                        break;
                    case SORT_DOWN:
                        sorter.size = SORT_UP;
                        break;
                    case SORT_UP:
                        sorter.size = UNSORTED;
                        break;
                }
                sorter.name = UNSORTED;
                sorter.date = UNSORTED;
                nuke_cache();
            }
            
            ImGui::NextColumn();
            switch (sorter.date) {
                case UNSORTED:
                    header = ("  Date & Time");
                    break;
                case SORT_DOWN:
                    header = ("v Date & Time");
                    break;
                case SORT_UP:
                    header = ("^ Date & Time");
                    break;
            }
            
            if (ImGui::Selectable(header.c_str())) {
                switch (sorter.date) {
                    case UNSORTED:
                        sorter.date = SORT_DOWN;
                        break;
                    case SORT_DOWN:
                        sorter.date = SORT_UP;
                        break;
                    case SORT_UP:
                        sorter.date = UNSORTED;
                        break;
                }
                sorter.name = UNSORTED;
                sorter.size = UNSORTED;
                nuke_cache();
            }
            ImGui::NextColumn();
            ImGui::Separator();

            for (auto& p : files) {
                std::string label = std::string(u8"##") + p.filename;
                if (ImGui::Selectable(reinterpret_cast<const char *>(label.c_str()), p.selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    for (auto& f : files) f.selected = false;
                    p.selected = true;
                    if (flags & NewFile) {
                        new_file = std::filesystem::path(p.filename).filename().u8string();
                    }
                }
                ImGui::SameLine();
                ImGui::Text(reinterpret_cast<const char*>(p.filename.c_str()));
                ImGui::NextColumn();
                ImGui::Text(std::to_string(p.size).c_str());
                ImGui::NextColumn();
                ImGui::Text(p.dateTime.c_str());
                ImGui::NextColumn();

                if (p.selected) selected = &p;
            }

            ImGui::EndChild();
        }
        std::string selectedStr;
        bool gotSelected = selected;
        if (flags & NewFile) {
            ImGui::Text(reinterpret_cast<const char*>(current_path.u8string().c_str()));
            ImGui::SameLine();
            std::string label = std::string("##") + title + "Filename";
            InputText(label.c_str(), &new_file);
            selectedStr = new_file;
            gotSelected = !new_file.empty();
        } else {
            selectedStr = (current_path / std::filesystem::path(selected ? selected->filename : std::string(u8"..."))).u8string();
            ImGui::Text(reinterpret_cast<const char*>(selectedStr.c_str()));
        }
        if (!gotSelected) {
            const ImVec4 lolight = ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
            ImGui::PushStyleColor(ImGuiCol_Button, lolight);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, lolight);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, lolight);
        }
        if (ImGui::Button(("OK"), ImVec2(120, 30)) && gotSelected) {
            selected_dirs.clear();
            selected_dirs.push_back(selectedStr);
            ImGui::CloseCurrentPopup();
            done = true;
        }
        if (!gotSelected) ImGui::PopStyleColor(3);
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button(("Cancel"), ImVec2(120, 30))) {
            ImGui::CloseCurrentPopup();
            selected_dirs.clear();
            done = true;
        }
        ImGui::EndPopup();

        if (goUp) {
            current_path = current_path.parent_path();
            nuke_cache();
        } else if (!goDown.empty()) {
            current_path = current_path / goDown;
            nuke_cache();
        } else if (goHome) {
            set_to_current_path();
            nuke_cache();
        }
    }

    return done;
}

void FileDialog::nuke_cache() {
    cache_dirty = true;
    roots.clear();
    
    directories.clear();
    files.clear();
}