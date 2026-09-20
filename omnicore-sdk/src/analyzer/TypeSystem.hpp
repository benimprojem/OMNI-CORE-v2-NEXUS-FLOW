// ============================================================
// TypeSystem.hpp — NexusFlow Type Registry & Verification
// ============================================================
/*
 * MODÜL: TypeSystem
 * GÖREV: Dilin tip hiyerarşisini (i32, u64, struct, enum vb.) yönetir ve
 *        operasyonlar arası tip uyumluluğunu (Type Checking) denetler.
 * 
 * ÖZELLİKLER:
 * - instance(): Singleton örneğine erişir.
 * - registerType(): Yeni bir kullanıcı tanımlı tipi sisteme kaydeder.
 * - isCompatible(): İki tipin birbirine atanabilirliğini kontrol eder.
 * - getTypeID(): İsimden benzersiz tip ID'sini üretir/getirir.
 * 
 * DURUM: Thread-safe singleton yapısında çalışır.
 */

#ifndef NEXUS_TYPE_SYSTEM_HPP
#define NEXUS_TYPE_SYSTEM_HPP

#include <string>
#include <unordered_map>
#include <vector>

// Core OIR Type IDs (Analyzer uses these to enrich symbols)
namespace TypeID {
    constexpr uint32_t UNKNOWN = 0x00;
    constexpr uint32_t I32     = 0x01;
    constexpr uint32_t U32     = 0x02;
    constexpr uint32_t D64     = 0x03;
    constexpr uint32_t BOOL    = 0x04;
    constexpr uint32_t STR     = 0x05;
    constexpr uint32_t VOID    = 0x06;
}

class TypeSystem {
    std::unordered_map<std::string, uint32_t> typeMap;
    // ... logic ...

    TypeSystem() {
        typeMap["i32"]  = TypeID::I32;
        typeMap["u32"]  = TypeID::U32;
        typeMap["d64"]  = TypeID::D64;
        typeMap["bool"] = TypeID::BOOL;
        typeMap["str"]  = TypeID::STR;
        typeMap["void"] = TypeID::VOID;
    }

public:
    static TypeSystem& instance() {
        static TypeSystem ts;
        return ts;
    }

    uint32_t getTypeID(const std::string& name) {
        auto it = typeMap.find(name);
        return (it != typeMap.end()) ? it->second : TypeID::UNKNOWN;
    }

    bool isCompatible(uint32_t left, uint32_t right) {
        if (left == right) return true;
        // Basic promotions ...
        return false;
    }
};

#endif
