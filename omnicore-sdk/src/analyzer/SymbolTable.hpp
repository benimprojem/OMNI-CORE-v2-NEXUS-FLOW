// ============================================================
// SymbolTable.hpp — NexusFlow Symbol Management
// ============================================================
/*
 * MODÜL: SymbolTable
 * GÖREV: Programdaki tüm sembolleri (değişkenler, fonksiyonlar, kurallar vb.)
 *        ID tabanlı bir sözlük yapısında tutar ve hızlı erişim sağlar.
 * 
 * FONKSİYONLAR:
 * - addSymbol(): Yeni bir sembolü tabloya ekler.
 * - getSymbol(): ID üzerinden sembol detaylarına erişir.
 * - findByName(): İsme göre sembol araması yapar.
 * - exists(): ID'nin tabloda tanımlı olup olmadığını denetler.
 * - clear(): Tabloyu sıfırlar (yeni analiz oturumu için).
 */

#ifndef NEXUS_SYMBOL_TABLE_HPP
#define NEXUS_SYMBOL_TABLE_HPP

#include "../parser/OIRWriter.hpp"
#include <string>
#include <vector>
#include <unordered_map>

class SymbolTable {
    std::unordered_map<uint32_t, OIRSymbol> table;
    std::unordered_map<std::string, uint32_t> nameMap;

public:
    void addSymbol(const OIRSymbol& sym) {
        table[sym.id] = sym;
        nameMap[sym.name] = sym.id;
    }

    OIRSymbol* getSymbol(uint32_t id) {
        auto it = table.find(id);
        return (it != table.end()) ? &it->second : nullptr;
    }

    uint32_t findByName(const std::string& name) {
        auto it = nameMap.find(name);
        return (it != nameMap.end()) ? it->second : 0;
    }

    bool exists(uint32_t id) const {
        return table.find(id) != table.end();
    }

    size_t size() const { return table.size(); }
    void clear() { table.clear(); nameMap.clear(); }
};

#endif
