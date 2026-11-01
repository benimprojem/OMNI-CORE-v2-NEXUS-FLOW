// ============================================================
// OIRWriter.hpp — NexusFlow OIR Binary / Trace Emitter
// ============================================================
/*
 * MODÜL: OIRWriter
 * GÖREV: Parser tarafından üretilen operatörleri ve sembolleri bellek içinde
 *        tutar ve diske (binary/trace) kaydeder.
 * 
 * API:
 * - OIRWriter(targetId, errors, debug, path): Constructor.
 * - save(): Dosyayı ön tanımlı yola kaydeder.
 * - dump(): OIR içeriğini terminale yazdırır (trace eşleşmesi).
 */

#ifndef NEXUS_OIR_WRITER_HPP
#define NEXUS_OIR_WRITER_HPP

#include "OIRError.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <limits>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace OIROp {
    // Build / Target
    constexpr uint8_t SET_TARGET   = 0x01;
    constexpr uint8_t LINK_LIB     = 0x02;
    constexpr uint8_t USE_MOD      = 0x05;

    // Variables & Assignment
    constexpr uint8_t DEF_VAR      = 0x10;
    constexpr uint8_t CAPTURE      = 0x11;
    constexpr uint8_t RELOCATE     = 0x12;
    constexpr uint8_t ASSIGN       = 0x13;
    constexpr uint8_t PIPE         = 0x14;
    constexpr uint8_t IGNORE       = 0x15;
    constexpr uint8_t ZONE_WRITE   = 0x16;
    constexpr uint8_t FLOW_FEED    = 0x17;

    // Control Flow
    constexpr uint8_t RETURN_VOID  = 0x20;
    constexpr uint8_t RETURN_VAL   = 0x21;
    constexpr uint8_t BREAK_LOOP   = 0x22;
    constexpr uint8_t CONTINUE_LOP = 0x23;
    constexpr uint8_t COND_JUMP    = 0x24;
    constexpr uint8_t UNCOND_JUMP  = 0x25;
    constexpr uint8_t LABEL_DEF    = 0x26;
    constexpr uint8_t PANIC        = 0x27;
    constexpr uint8_t HALT_FLOW    = 0x28;
    constexpr uint8_t CLEANUP      = 0x29;

    // Functions
    constexpr uint8_t DEF_FUNC     = 0x30;
    constexpr uint8_t END_FUNC     = 0x31;
    constexpr uint8_t CALL         = 0x32;
    constexpr uint8_t DEF_EXTERN   = 0x33;
    constexpr uint8_t DEF_LAMBDA   = 0x34;
    constexpr uint8_t INTENT       = 0x35;

    // Structures / Rules
    constexpr uint8_t DEF_STRUCT   = 0x40;
    constexpr uint8_t DEF_ENUM     = 0x41;
    constexpr uint8_t DEF_UNION    = 0x42;
    constexpr uint8_t DEF_NEWTYPE  = 0x43;
    constexpr uint8_t END_TYPEDEF  = 0x44;
    constexpr uint8_t HARD_LOCK    = 0x45;
    constexpr uint8_t MAP_DEF      = 0x46;
    constexpr uint8_t APPLY_RULE   = 0x48;
    constexpr uint8_t ON_RULE      = 0x49;

    // Groups
    constexpr uint8_t DEF_GROUP      = 0x50;
    constexpr uint8_t GROUP_MEMBER   = 0x51;
    constexpr uint8_t METHOD_FULL    = 0x52;
    constexpr uint8_t METHOD_ONELINER= 0x53;

    // Blocks
    constexpr uint8_t IF_BEGIN     = 0x60;
    constexpr uint8_t ELSIF_BEGIN  = 0x61;
    constexpr uint8_t ELS_BEGIN    = 0x62;
    constexpr uint8_t IF_END       = 0x63;

    // Loops
    constexpr uint8_t LOOP_INF     = 0x70;
    constexpr uint8_t LOOP_RANGE   = 0x71;
    constexpr uint8_t LOOP_FOREACH = 0x72;
    constexpr uint8_t LOOP_FOR     = 0x73;
    constexpr uint8_t LOOP_WHILE   = 0x74;
    constexpr uint8_t LOOP_WHILE_S = 0x75;
    constexpr uint8_t LOOP_END     = 0x79;

    // Selection
    constexpr uint8_t SEL_BEGIN    = 0x80;
    constexpr uint8_t SEL_BRANCH   = 0x81;
    constexpr uint8_t SEL_END      = 0x82;
    constexpr uint8_t SCAN_BEGIN   = 0x85;
    constexpr uint8_t SCAN_PATTERN = 0x86;
    constexpr uint8_t SCAN_END     = 0x87;

    // Flow
    constexpr uint8_t ROLLING_RETRY = 0x90;
    constexpr uint8_t CATCH_ERR     = 0x91;
    constexpr uint8_t FALLBACK      = 0x92;

    // Misc
    constexpr uint8_t EXPR         = 0xA0;
    constexpr uint8_t ASM_BLOCK    = 0xB0;
    constexpr uint8_t ASM_LABEL    = 0xB1;
    constexpr uint8_t MACRO_CALL   = 0xB2;
}

