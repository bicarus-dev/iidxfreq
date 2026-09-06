#pragma once

#include "result.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace iidxfreq {

Result<std::filesystem::path> module_path(HMODULE module);
Result<std::string> pe_identifier(const std::filesystem::path& path, std::string_view model);

}