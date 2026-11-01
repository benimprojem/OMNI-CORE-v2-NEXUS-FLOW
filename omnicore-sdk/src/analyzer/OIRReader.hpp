// ============================================================
// OIRReader.hpp — Defensive OIR v2 Binary Reader
// ============================================================
// Reads the existing OIR2 / version 0x0002 layout emitted by
// OIRWriter.hpp. This reader validates the complete container
// before publishing symbols or instructions to later stages.
//
// OIR v2 layout:
//   Header: 48 bytes
//   Symbol: id(4), prefix(1), typeId(4), flags(2), nameLen(2), name(N)
//   Opcode: op(1), type(1), symbolId(4), operandId(4), line(4), col(2)
// All integer fields are little-endian.

#ifndef NEXUS_OIR_READER_HPP
#define NEXUS_OIR_READER_HPP

#include "../parser/OIRWriter.hpp"
#include "../parser/OIRError.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <utility>

struct OIRPayload {
    uint32_t instrIndex;
    std::string data;
};

class OIRReader {
    static constexpr uint32_t HEADER_SIZE = 48;
    static constexpr uint32_t INSTRUCTION_SIZE = 16;
    static constexpr uint16_t SUPPORTED_VERSION = 0x0002;
    static constexpr uint16_t MAX_SYMBOL_NAME = 1024;

    std::string filename;
    ErrorList& errors;

public:
    std::string targetId;
    uint16_t mode = 0;
    std::vector<OIRSymbol> symbols;
    std::vector<OIRPayload> payloads;
    std::vector<OIRInstruction> instructions;

    OIRReader(const std::string& file, ErrorList& errs)
        : filename(file), errors(errs) {}

