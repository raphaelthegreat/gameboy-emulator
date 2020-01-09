#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

enum Flags { NewFile = 1 };
enum sort { UNSORTED, SORT_DOWN, SORT_UP };

struct Root {
    std::string root;
    std::string label;
};

struct File {
    std::string filename;
    std::uintmax_t size;
    std::string dateTime;
    std::time_t dateTimeTimeT;
    
    bool selected;
};

struct Sorter {
public:
    bool operator()(const File& a, const File& b) const;

public:
    sort name = SORT_DOWN;
    sort size = UNSORTED;
    sort date = UNSORTED;
};

class FileDialog {
public:
    FileDialog(uint64_t flags = 0);
    
    void set_to_current_path();
    void open_dialog();
    
    auto selected() const { return selected_dirs; }
    bool draw();
    
public:
    std::filesystem::path current_path;

private:
    void fill_roots();
    void nuke_cache();

private:
    bool cache_dirty = true;
    uint64_t flags;
    std::string title;
    
    std::vector<Root> roots;
    std::vector<std::string> directories;
    
    std::vector<File> files;
    std::vector<std::string> selected_dirs;
    
    std::filesystem::space_info space_info;
    std::string new_file;

    Sorter sorter;
};