namespace SymPrefix {
    constexpr uint8_t VAR    = 0x01;
    constexpr uint8_t CONST  = 0x02;
    constexpr uint8_t FUNC   = 0x03;
    constexpr uint8_t NT     = 0x04;
    constexpr uint8_t GROUP  = 0x05;
    constexpr uint8_t RULE   = 0x06;
    constexpr uint8_t EXTERN = 0x07;
}

namespace SymFlags {
    constexpr uint16_t NONE    = 0x0000;
    constexpr uint16_t MUTABLE = 0x0001;
    constexpr uint16_t PUBLIC  = 0x0002;
    constexpr uint16_t EXTERN  = 0x0004;
}

struct OIRSymbol {
    uint32_t    id;
    uint8_t     prefix;
    uint32_t    typeId = 0;
    uint16_t    flags;
    std::string name;
};

struct OIRInstruction {
    uint8_t     op;
    uint8_t     typeId;
    uint32_t    symbolId;
    uint32_t    operandId;
    uint32_t    line;
    uint16_t    col;
    std::string rawData;
};

class OIRWriter {
    static constexpr uint32_t HEADER_SIZE = 48;
    static constexpr uint32_t INSTRUCTION_SIZE = 16;
    static constexpr uint16_t MAX_SYMBOL_NAME = 1024;

    std::string target;
    int mode;
    std::string outPath;

    std::vector<OIRSymbol> symTable;
    std::vector<OIRInstruction> codeStream;
    ErrorList& errors;

public:
    OIRWriter(const std::string& tid, ErrorList& errs, bool debug, const std::string& path)
        : target(tid), mode(debug ? 1 : 0), outPath(path), errors(errs) {}

    // Temporary compatibility constructor
    OIRWriter(ErrorList& errs)
        : target("Win64"), mode(0), outPath("output.oir"), errors(errs) {}

    void setTarget(const std::string& value) { target = value; }
    void setDebug(bool value) { mode = value ? 1 : 0; }

    uint32_t addSymbol(const std::string& name, uint8_t prefix = SymPrefix::VAR,
                       uint16_t flags = SymFlags::NONE) {
        for (const auto& symbol : symTable) {
            if (symbol.name == name) return symbol.id;
        }
        if (symTable.size() >= std::numeric_limits<uint32_t>::max()) {
            errors.error("P0001", "OIR symbol tablosu ID kapasitesini asti.");
            return 0;
        }
        const uint32_t id = static_cast<uint32_t>(symTable.size()) + 1;
        symTable.push_back({id, prefix, 0, flags, name});
        return id;
    }

    void emit(uint8_t op, const std::string& raw = "", uint32_t sid = 0,
              int line = 0, int col = 0) {
        appendInstruction(op, 0, sid, 0, raw, line, col);
    }

    void emitWithOperand(uint8_t op, uint32_t operand, uint32_t sid = 0,
                         const std::string& raw = "", int line = 0, int col = 0) {
        appendInstruction(op, 0, sid, operand, raw, line, col);
    }

