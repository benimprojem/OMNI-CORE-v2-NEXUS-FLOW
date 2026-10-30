# OmniCore — Target Modeli

**Durum:** Mimari sözleşme taslağı; mevcut C uygulamasına bütünüyle uygulanmış değildir.

## 1. Host ve target

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

C OCC girişinde şu anda `x86_64_baremetal` varsayılanı ve Windows DLL yükleme yolu bulunuyor. Bu mevcut prototip davranışıdır; bu belgede tanımlanan host-default ve üç işletim sistemi hedef modeli henüz uygulanmış değildir. Build ve target çözümleme çalışmaları bu farkı kapatacaktır.

## 7. Uygulama sırası

1. Mevcut `.target` dosyalarının envanteri ve alan sözlüğü.
2. Target parser için sınır/format doğrulaması.
3. Host OS/architecture tespiti.
4. Target resolver ve CLI seçenekleri.
5. Backend capability matrisi.
6. Platform başına build/test ve cross-compile doğrulaması.
