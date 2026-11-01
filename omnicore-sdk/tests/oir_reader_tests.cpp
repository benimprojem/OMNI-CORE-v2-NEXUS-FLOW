#include "../src/analyzer/OIRReader.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static std::vector<char> readBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(file),
                             std::istreambuf_iterator<char>());
}

static void writeBytes(const std::string& path, const std::vector<char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    assert(file.good());
}

static bool rejects(const std::string& path) {
    ErrorList errors;
    OIRReader reader(path, errors);
    const bool loaded = reader.load();
    return !loaded && errors.hasErrors() && reader.instructions.empty() &&
           reader.symbols.empty();
}

int main() {
    const std::string validPath = "oir_reader_valid.oir";
    const std::string badVersionPath = "oir_reader_bad_version.oir";
    const std::string truncatedPath = "oir_reader_truncated.oir";
    const std::string badOffsetPath = "oir_reader_bad_offset.oir";
    const std::string badTargetPath = "oir_reader_bad_target.oir";

    // Valid round-trip: Writer -> file -> Reader.
    {
        ErrorList errors;
        OIRWriter writer("Win64", errors, false, validPath);
        const uint32_t symbolId = writer.addSymbol("sample", SymPrefix::VAR,
                                                   SymFlags::MUTABLE);
        assert(symbolId == 1);
        writer.emit(OIROp::DEF_VAR, "", symbolId, 7, 3);
        assert(writer.save());
        assert(!errors.hasErrors());
    }

    {
        ErrorList errors;
        OIRReader reader(validPath, errors);
        assert(reader.load());
        assert(!errors.hasErrors());
        assert(reader.targetId == "Win64");
        assert(reader.mode == 0);
        assert(reader.symbols.size() == 1);
        assert(reader.instructions.size() == 1);
        assert(reader.getSymbol(1) != nullptr);
        assert(reader.getSymbol(1)->name == "sample");
        assert(reader.instructions[0].op == OIROp::DEF_VAR);
        assert(reader.instructions[0].symbolId == 1);
        assert(reader.instructions[0].line == 7);
        assert(reader.instructions[0].col == 3);
    }

    const std::vector<char> valid = readBytes(validPath);
    assert(valid.size() >= 48 + 16);

    // Unsupported version must be rejected.
    {
        std::vector<char> bytes = valid;
        bytes[4] = 0x7F;
        bytes[5] = 0x7F;
        writeBytes(badVersionPath, bytes);
        assert(rejects(badVersionPath));
    }

    // A partial fixed-width instruction must be rejected.
    {
        std::vector<char> bytes = valid;
        bytes.pop_back();
        writeBytes(truncatedPath, bytes);
        assert(rejects(truncatedPath));
    }

    // codeOffset beyond EOF must be rejected.
    {
        std::vector<char> bytes = valid;
        const uint32_t badOffset = static_cast<uint32_t>(bytes.size() + 1);
        bytes[44] = static_cast<char>(badOffset & 0xFF);
        bytes[45] = static_cast<char>((badOffset >> 8) & 0xFF);
        bytes[46] = static_cast<char>((badOffset >> 16) & 0xFF);
        bytes[47] = static_cast<char>((badOffset >> 24) & 0xFF);
        writeBytes(badOffsetPath, bytes);
        assert(rejects(badOffsetPath));
    }

    // Target field must contain a NUL terminator within its 32-byte field.
    {
        std::vector<char> bytes = valid;
        for (size_t i = 8; i < 40; ++i) bytes[i] = 'X';
        writeBytes(badTargetPath, bytes);
        assert(rejects(badTargetPath));
    }

    std::remove(validPath.c_str());
    std::remove(badVersionPath.c_str());
    std::remove(truncatedPath.c_str());
    std::remove(badOffsetPath.c_str());
    std::remove(badTargetPath.c_str());

    return 0;
}
