#pragma once
#include <optional>
#include <string>

std::optional<std::wstring> find_editor();
bool                        launch_and_wait(const std::wstring &editor_path, const std::wstring &args, const std::wstring &filepath);
