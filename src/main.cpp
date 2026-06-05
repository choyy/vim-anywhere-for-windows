#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

#include <shellapi.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "editor.h"

#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY 1
#define ID_HOTKEY 1
#define IDM_AUTOSTART 99
#define IDM_EXIT 100
#define IDI_ICON 101

namespace {

bool is_autostart() {
    HKEY                          hKey = nullptr;
    std::array<wchar_t, MAX_PATH> buf{};
    DWORD                         size = sizeof(buf);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = RegQueryValueExW(hKey, L"VimAnywhereForWindows", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf.data()), &size) == ERROR_SUCCESS; // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    RegCloseKey(hKey);
    return ok;
}

void set_autostart(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return;
    }
    if (enable) {
        std::array<wchar_t, MAX_PATH> exe{};
        GetModuleFileNameW(nullptr, exe.data(), static_cast<UINT>(exe.size()));
        RegSetValueExW(hKey, L"VimAnywhereForWindows", 0, REG_SZ, reinterpret_cast<const BYTE *>(exe.data()), static_cast<DWORD>((wcslen(exe.data()) + 1) * sizeof(wchar_t))); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    } else {
        RegDeleteValueW(hKey, L"VimAnywhereForWindows");
    }
    RegCloseKey(hKey);
}

std::filesystem::path create_temp_file() {
    std::array<wchar_t, MAX_PATH> dir{};
    std::array<wchar_t, MAX_PATH> name{};
    GetTempPathW(static_cast<UINT>(dir.size()), dir.data());
    GetTempFileNameW(dir.data(), L"ve", 0, name.data());
    return std::filesystem::path(name.data());
}

std::string to_utf8(const std::wstring &wstr) {
    int               len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::vector<char> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), utf8.data(), len, nullptr, nullptr);
    return {utf8.data(), utf8.size()};
}

std::wstring from_utf8(const std::string &str) {
    int          wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), result.data(), wlen);
    return result;
}

void write_file(const std::filesystem::path &path, const std::wstring &text) {
    auto          utf8 = to_utf8(text);
    std::ofstream f(path, std::ios::binary);
    if (!f) { return; }
    constexpr std::array<char, 3> bom = {'\xEF', '\xBB', '\xBF'};
    f.write(bom.data(), 3);
    f.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

std::wstring read_file(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate); // NOLINT(hicpp-signed-bitwise)
    if (!f) { return {}; }
    auto size = f.tellg();
    if (size <= 0) { return {}; }
    f.seekg(0);
    std::string buf(static_cast<size_t>(size), '\0');
    f.read(buf.data(), size);
    int offset = 0;
    if (size >= 3 && static_cast<unsigned char>(buf[0]) == 0xEF && static_cast<unsigned char>(buf[1]) == 0xBB && static_cast<unsigned char>(buf[2]) == 0xBF) {
        offset = 3;
    }
    return from_utf8(buf.substr(offset));
}

void send_key(WORD vk, bool down) {
    INPUT in  = {INPUT_KEYBOARD};
    in.ki.wVk = vk;                                 // NOLINT(cppcoreguidelines-pro-type-union-access)
    if (!down) { in.ki.dwFlags = KEYEVENTF_KEYUP; } // NOLINT(cppcoreguidelines-pro-type-union-access)
    SendInput(1, &in, sizeof(INPUT));
}

void release_modifier(WORD vk) {
    if ((static_cast<USHORT>(GetAsyncKeyState(vk)) & 0x8000u) != 0) {
        send_key(vk, false);
        Sleep(20);
    }
}

void simulate_copy() {
    release_modifier(VK_MENU);
    send_key(VK_CONTROL, true);
    Sleep(30);
    send_key('C', true);
    Sleep(10);
    send_key('C', false);
    Sleep(30);
    send_key(VK_CONTROL, false);
}

void simulate_paste() {
    release_modifier(VK_MENU);
    send_key(VK_CONTROL, true);
    Sleep(30);
    send_key('V', true);
    Sleep(10);
    send_key('V', false);
    Sleep(30);
    send_key(VK_CONTROL, false);
}

bool focus_window(HWND hwnd) {
    if (hwnd == nullptr || IsWindow(hwnd) == FALSE) { return false; }
    if (GetForegroundWindow() == hwnd) { return true; }
    DWORD target  = GetWindowThreadProcessId(hwnd, nullptr);
    DWORD current = GetCurrentThreadId();
    AttachThreadInput(current, target, TRUE);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    AttachThreadInput(current, target, FALSE);
    return GetForegroundWindow() == hwnd;
}

