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
    std::string target;
    int         mode;
    std::string outPath;
    
    std::vector<OIRSymbol>      symTable;
    std::vector<OIRInstruction> codeStream;
    ErrorList&                  errors;

public:
    OIRWriter(const std::string& tid, ErrorList& errs, bool debug, const std::string& path)
        : target(tid), mode(debug ? 1 : 0), outPath(path), errors(errs) {}

    // Temporary compatibility constructor
    OIRWriter(ErrorList& errs) : target("Win64"), mode(0), outPath("output.oir"), errors(errs) {}

    void setTarget(const std::string& t) { target = t; }
    void setDebug(bool d) { mode = d ? 1 : 0; }

    uint32_t addSymbol(const std::string& name, uint8_t prefix = 0x01, uint16_t flags = 0x00) {
        for (const auto& s : symTable) if (s.name == name) return s.id;
        uint32_t id = (uint32_t)symTable.size() + 1;
        symTable.push_back({ id, prefix, 0, flags, name });
        return id;
    }

    void emit(uint8_t op, const std::string& raw = "", uint32_t sid = 0, int l = 0, int c = 0) {
        codeStream.push_back({ op, 0, sid, 0, (uint32_t)l, (uint16_t)c, raw });
    }

    void emitWithOperand(uint8_t op, uint32_t operand, uint32_t sid = 0, const std::string& raw = "", int l = 0, int c = 0) {
        codeStream.push_back({ op, 0, sid, operand, (uint32_t)l, (uint16_t)c, raw });
    }

    bool save(const std::string& path = "") {
        std::string finalPath = path.empty() ? outPath : path;
        if (errors.hasErrors()) return false;
        std::ofstream fs(finalPath, std::ios::binary);
        if (!fs) return false;
        
        // ---- PASS 1: Sembol tablosu boyutunu hesapla ----
        uint32_t symTableSize = 0;
        for (const auto& s : symTable) {
            symTableSize += 4 + 1 + 4 + 2 + 2 + (uint32_t)s.name.size(); // id+prefix+typeId+flags+len+name
        }

        // ---- HEADER (48 byte) ----
        // [0..3]   Magic: "OIR2"
        // [4..5]   Version: 0x0002
        // [6..7]   Mode
        // [8..39]  Target (32 byte)
        // [40..43] symTableOffset (= 48, right after header)
        // [44..47] codeOffset     (= 48 + symTableSize)
        char magic[4] = {'O', 'I', 'R', '2'}; fs.write(magic, 4);
        uint16_t v = 2;          fs.write((char*)&v, 2);
        uint16_t m = (uint16_t)mode; fs.write((char*)&m, 2);
        char tBuf[32]; std::memset(tBuf, 0, 32);
        std::strncpy(tBuf, target.c_str(), 31); fs.write(tBuf, 32);
        
        uint32_t symOff  = 48;
        uint32_t codeOff = 48 + symTableSize;
        fs.write((char*)&symOff,  4);
        fs.write((char*)&codeOff, 4);

        // ---- SYMBOL TABLE ----
        for (const auto& s : symTable) {
            fs.write((char*)&s.id,     4);
            fs.write((char*)&s.prefix, 1);
            fs.write((char*)&s.typeId, 4);
            fs.write((char*)&s.flags,  2);
            uint16_t len = (uint16_t)s.name.size();
            fs.write((char*)&len, 2);
            fs.write(s.name.c_str(), len);
        }

        // ---- CODE STREAM ----
        for (const auto& i : codeStream) {
            fs.write((char*)&i.op,       1);
            fs.write((char*)&i.typeId,   1);
            fs.write((char*)&i.symbolId, 4);
            fs.write((char*)&i.operandId,4);
            fs.write((char*)&i.line,     4);
            fs.write((char*)&i.col,      2);
        }
        fs.close();
        if (mode == 1) dump();
        return true;
    }

    void dump() {
        std::cout << "\n--- OIR TRACE LOG ---\n";
        for (const auto& i : codeStream) {
            std::cout << "  L" << i.line << ":" << i.col << " OP:" << std::hex << (int)i.op << std::dec 
                      << "  SID:" << i.symbolId << "  RAW: " << i.rawData << "\n";
        }
    }
};

#endif