    bool load() {
        // A Reader instance may be reused. Never expose stale or partial data.
        targetId.clear();
        mode = 0;
        symbols.clear();
        payloads.clear();
        instructions.clear();

        std::ifstream f(filename, std::ios::binary);
        if (!f) {
            fail("A0001", "OIR dosyasi acilamadi: " + filename);
            return false;
        }

        f.seekg(0, std::ios::end);
        const std::streamoff endPos = f.tellg();
        if (endPos < 0 || static_cast<uint64_t>(endPos) < HEADER_SIZE ||
            static_cast<uint64_t>(endPos) > std::numeric_limits<uint32_t>::max()) {
            fail("A0003", "OIR dosya boyutu gecersiz veya desteklenen adres araliginin disinda.");
            return false;
        }
        const uint32_t fileSize = static_cast<uint32_t>(endPos);
        f.seekg(0, std::ios::beg);

        char magic[4] = {};
        if (!readExact(f, magic, sizeof(magic)) || std::memcmp(magic, "OIR2", 4) != 0) {
            fail("A0002", "Gecersiz veya bozuk OIR dosyasi (Magic uyusmuyor).");
            return false;
        }

        uint16_t version = 0, parsedMode = 0;
        if (!readU16(f, version) || !readU16(f, parsedMode)) {
            fail("A0003", "OIR header eksik veya kesilmis.");
            return false;
        }
        if (version != SUPPORTED_VERSION) {
            fail("A0004", "Desteklenmeyen OIR surumu: " + std::to_string(version));
            return false;
        }
        if (parsedMode > 1) {
            fail("A0005", "Gecersiz OIR mode degeri.");
            return false;
        }

        char targetBuffer[32] = {};
        if (!readExact(f, targetBuffer, sizeof(targetBuffer))) {
            fail("A0003", "OIR target alani eksik.");
            return false;
        }
        const void* terminator = std::memchr(targetBuffer, '\0', sizeof(targetBuffer));
        if (!terminator) {
            fail("A0005", "OIR target alani null-terminator icermiyor.");
            return false;
        }
        const std::string parsedTarget(targetBuffer,
            static_cast<const char*>(terminator) - targetBuffer);

        uint32_t symbolOffset = 0, codeOffset = 0;
        if (!readU32(f, symbolOffset) || !readU32(f, codeOffset)) {
            fail("A0003", "OIR offset alanlari eksik.");
            return false;
        }

        if (symbolOffset != HEADER_SIZE || codeOffset < symbolOffset ||
            codeOffset > fileSize) {
            fail("A0006", "OIR symbol/code offset degerleri dosya sinirlariyla uyusmuyor.");
            return false;
        }

        const uint32_t codeBytes = fileSize - codeOffset;
        if ((codeBytes % INSTRUCTION_SIZE) != 0) {
            fail("A0007", "OIR opcode stream eksik veya bozuk bir talimat kaydi iceriyor.");
            return false;
        }

        std::vector<OIRSymbol> parsedSymbols;
        std::vector<OIRInstruction> parsedInstructions;
        std::unordered_set<uint32_t> symbolIds;

        // Symbol table: parse only within [symbolOffset, codeOffset).
        f.seekg(symbolOffset, std::ios::beg);
        while (true) {
            const std::streamoff recordStart = f.tellg();
            if (recordStart < 0) {
                fail("A0008", "OIR symbol table konumu okunamadi.");
                return false;
            }
            if (recordStart == static_cast<std::streamoff>(codeOffset)) break;
            if (recordStart > static_cast<std::streamoff>(codeOffset) ||
                static_cast<uint64_t>(recordStart) + 13 > codeOffset) {
                fail("A0008", "OIR symbol kaydi kesilmis.");
                return false;
            }

            OIRSymbol symbol{};
            uint16_t nameLength = 0;
            if (!readU32(f, symbol.id) ||
                !readExact(f, reinterpret_cast<char*>(&symbol.prefix), 1) ||
                !readU32(f, symbol.typeId) ||
                !readU16(f, symbol.flags) ||
                !readU16(f, nameLength)) {
                fail("A0008", "OIR symbol header okunamadi.");
                return false;
            }

            const std::streamoff afterHeader = f.tellg();
            if (afterHeader < 0 || nameLength > MAX_SYMBOL_NAME ||
                static_cast<uint64_t>(afterHeader) + nameLength > codeOffset) {
                fail("A0009", "OIR symbol name uzunlugu gecersiz veya kayit sinirini asiyor.");
                return false;
            }
            if (symbol.id == 0 || !symbolIds.insert(symbol.id).second) {
                fail("A0010", "OIR symbol ID sifir veya tekrar ediyor: " + std::to_string(symbol.id));
                return false;
            }
            if (symbol.prefix < SymPrefix::VAR || symbol.prefix > SymPrefix::EXTERN) {
                fail("A0011", "OIR symbol prefix degeri taninmiyor.");
                return false;
            }

            symbol.name.assign(nameLength, '\0');
            if (nameLength != 0 && !readExact(f, &symbol.name[0], nameLength)) {
                fail("A0008", "OIR symbol name alani kesilmis.");
                return false;
            }
            parsedSymbols.push_back(std::move(symbol));
        }

        const std::streamoff symbolTableEnd = f.tellg();
        if (symbolTableEnd < 0 ||
            symbolTableEnd != static_cast<std::streamoff>(codeOffset)) {
            fail("A0008", "OIR symbol table code offset ile tam olarak sonlanmiyor.");
            return false;
        }

        // Fixed-width opcode records. Decode every field explicitly as LE.
        f.seekg(codeOffset, std::ios::beg);
        const uint32_t instructionCount = codeBytes / INSTRUCTION_SIZE;
        parsedInstructions.reserve(instructionCount);
        for (uint32_t index = 0; index < instructionCount; ++index) {
            OIRInstruction instruction{};
            if (!readExact(f, reinterpret_cast<char*>(&instruction.op), 1) ||
                !readExact(f, reinterpret_cast<char*>(&instruction.typeId), 1) ||
                !readU32(f, instruction.symbolId) ||
                !readU32(f, instruction.operandId) ||
                !readU32(f, instruction.line) ||
                !readU16(f, instruction.col)) {
                fail("A0007", "OIR opcode kaydi okunamadi veya kesilmis.");
                return false;
            }
            parsedInstructions.push_back(std::move(instruction));
        }

        // Publish only after the entire file has passed structural validation.
        targetId = parsedTarget;
        mode = parsedMode;
        symbols.swap(parsedSymbols);
        instructions.swap(parsedInstructions);
        return true;
    }

    const OIRSymbol* getSymbol(uint32_t id) const {
        for (const auto& symbol : symbols) {
            if (symbol.id == id) return &symbol;
        }
        return nullptr;
    }

private:
    void fail(const std::string& code, const std::string& message) {
        errors.error(code, message, filename, 0, 0);
    }

    static bool readExact(std::ifstream& stream, char* destination, std::streamsize size) {
        return size == 0 || static_cast<bool>(stream.read(destination, size));
    }

    static bool readU16(std::ifstream& stream, uint16_t& value) {
        uint8_t bytes[2] = {};
        if (!readExact(stream, reinterpret_cast<char*>(bytes), 2)) return false;
        value = static_cast<uint16_t>(bytes[0]) |
                (static_cast<uint16_t>(bytes[1]) << 8);
        return true;
    }

    static bool readU32(std::ifstream& stream, uint32_t& value) {
        uint8_t bytes[4] = {};
        if (!readExact(stream, reinterpret_cast<char*>(bytes), 4)) return false;
        value = static_cast<uint32_t>(bytes[0]) |
                (static_cast<uint32_t>(bytes[1]) << 8) |
                (static_cast<uint32_t>(bytes[2]) << 16) |
                (static_cast<uint32_t>(bytes[3]) << 24);
        return true;
    }
};

#endif // NEXUS_OIR_READER_HPP
