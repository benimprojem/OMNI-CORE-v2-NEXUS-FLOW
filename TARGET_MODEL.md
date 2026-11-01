# OmniCore — Target Modeli

**Durum:** Mimari sözleşme taslağı; mevcut C uygulamasına bütünüyle uygulanmış değildir.

> Önemli: Target yalnızca OS/CPU hedefi değildir. Heterojen işlemci ve aygıt topolojisi ile tek fiziksel makineye bağlama modeli için [TARGET_HARDWARE_PROFILE.md](TARGET_HARDWARE_PROFILE.md) esas alınmalıdır. Bu belge, mevcut v1 parser davranışını ve geçiş durumunu açıklar.

## 1. Host ve target

Derleme boyunca target seçimi ve aktarımı için [TARGET_PIPELINE_CONTRACT.md](TARGET_PIPELINE_CONTRACT.md), tam donanım topolojisi ve exact-machine modeli için [TARGET_HARDWARE_PROFILE.md](TARGET_HARDWARE_PROFILE.md) geçerlidir.

- **Host:** OCC derleyicisinin çalıştığı makine ve işletim sistemi.
- **Target:** Derlenen programın çalışacağı donanım konfigürasyonu ve onun üzerinde çalışan OS/runtime.
- Target bir CPU mimarisinden ibaret değildir; CPU, GPU, NPU, FAPU, bellek, interconnect ve çevre birimlerini kapsayabilir.
- Host varsayılanı yalnızca genel/family profile derlemelerinde kolaylık sağlar. Exact-machine profili açıkça seçilmeli veya proje yapılandırmasında tanımlanmalıdır.
- Host ile target aynı olmak zorunda değildir. Exact-machine cross-compilation da mümkündür; derleyicinin host'u hedef donanım tanımının yerine geçmez.

## 2. Target bileşenleri

| Alan | Örnek | Anlam |
|---|---|---|
| `os` | `windows`, `linux`, `macos` | Hedef işletim sistemi |
| `arch` | `x86_64`, `aarch64` | Genel işlemci mimarisi |
| `abi` | `ms-x64`, `sysv`, `darwin-arm64` | Çağrı ve binary arayüzü |
| `format` | `pe-coff`, `elf`, `macho` | Nesne/çalıştırılabilir dosya biçimi |
| `cpu_model` | isteğe bağlı model adı | Belirli işlemci modeli/ailesi |
| `features` | `sse2`, `avx2`, `neon` | İsteğe bağlı ISA yetenekleri |
| `assembler` | araç adı | Assembly/object üretim aracı |
| `linker` | araç adı | Linker |
| `backend` | backend kimliği | Kullanılacak codegen uygulaması |

Bu örnekler şema alanlarını anlatır; tüm kombinasyonların bugün desteklendiğini göstermez.

## 3. Kullanıcı seçimleri

1. **Hiç seçim yok:** Host OS + host architecture algılanır ve varsayılan target olur.
2. **Genel mimari seçimi:** Örneğin x86-64. OS ayrıca belirtilmediyse host OS kullanılır; seçilen target host'tan farklı olabilir.
3. **İşletim sistemi + mimari:** Örneğin Linux + AArch64; cross-compile hedefi.
4. **Genel mimari + feature seçimi:** Örneğin x86-64 + AVX2. Backend ve hedef runtime'ın bu yeteneği desteklediği doğrulanmalıdır.
5. **CPU modeli seçimi:** Modelin gerektirdiği feature kümesi çözülür; çelişen veya backend tarafından desteklenmeyen seçim reddedilir.
6. **Özel işlemci:** Gelecekte özel architecture/CPU modeli, target descriptor ve kendine ait backend ile eklenebilir. Genel derleyici akışı özel işlemciye bağımlı değildir.

## 4. Feature güvenliği

- Derleyici yalnızca seçilen backend'in desteklediği talimatları üretebilir.
- Bir feature'ın target'ta seçilmesi, derlenen uygulamanın o feature olmayan CPU'da çalışabileceği anlamına gelmez.
- Runtime CPU dispatch ileride ayrıca desteklenebilir; statik target feature seçimiyle aynı şey değildir.
- Varsayılan host target seçilirken CPU feature seviyesi için güvenli ve taşınabilir bir baseline belirlenmelidir. Host'un tüm özelliklerini otomatik açmak, üretilen binary'yi başka makinelerde çalışmaz hale getirebilir.
- Bilinmeyen feature, ABI veya target kombinasyonları sessizce yok sayılmamalıdır.

## 5. Platform aileleri

Planlanan ilk platform ailesi:

- Windows: x86-64 ve AArch64; Microsoft platform ABI ve PE/COFF.
- Linux: x86-64 ve AArch64; ilgili System V ABI profilleri ve ELF.
- macOS: x86-64 ve AArch64; Darwin ABI ve Mach-O.

