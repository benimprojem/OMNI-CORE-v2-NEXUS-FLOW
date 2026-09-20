# OmniCore SDK — Görev Listesi
Okunabilir klasörler
targets/ 	target dosyaları
docs/   	spec dosyaları
bin/    	derlenen dosyalar
lib/		derlenen kütüphane dosyaları
stdlib/ 	nexus flow kütüphane dosyaları
dpslib/     bağımlılık kütüphaneleri
src/    	Kaynak dosyalar
myprojem/  	örnek proje klasörü
test/ 		test dosyaları
logs/
lang/
package/	Derleme için gerekli olabilecek dosyalar paketler
inc/  		Genel Ayar, info dosyaları 

Okunamaz klasörler
temp/	eski kaynak ve spec dosyaları
nexus docs/ eski dokümanlar


## Dökümantasyon ve Bütünlük
**Durum: Tüm dosyalara uygulanacak. Herhangi bir değişiklik ve ekleme olduğunda güncellenecek.**
- [x] `implementation_plan.md`: Token kapsama matrisi oluşturuldu.
- [ ] `nfsyntax.md`: Dilin tüm özelliklerini içeren teknik kılavuz.
- [ ] Kaynak kod dosyalarının başına kapsamlı modül açıklamaları ekle.

## Modül 0: Omni Core Compiler occ.exe (`src/occ/`)
**Durum: %80 (Tamamlandı)**
- [x] `-t` parametresi ile hedefin (target) belirlenmesi (`-t win64`).
- [x] Hedef dosyasının `targets/` klasöründe aranması (örn. `win64.target`).
- [x] Eğer target bulunamazsa hata üretip `targets/` içeriğini listeleyerek kullanıcıdan seçim istemesi.
- [x] `-t` belirtilmemişse yerel sistemin varsayılan hedef (local system target) olarak belirlenmesi.
- [x] Hedef dosyasındaki bilgilerin (veya direktiflerin) OIR sistemine işlenip/iletilip parser'a (DLL'e) aktarılması. (OIR header/targetId olarak).
- [ ] Çoklu derleme kuralları ve [ occ.exe -nexus ] komutu ile nexus.nxr dosyasından okunarak derleme.
- [ ] occ.ocf dosyasını okuma, bağlılıklar ve derleme için gerekli metadata bilgileri.


## Modül 1: Parser DLL (`src/parser/`)
**Durum: %100 (Kapasitif Doğrulama)**
- [x] Lexer: Tüm 180+ token ve niyet operatörleri (The 15 Keys) kapsandı.
- [x] Parser: 1112+ satırlık tam kapasite (Loop variants, group, nt:, rules/on).
- [x] OIR Writer: v2 Binary Spec (0x01-0xB2 opcodes) tamamlandı.
- [x] **DOKÜMANTASYON:** Tüm C++ dosyaları (Lexer, Parser, OIRWriter) fonksiyon bazlı dökümante edildi.

## Modül 2: Semantik Analizör (`src/analyzer/`)
**Durum: %90 (Kapasitif Doğrulama)**
- [x] `analyzer.dll` entegrasyonu ve OIRReader restorasyonu.
- [x] **Gelişmiş Semantik:** Tüm OIR opcodeları için switch-case ve doğrulama mantığı.
- [x] **Implicit Handlers:** (h1), (v2) gibi yapıların otomatik tanınması.
- [x] **DOKÜMANTASYON:** Analyzer.cpp/hpp, SymbolTable ve TypeSystem dökümante edildi.
- [ ] Uyarı mesajları gösterilmiyor, düzeltilmeli.

(Diğer modüller plan aşamasında)

asm kodlarında öncelik arm. daha sonra testler için windows ortamında 80-86 x64 kodlama.. bu iki asm kodlama eksiksiz olmalıdır. Diğerleri daha sonra eklenebilir bunun için gerekli todoo bırakılacak.
