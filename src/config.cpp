#include "config.h"

#include <array>
#include <filesystem>
#include <windows.h>

Config Config::load() {
    Config                        cfg;
    std::array<wchar_t, MAX_PATH> exe_path = {};
    GetModuleFileNameW(nullptr, exe_path.data(), static_cast<UINT>(exe_path.size()));
    auto ini_path = std::filesystem::path(exe_path.data()).parent_path() / L"vim-anywhere-for-windows.ini";

    std::array<wchar_t, 1024> buf = {};
    buf[0]                        = L'\0';
    GetPrivateProfileStringW(L"editor", L"path", L"", buf.data(), static_cast<UINT>(buf.size()), ini_path.c_str());
    cfg.editor_path = buf.data();

    buf[0] = L'\0';
    GetPrivateProfileStringW(L"editor", L"args", L"", buf.data(), static_cast<UINT>(buf.size()), ini_path.c_str());
    cfg.editor_args = buf.data();

    return cfg;
}