std::wstring read_clipboard() {
    std::wstring result;
    if (OpenClipboard(nullptr) != FALSE) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h != nullptr) {
            const auto *p = static_cast<const wchar_t *>(GlobalLock(h));
            if (p != nullptr) {
                result = p;
                GlobalUnlock(h);
            }
        }
        CloseClipboard();
    }
    return result;
}

void write_clipboard(const std::wstring &text) {
    if (OpenClipboard(nullptr) != FALSE) {
        EmptyClipboard();
        SIZE_T size = (text.size() + 1) * sizeof(wchar_t);
        HANDLE h    = GlobalAlloc(GMEM_MOVEABLE, size);
        if (h != nullptr) {
            auto *p = static_cast<wchar_t *>(GlobalLock(h));
            if (p != nullptr) {
                wcscpy_s(p, text.size() + 1, text.c_str());
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
            } else {
                GlobalFree(h);
            }
        }
        CloseClipboard();
    }
}

void run() {
    Sleep(50);

    HWND hwnd = GetForegroundWindow();
    if (hwnd == nullptr) { return; }

    auto saved = read_clipboard();
    write_clipboard(L"");
    Sleep(50);

    simulate_copy();
    Sleep(100);

    std::wstring text = read_clipboard();
    if (text.empty()) {
        write_clipboard(saved);
        return;
    }

    auto temp = create_temp_file();
    write_file(temp, text);

    auto editor = find_editor();
    if (!editor) {
        MessageBoxW(nullptr, L"gvim not found.\n\nInstall Vim.", L"Vim Anywhere for Windows", MB_OK | MB_ICONERROR);
        write_clipboard(saved);
        return;
    }

    if (!launch_and_wait(*editor, L"", temp.wstring())) {
        MessageBoxW(nullptr, L"Failed to launch editor.", L"Vim Anywhere for Windows", MB_OK | MB_ICONERROR);
        write_clipboard(saved);
        return;
    }

    std::wstring new_text = read_file(temp);
    DeleteFileW(temp.c_str());

    focus_window(hwnd);
    Sleep(50);

    write_clipboard(new_text);
    simulate_paste();
    Sleep(50);

    write_clipboard(saved);
}

void show_tray_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (is_autostart() ? MF_CHECKED : MF_UNCHECKED), IDM_AUTOSTART, L"开机自启");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_HOTKEY:
        run();
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            show_tray_menu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_AUTOSTART) {
            set_autostart(!is_autostart());
        }
        if (LOWORD(wParam) == IDM_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        set_autostart(false);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) { // NOLINT(readability-non-const-parameter)
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"VimAnywhereForWindowsMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Vim Anywhere for Windows is already running.", L"Vim Anywhere for Windows", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    HWND            hwnd = nullptr;
    NOTIFYICONDATAW nid  = {sizeof(NOTIFYICONDATAW)};

    auto fail = [&](LPCWSTR msg, int code) {
        MessageBoxW(nullptr, msg, L"Vim Anywhere for Windows", MB_OK | MB_ICONERROR);
        if (nid.uFlags & NIF_MESSAGE) { Shell_NotifyIconW(NIM_DELETE, &nid); }
        if (hwnd != nullptr) { DestroyWindow(hwnd); }
        if (hMutex != nullptr) { CloseHandle(hMutex); }
        return code;
    };

    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"VimAnywhereForWindowsClass";
    if (RegisterClassW(&wc) == 0) {
        return fail(L"Failed to register window class.", 1);
    }

    hwnd = CreateWindowW(L"VimAnywhereForWindowsClass", L"Vim Anywhere for Windows", 0,
                         0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    if (hwnd == nullptr) {
        return fail(L"Failed to create window.", 1);
    }

    if (RegisterHotKey(hwnd, ID_HOTKEY, MOD_CONTROL | MOD_ALT, 'E') == FALSE) {
        return fail(L"Failed to register hotkey Ctrl+Alt+E.\nIt may be in use by another application.", 1);
    }

    nid.cbSize           = sizeof(NOTIFYICONDATAW);
    nid.hWnd             = hwnd;
    nid.uID              = ID_TRAY;
    nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_ICON));
    wcsncpy_s(nid.szTip, L"Vim Anywhere for Windows (Ctrl+Alt+E)", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Shell_NotifyIconW(NIM_DELETE, &nid);
    UnregisterHotKey(hwnd, ID_HOTKEY);
    CloseHandle(hMutex);

    return 0;
}
