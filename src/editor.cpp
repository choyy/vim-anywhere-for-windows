#include "editor.h"

#include <array>
#include <vector>
#include <windows.h>

std::optional<std::wstring> find_editor() {
    std::array<wchar_t, MAX_PATH> buf = {};
    if (SearchPathW(nullptr, L"gvim", L".exe", MAX_PATH, buf.data(), nullptr) > 0) {
        return std::wstring(buf.data());
    }

    std::vector<std::wstring> roots = {
        LR"(C:\Program Files\Vim)",
        LR"(C:\Program Files (x86)\Vim)",
    };
    for (const auto &root : roots) {
        std::wstring     pattern = root + L"\\vim*";
        WIN32_FIND_DATAW fd;
        HANDLE           h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        while (true) {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                std::wstring path = root + L"\\" + fd.cFileName + L"\\gvim.exe"; // NOLINT(hicpp-no-array-decay, cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    FindClose(h);
                    return path;
                }
            }
            if (FindNextFileW(h, &fd) == FALSE) { break; }
        }
        FindClose(h);
    }

    std::vector<std::wstring> nvim_paths = {
        LR"(C:\Program Files\Neovim\bin\nvim-qt.exe)",
        LR"(C:\Program Files (x86)\Neovim\bin\nvim-qt.exe)",
    };
    for (const auto &path : nvim_paths) {
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }

    return std::nullopt;
}

bool launch_and_wait(const std::wstring &editor_path, const std::wstring &args, const std::wstring &filepath) {
    std::wstring        cmd = L"\"" + editor_path + L"\" " + args + L" \"" + filepath + L"\"";
    STARTUPINFOW        si  = {sizeof(si)};
    PROCESS_INFORMATION pi  = {};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) == FALSE) {
        return false;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    return true;
}