Her kombinasyon için assembler, linker, system library, entry point, calling convention, object format ve test kapsamı ayrı doğrulanmalıdır.

## 6. Mevcut uygulama farkı

C OCC CLI artık açık `--target` seçimini korur; seçim yapılmadığında derleyici host'u için tanımlı hedef adını seçer (şu an Windows x86-64 → `Win64`, Linux x86-64 → `Linux64`, macOS AArch64 → `macOS_AArch64`). Diğer host kombinasyonlarında sessizce BareMetal veya başka bir hedefe düşmek yerine açık `--target` ister. Bu ilk host eşlemesidir; proje/build yapılandırmasından target seçimi ve eksik host descriptor'ları henüz tamamlanmamıştır. Target descriptor'ın normalize OIR girdisi olarak aşamalara aktarımı da henüz uygulanmamıştır.

## 7. Öncelikli uygulama sırası

1. Target v2 genel veri modeli korunur; cihaz türleri GPU/NPU/FAPU/DSP/FPGA dahil genişletilebilir kalır.
2. İlk parser ve resolver kapsamı OS + BareMetal profilleri ile CPU yetenekleridir.
3. CPU ISA/uzantıları, word/address width, endian, register-file özellikleri, atomikler, SIMD/vector ve FP formatları normalize edilir.
4. İlk platform hedefleri Windows x86-64, Linux x86-64, macOS AArch64 ve BareMetal x86-64'tür.
5. CPU/backend yetenek kontrolü ve Target OIR aktarımı tamamlanır.
6. GPU/NPU/FAPU/DSP/FPGA ve diğer programlanabilir çiplerin operasyonel desteği sonraki aşamalara bırakılır.


## 8. Mevcut `.target` dosyalarının disk biçimi

Mevcut descriptor dosyaları TOML'un tam uygulaması olarak değil, aşağıdaki dar INI/TOML benzeri alt küme olarak ele alınacaktır:

- Bölüm başlıkları: `[target_info]`, `[cpu_arch]`, `[instruction_set]`, `[memory_model]`, `[abi_convention]`, `[output_format]`, `[nexus_primitives]`.
- Atamalar: `key = value`.
- String değerler çift tırnaklıdır; sayılar decimal/hex olabilir; bazı bellek boyutları `K/M` son eki kullanır.
- `#` satır içi ve tam satır yorumları desteklenir.
- Liste alanları `[...]` biçimindedir.
- Parser satır, anahtar, değer ve string uzunluklarını sınırlandırmalı; eksik, yinelenen veya bilinmeyen zorunlu alanları açık hata olarak raporlamalıdır.
- Descriptor varlığı backend desteği anlamına gelmez. Parser descriptor'ı okuyabilir; backend capability kontrolü target'ı desteklenmiyorsa codegen'i durdurmalıdır.

### Kimlik ve OS alanı

`[target_info]` bölümünde `identifier` ve `os` alanları bulunmalıdır. `os` değerleri `none`, `windows`, `linux`, `macos` gibi açık isimlerdir. Beş mevcut descriptor'a bu alan eklendi:

| Descriptor | OS | Mimari | Durum |
|---|---|---|---|
| `BareMetal64.target` | `none` | x86_64 | Mevcut C codegen için ilk test hedefi |
| `Linux64.target` | `linux` | x86_64 | Descriptor var; Linux backend doğrulanmış değil |
| `Win64.target` | `windows` | x86_64 | Descriptor var; Windows ABI codegen doğrulanmış değil |
| `macOS_AArch64.target` | `macos` | aarch64 | Descriptor var; AArch64 backend yok |
| `RV32IMC.target` | `none` | riscv32 | Descriptor var; RISC-V backend yok |

`custom_dsp_v1.target` dosyası `...` placeholder'ları içerdiği için tamamlanmış bir descriptor değildir; parser testlerinde geçersiz/incomplete örnek olarak kullanılmalıdır.

### Mevcut parser farkı

C codegen içindeki eski `parse_target()` hâlâ `key: value` satırları bekliyor; mevcut dosyalar ise bölüm + `key = value` biçiminde. Eski parser dosya açılamadığında da varsayılan target değerleriyle devam ediyor. Bu uyumsuzluk çözülene kadar C codegen'in mevcut descriptor dosyalarını gerçekten okuduğu varsayılmamalıdır.


## Version boundary

The current compiler continues to use the v1 `.target` descriptors and the existing legacy target parser/backend checks. The v2 `.target2` descriptors, normalized C model and bounded parser are preparatory only; they are not loaded by the active compilation path. Do not require v2 fields or parser support for current OS/BareMetal compilation.
