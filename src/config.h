#pragma once
#include <string>

struct Config {
    std::wstring editor_path;
    std::wstring editor_args;

    static Config load();
};
