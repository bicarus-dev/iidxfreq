#include "module.h"

#include <array>
#include <fstream>
#include <sstream>

namespace iidxfreq {

Result<std::filesystem::path> module_path(HMODULE module) {
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) {
        return Error{"Cannot resolve module path."};
    }
    return std::filesystem::path(path.data());
}

Result<std::string> pe_identifier(const std::filesystem::path& path, std::string_view model) {
    std::ifstream input(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!input.read(reinterpret_cast<char*>(&dos), sizeof(dos)) ||
        dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos))) {
        return Error{"Cannot read a valid DOS header from the game DLL."};
    }
    input.seekg(dos.e_lfanew);
    IMAGE_NT_HEADERS64 nt{};
    if (!input.read(reinterpret_cast<char*>(&nt), sizeof(nt)) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return Error{"Cannot read a valid x64 PE header from the game DLL."};
    }
    // Match Spice's patch identifier: model, PE timestamp, and entry-point RVA (not ASLR base).
    std::ostringstream identifier;
    identifier << model << '-' << std::hex << nt.FileHeader.TimeDateStamp << '_'
               << nt.OptionalHeader.AddressOfEntryPoint;
    return identifier.str();
}

}