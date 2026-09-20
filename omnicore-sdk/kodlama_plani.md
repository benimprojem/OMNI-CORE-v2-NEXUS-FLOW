# OmniCore SDK — Kodlama Planı & Mimari Analiz

## 1. Proje Genel Bakış

**NexusFlow** → yeni nesil sistem programlama dili  
**OmniCore Compiler (OCC)** → NexusFlow kaynak kodunu derleyen araç zinciri  
**Hedef:** `.nx` kaynak → Lexer → Parser → OIR → Optimizer → CodeGen → `.asm/.obj/.exe/.dll`

---

## 2. Derleyici Boru Hattı (Pipeline)

```
.nx kaynak kodu
    │
    ▼
[Lexer] → Token akışı
    │
    ▼
[Parser] → OIR (binary intermediate representation)
    │
    ▼
[Semantic Analyzer] → Doğrulanmış OIR + Sembol Tablosu
    │
    ▼
[Optimizer] → Optimize edilmiş OIR (DAG üzerinde pass'lar)
    │
    ▼
[Code Generator] → Mimari assembly (.s / .asm)
    │
    ▼
[Linker (ocl)] → .exe / .elf / .dll / .so / .a
```

**Şu anki pozisyon:** `Parser` DLL olarak C++ ile yazılıyor. Diğer modüller henüz yok.

---

## 3. Mevcut Durum Analizi

### 3.1 Neler Var (Mevcut Dosyalar)

| Dosya | Durum | Kritik Sorunlar |
|-------|-------|-----------------|
| `Token.hpp` | Var | Eksik tokenlar; `AdvancedToken.hpp` ile çakışıyor |
| `AdvancedToken.hpp` | Var | Token.hpp'ten farklı enum tanımı — **çift tanım** |
| `Lexer.cpp` | %30 | Token listesi döndürmüyor; sayı/string lexing eksik |
| `NexusParser.cpp` | %50 | 7+ tanımsız metot, expression parser yüzeysel |
| `OIR_Generator.cpp` | %10 | Sadece header yazıyor; parser entegrasyonu yok |

### 3.2 Tamamen Boş Modüller

| Klasör | İçerik | Öncelik |
|--------|--------|---------|
| `src/analizer/` | Boş | Yüksek |
| `src/codegen/` | Boş | Yüksek |
| `src/optimizer/` | Boş | Orta |

### 3.3 Yapısal Sorunlar

> [!WARNING]
> **`Token.hpp` ve `AdvancedToken.hpp` aynı anda iki `TokenType` enum tanımlıyor.**  
> `NexusParser.cpp`, `AdvancedToken.hpp`'i include ediyor ama içinde Token.hpp'in token'larını kullanıyor. Derleme hatası garantili.

> [!IMPORTANT]
> **`NexusParser.cpp` içinde 7 adet tanımsız metot var:**  
> `parseFunction`, `parseIf`, `parseRules`, `parseMacro`, `parseAnonymousBlock`, `parseDefaultBlock`, `parseNewTypeDefinition`  
> Bunlar tamamlanmadan parser derlenemez.

---

## 4. Faz Planı

### Faz 0: Altyapı Temizliği (Önce Yapılacak)

**Hedef:** Mevcut kodu derlenebilir hale getir.

1. `Token.hpp` ve `AdvancedToken.hpp`'i tek `Token.hpp`'te birleştir
2. Eksik tüm `TokenType` enum değerlerini ekle 
3. `Lexer.cpp`'e `tokenize() → std::vector<Token>` metodu ekle
4. `NexusParser.cpp`'e 7 eksik metodun stub'larını ekle
5. CMake / Makefile ile DLL derleme build sistemi kur

**Çıktı:** Derlenen ama henüz tam çalışmayan parser DLL

---

### Faz 1: Lexer Tamamlama

**Hedef:** Her NexusFlow token'ını doğru tanıyan tam lexer.

**Yapılacaklar:**

```cpp
// Tam Lexer interface
class NexusLexer {
public:
    std::vector<Token> tokenize(const std::string& source);
private:
    Token lexString();        // "..." ve escape sequences
    Token lexChar();          // 'a'
    Token lexNumber();        // int, float, hex (0xFF), bit (1010b)
    Token lexOperator();      // 15 key + bileşik operatörler
    Token lexIdentifier();    // keyword mi, identifier mi?
    void skipComment();       // # ve ## blok ##
    void trackPosition();     // line:col takibi
};
```

**Token öncelik sırası (15 Key):**
`(h)` → `<-` → `_>` → `?(n,ms)` → `?->` → `?=>` → `->` → `!->` → `<<` → `>>` → `(e){}` → `?;` → `!!` → `@` → `!!= `

---

### Faz 2: Parser Tamamlama

**Hedef:** Tüm NexusFlow syntaxını OIR instruction'larına dönüştüren tam parser.

**Eksik metodlar:**

```cpp
void parseFunction();       // f:isim(params)!tip { body }
void parseIf();             // (cond) =?> { true_body } -> { false_body }  
void parseRules();          // rules { r:kural ... }
void parseMacro();          // :macro! { ... }
void parseAnonymousBlock(); // identifier => { ... }
void parseDefaultBlock();   // default { ... } (group içinde)
void parseNewTypeDefinition(); // nt:struct:Oyuncu
```

