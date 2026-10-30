// ============================================================
// OIRReader.hpp — NexusFlow OIR Binary Reader
// ============================================================
/*
 * MODÜL: OIRReader
 * GÖREV: .oir uzantılı binary ara temsil (IR) dosyasını okur ve
 *        bellekteki sembol/talimat yapılarına (Analyzer için) dönüştürür.
 * 
 * ANA SÜREÇLER:
 * - load(): Dosyayı açar, Magic/Version kontrolü yapar ve bölümleri okur.
 * - getPayload(): Belirli bir talimata bağlı ek verileri (string literal vb.) döner.
 * - getSymbol(): ID üzerinden sembol bilgilerine erişim sağlar.
 * 
 * YAPILAR:
 * - OIRPayload: Talimat dışı büyük verileri (rawData) tutar.
 */

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

struct OIRPayload {
    uint32_t    instrIndex;
    std::string data;
};

class OIRReader {
    std::string             filename;
    ErrorList&              errors;

public:
    std::string                targetId;
    uint16_t                   mode = 0;
    std::vector<OIRSymbol>     symbols;
    std::vector<OIRPayload>    payloads;
    std::vector<OIRInstruction> instructions;

    OIRReader(const std::string& file, ErrorList& errs)
        : filename(file), errors(errs) {}

    bool load() {
        std::ifstream f(filename, std::ios::binary);
        if (!f) {
            errors.error("A0001", "OIR dosyasi acilamadi: " + filename);
            return false;
        }

        char magic[4];
        if (!f.read(magic, 4) || std::strncmp(magic, "OIR2", 4) != 0) {
            errors.error("A0002", "Gecersiz veya bozuk OIR dosyasi (Magic uyusmuyor).");
            return false;
        }

        uint16_t ver = read16(f);
        if (ver != 0x0002) return false;

        mode = read16(f);
        char targetBuf[32];
        f.read(targetBuf, 32);
        targetId = std::string(targetBuf);

        uint32_t symTblOff = read32(f);
        uint32_t codeOff   = read32(f);

        // [SYMBOL TABLE]
        f.seekg(symTblOff);
        // Note: The count is not explicitly stored in OIR v2 header, but we read until codeOff
        while (f.tellg() < (std::streampos)codeOff && !f.eof()) {
            OIRSymbol s;
            if (!f.read((char*)&s.id, 4)) break;
            s.prefix = (uint8_t)f.get();
            f.read((char*)&s.typeId, 4);
            f.read((char*)&s.flags, 2);
            uint16_t nlen = read16(f);
            if (nlen > 1024) break; // safety
            std::string name(nlen, '\0');
            f.read(&name[0], nlen);
            s.name = name;
            symbols.push_back(s);
        }

        // [CODE STREAM]
        f.seekg(codeOff);
        while (!f.eof()) {
            OIRInstruction instr;
            int op = f.get(); if (op == EOF) break;
            instr.op = (uint8_t)op;
            instr.typeId = (uint8_t)f.get();
            f.read((char*)&instr.symbolId, 4);
            f.read((char*)&instr.operandId, 4);
            f.read((char*)&instr.line, 4);
            f.read((char*)&instr.col, 2);
            instructions.push_back(instr);
        }
        return true;
    }

    const OIRSymbol* getSymbol(uint32_t id) const {
        for (const auto& s : symbols) if (s.id == id) return &s;
        return nullptr;
    }

private:
    uint16_t read16(std::ifstream& f) {
        uint8_t b0 = f.get(), b1 = f.get();
        return (uint16_t)b0 | ((uint16_t)b1 << 8);
    }
    uint32_t read32(std::ifstream& f) {
        uint8_t b0 = f.get(), b1 = f.get(), b2 = f.get(), b3 = f.get();
        return (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
    }
};

#endif