    bool save(const std::string& path = "") {
        const std::string finalPath = path.empty() ? outPath : path;
        if (errors.hasErrors()) return false;
        if (finalPath.empty()) {
            errors.error("P0002", "OIR cikti yolu bos olamaz.");
            return false;
        }
        if (mode < 0 || mode > 1 || target.size() > 31 ||
            target.find('\0') != std::string::npos) {
            errors.error("P0003", "OIR header target veya mode alani gecersiz.");
            return false;
        }

        uint64_t symbolTableSize = 0;
        for (const auto& symbol : symTable) {
            if (symbol.id == 0 || symbol.name.size() > MAX_SYMBOL_NAME ||
                symbol.name.find('\0') != std::string::npos ||
                symbol.prefix < SymPrefix::VAR || symbol.prefix > SymPrefix::EXTERN) {
                errors.error("P0004", "OIR symbol kaydi formata uygun degil: " + symbol.name);
                return false;
            }
            symbolTableSize += 13u + static_cast<uint64_t>(symbol.name.size());
            if (symbolTableSize > std::numeric_limits<uint32_t>::max() - HEADER_SIZE) {
                errors.error("P0005", "OIR symbol tablosu 32-bit offset kapasitesini asti.");
                return false;
            }
        }

        const uint64_t codeOffset64 = HEADER_SIZE + symbolTableSize;
        const uint64_t instructionBytes =
            static_cast<uint64_t>(codeStream.size()) * INSTRUCTION_SIZE;
        if (instructionBytes > std::numeric_limits<uint32_t>::max() - codeOffset64) {
            errors.error("P0006", "OIR instruction stream 32-bit dosya offset kapasitesini asti.");
            return false;
        }

        std::ofstream file(finalPath, std::ios::binary | std::ios::trunc);
        if (!file) {
            errors.error("P0007", "OIR cikti dosyasi olusturulamadi: " + finalPath);
            return false;
        }

        const char magic[4] = {'O', 'I', 'R', '2'};
        char targetBuffer[32] = {};
        std::memcpy(targetBuffer, target.data(), target.size());

        file.write(magic, sizeof(magic));
        writeU16(file, 0x0002);
        writeU16(file, static_cast<uint16_t>(mode));
        file.write(targetBuffer, sizeof(targetBuffer));
        writeU32(file, HEADER_SIZE);
        writeU32(file, static_cast<uint32_t>(codeOffset64));

        for (const auto& symbol : symTable) {
            writeU32(file, symbol.id);
            file.put(static_cast<char>(symbol.prefix));
            writeU32(file, symbol.typeId);
            writeU16(file, symbol.flags);
            writeU16(file, static_cast<uint16_t>(symbol.name.size()));
            if (!symbol.name.empty()) {
                file.write(symbol.name.data(),
                           static_cast<std::streamsize>(symbol.name.size()));
            }
        }

        for (const auto& instruction : codeStream) {
            file.put(static_cast<char>(instruction.op));
            file.put(static_cast<char>(instruction.typeId));
            writeU32(file, instruction.symbolId);
            writeU32(file, instruction.operandId);
            writeU32(file, instruction.line);
            writeU16(file, instruction.col);
        }

        file.flush();
        const bool writeOk = file.good();
        file.close();
        if (!writeOk || file.fail()) {
            errors.error("P0008", "OIR dosyasina yazma islemi tamamlanamadi: " + finalPath);
            return false;
        }

        if (mode == 1) dump();
        return true;
    }

    void dump() {
        std::cout << "\n--- OIR TRACE LOG ---\n";
        for (const auto& instruction : codeStream) {
            std::cout << "  L" << instruction.line << ":" << instruction.col
                      << " OP:" << std::hex << static_cast<int>(instruction.op)
                      << std::dec << " SID:" << instruction.symbolId
                      << " RAW: " << instruction.rawData << "\n";
        }
    }

private:
    void appendInstruction(uint8_t op, uint8_t typeId, uint32_t symbolId,
                           uint32_t operandId, const std::string& raw,
                           int line, int col) {
        if (line < 0 || col < 0 || col > std::numeric_limits<uint16_t>::max()) {
            errors.error("P0009", "OIR source konumu gecersiz.");
            return;
        }
        OIRInstruction instruction{};
        instruction.op = op;
        instruction.typeId = typeId;
        instruction.symbolId = symbolId;
        instruction.operandId = operandId;
        instruction.line = static_cast<uint32_t>(line);
        instruction.col = static_cast<uint16_t>(col);
        instruction.rawData = raw;
        codeStream.push_back(std::move(instruction));
    }

    static void writeU16(std::ofstream& stream, uint16_t value) {
        const char bytes[2] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF)
        };
        stream.write(bytes, sizeof(bytes));
    }

    static void writeU32(std::ofstream& stream, uint32_t value) {
        const char bytes[4] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF),
            static_cast<char>((value >> 16) & 0xFF),
            static_cast<char>((value >> 24) & 0xFF)
        };
        stream.write(bytes, sizeof(bytes));
    }
};
#endif