**Expression Parser (Pratt):**
```
Mevcut: getExpression() → düz string döndürüyor
Hedef: parseExpression() → operator precedence (pratt parser)
    + precedence levels: || < && < == != < < > <= >= < + - < * / % < unary
```

**Hata yönetimi:**
```cpp
// Şu an: throw std::string  (yakalanmıyor)
// Hedef: Error recovery + hata listesi
struct ParseError { int line, col; std::string msg; };
std::vector<ParseError> errors;
void synchronize();  // panic mode recovery
```

---

### Faz 3: OIR Generator Entegrasyonu

**Hedef:** Parser'dan gelen instruction'ları binary `.oir` formatında dosyaya yaz.

**OIR Binary Format:**
```
[HEADER - 12 byte]
  "NXFW" (4)   → signature
  0x0002  (2)  → version
  arch    (1)  → 32/64 bit
  simd    (1)  → none/sse/avx/avx512
  abi     (1)  → sysv/ms_x64/none
  entry   (4)  → entry point offset

[INSTRUCTIONS]
  opcode  (1)  → instruction kodu
  len     (2)  → payload byte sayısı
  payload (N)  → instruction verisi

[STRING TABLE]
  count   (2)  → string sayısı
  strings (N)  → null-terminated stringler
```

**OIR Opcode tablosu (temel):**
| Opcode | İsim | Açıklama |
|--------|------|----------|
| 0x01 | SET_TARGET | Hedef mimari belirle |
| 0x05 | USE_MOD | Modül import |
| 0x10 | DEF_VAR | Değişken tanımla |
| 0x11 | CAPTURE | Handle'a değer yakala |
| 0x20 | RETURN | Fonksiyondan dön |
| 0x30 | DEF_FUNC | Fonksiyon tanımla |
| 0x40 | SELECT_BRANCH | select/match dalı |
| 0x50-0x55 | LOOP_* | Döngü tipleri |
| 0x60 | DEF_STRUCT | Struct tanımla |
| 0x80-0x82 | LABEL_* | Etiket / atlama |
| 0x90-0x91 | METHOD_* | Grup metodu |
| 0xD0-0xD2 | ROLLING_* | Hata yönetimi |
| 0xE0 | EXPR_EVAL | İfade değerlendir |
| 0xFF | CLEANUP | Kaynakları temizle |

---

### Faz 4: Semantik Analizör

**Hedef:** OIR üzerinde anlam kontrolleri.

```cpp
class SemanticAnalyzer {
    SymbolTable symtab;     // Scope stack
    TypeChecker typer;      // Tip uyumu kontrolü
    HandlerTracker owner;   // Masa sahiplik analizi
    LabelResolver labels;   // Forward/backward jump çözümleme
public:
    bool analyze(OIRProgram& prog, std::vector<SemanticError>& errs);
};
```

---

### Faz 5: Code Generator

**Hedef:** OIR → x86-64 assembly (Windows + Linux ABI).

```cpp
class CodeGen {
    Target target;          // x86-64, ARM64
    ABI abi;                // MS x64, System V, bare-metal
    RegAllocator regalloc;  // Graph Coloring
public:
    void emit(OIRProgram& prog, std::ostream& out);
private:
    void emitProlog(Function& fn);
    void emitEpilog(Function& fn);
    void emitArith(OIRInstr& instr);
    void emitCall(OIRInstr& instr);
    void emitSIMD(OIRInstr& instr);   // AVX/SSE
    void emitIntrinsic(OIRInstr& instr); // spawn, listen vs.
};
```

---

## 5. DLL Mimarisi

```
parser.dll (C++ → C-ABI export)
    │
    ├── nxf_tokenize(src: *char) → TokenArray
    ├── nxf_parse(tokens: TokenArray) → OIRBytes
    ├── nxf_parse_file(path: *char) → OIRBytes
    └── nxf_free(ptr: *void)

nexus_parser.h (public header)
    → C ve diğer dillerden kullanılabilir
```

---

## 6. Öncelikli Acil Adımlar

```
[P0 — Bu hafta]
├── Token.hpp birleştirme + eksik enum'lar
├── Lexer.cpp tokenize() metodu
├── NexusParser.cpp 7 eksik metodun stub implementasyonu
└── CMakeLists.txt (DLL build)

[P1 — Sonraki adım]  
├── Lexer: tam sayı/string/char lexing
├── Parser: parseFunction() tam implementasyon
├── Parser: parseIf() (=?> syntax)
├── OIR_Generator: parser ile entegrasyon
└── DLL export header

[P2 — Orta vadeli]
├── Expression parser (Pratt)
├── src/analizer/ başlat
├── Error recovery (panic mode)
└── Satır/sütun takibi

[P3 — Uzun vadeli]  
├── src/codegen/ x86-64 backend
├── src/optimizer/ pass'lar
└── Runtime / Nexus Slot Matrix
```

---

## 7. Bağımlılık Sırası

```
Token.hpp (birleşik)
    ↓
Lexer.cpp (tokenize)
    ↓
NexusParser.cpp (tam parser)
    ↓
OIR_Generator.cpp (binary output)
    ↓
parser.dll (DLL export)
    ↓
src/analizer/ (semantic)
    ↓
src/optimizer/ (OIR optimize)
    ↓
src/codegen/ (assembly output)
    ↓
ocl linker
```
