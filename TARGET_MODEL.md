# OmniCore — Target Modeli

**Durum:** Mimari sözleşme taslağı; mevcut C uygulamasına bütünüyle uygulanmış değildir.

## 1. Host ve target

Derleme boyunca target seçimi ve aktarımı için bağlayıcı uygulama sözleşmesi [TARGET_PIPELINE_CONTRACT.md](TARGET_PIPELINE_CONTRACT.md) belgesindedir. Bu sözleşmeye göre seçilen/varsayılan descriptor, hedefe duyarlı aşamaların doğrulanmış girdisidir; genel OIR ise target-neutral kalır.

- **Host:** OCC derleyicisinin çalıştığı makine ve işletim sistemi.
- **Target:** Derlenen programın çalışacağı makine/işletim sistemi.
- Host ile target aynı olmak zorunda değildir. Farklı olduklarında cross-compilation yapılır.
- Kullanıcı target belirtmezse host OS + host architecture varsayılan alınır.
- Kullanıcı target belirtirse host varsayılanı kullanılmaz; açık seçim geçerlidir.

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

## 7. Uygulama sırası

1. Mevcut `.target` dosyalarının envanteri ve alan sözlüğü.
2. Target parser için sınır/format doğrulaması.
3. Host OS/architecture tespiti.
4. Target resolver ve CLI seçenekleri.
5. Backend capability matrisi.
6. Platform başına build/test ve cross-compile doğrulaması.


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
