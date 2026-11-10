# OCM — Omni Core Manager Tasarım Taslağı

**Durum:** Mimari taslak. OCM'nin proje oluşturma, bağımlılık yönetimi ve CLI uygulaması henüz geliştirilmedi. Paket metadata, registry, lockfile ve komut sözleşmeleri kesinleşmedi.

## 1. Amaç

OCM (Omni Core Manager), OCC/Nexus Flow geliştirme yaşam alanını hazırlayan ve yöneten yardımcı araçtır. İki ana sorumluluğu vardır:

1. **Proje yönetimi:** Sıfırdan yeni proje klasörü oluşturmak, seçilen şablona göre başlangıç dosyalarını üretmek ve mevcut proje metadata'sını yönetmek.
2. **Bağımlılık yönetimi:** Projelerin harici paketlerini merkezi depoya eklemek, sürümlerini çözmek, doğrulamak ve ortak depodan kullanıma sunmak.

OCM bir derleyici değildir. Kaynak kodu derleme, AST/Semantic/OIR üretimi ve backend işlemleri OCC'nin sorumluluğundadır.

Kanonik merkezi bağımlılık deposu:
- SDK geliştirme ağacında: `omnicore-sdk/occ/dsplibs/`
- Dağıtım paketinde: `occ/dsplibs/`

## 2. OCC / OCM / proje çalışma alanı

| Bileşen | Sorumluluk |
|---|---|
| OCM | Yeni proje oluşturma, şablon dosyaları üretme, proje metadata'sı, dependency ekleme/çözme/indirme, paket doğrulama ve merkezi depo yönetimi |
| OCC CLI/host | Proje config'ini keşfetme ve okuma, derleme hattını çalıştırma, proje kaynaklarını analiz etme, proje içindeki build çıktısını üretme |
| Lexer / Parser / AST | Nexus Flow kaynak metnini token ve AST'ye dönüştürme |
| Module Resolver + host provider | `use` isteklerini güvenli kaynak sağlayıcı üzerinden çözme, modül kimliği/grafiği/cycle yönetimi |
| Semantic | İçe aktarılan sembol, Group, görünürlük, alias, namespace ve tip doğrulaması |
| OIR / Verifier | Ara gösterim üretimi ve bağımsız bütünlük doğrulaması |
| Codegen | Yalnızca backend'e özgü doğrulama ve kod üretimi |

OCM, Parser veya Semantic içine gömülmez. Module Resolver doğrudan paket indirmez ve sınırsız filesystem erişimi yapmaz. OCC, OCM'nin proje/dependency işlemlerini ayrı bir araç olarak çağırabilir veya ileride tanımlanacak makine-arayüzü üzerinden çözülmüş paket köklerini alabilir.

## 3. Ayrı dosya sistemi alanları

SDK dağıtım kökü araçları ve ortak kaynakları barındırır:

```text
occ/                         # SDK/runtime package root
├── bin/
│   ├── occ
│   ├── ocm
│   └── stage DLLs
├── libs/                    # Önceden tanımlı standart Nexus Flow modülleri
├── dsplibs/                 # OCM merkezi üçüncü taraf paket deposu
├── targets/
├── lang/
└── occ.ocf                  # SDK/runtime config
```

OCM'nin oluşturduğu kullanıcı projesi ayrı bir çalışma alanıdır:

```text
<project-name>/
├── occ.ocf
├── README.md
├── .gitignore
├── src/
│   └── main.nxf
├── include/
├── tests/
└── build/                   # Projeye ait üretilmiş derleme çıktıları
```

Bu proje ağacı başlangıç şablonudur; OCM ileride minimal/application/library gibi şablonlar sunabilir. OCC ve OCM araçları SDK'nın `bin/` klasöründen çalıştırılır; kullanıcı proje kökünde komut verdiğinde işlemler aktif proje üzerinde yürütülür. Proje keşif politikası (çalışma dizini, üst dizin taraması, açık proje yolu/CLI argümanı) henüz kesinleşmemiştir.

Proje klasörüne OCC/OCM executable'ları, stage DLL'leri, standart kütüphane kopyaları veya üçüncü taraf dependency payload'ları kopyalanmaz. Proje klasörü yalnızca projeye ait kaynakları, yapılandırmayı ve üretilen build çıktısını taşır.

## 4. Proje oluşturma

OCM, boş bir hedef konumda yeni proje çalışma alanı oluşturabilmelidir:
- proje adı ve hedef klasör doğrulanır;
- başlangıç `occ.ocf`, `src/main.nxf`, `README.md`, `.gitignore` ve gerekli klasörler üretilir;
- temel scaffold için ağ erişimi gerekmez;
- mevcut dosyaların üzerine sessizce yazılmaz;
- dosyalar önce geçici kardeş dizinde hazırlanıp doğrulanır, sonra mümkünse atomik rename ile hedefe alınır;
- hata durumunda kısmi çıktı temizlenir ve mevcut hedef korunur.

Başlangıç OCF örneği henüz taslak düzeyindedir:

```ini
[package]
name = "<normalized-project-name>"
version = "0.1.0"
edition = "2026"

[package.metadata]
language = "en"

[include]
./src
./include

[dependencies]
```

Bu örnek dependency syntax'ı tanımlamaz. `[dependencies]` içeriğinin grameri OCM tarafından belirlenecektir. Mevcut C `occ_project_config` okuyucusu yalnızca bounded `[include]` girdilerini okur ve `[dependencies]` bölümünü bilerek yorumlamaz.

Üretilecek `main.nxf` dosyasının syntax'ı normatif `NEXUS-FLOW-SYNTAX-REFERENCE.md` ve güncel Parser testleriyle doğrulanmalıdır. Bilinen geçerli minimal program yoksa OCM tahmini fonksiyon/API üretmemeli; comment-only starter oluşturup giriş noktası eklenmesi gerektiğini bildirmelidir.

Ayrıntılı başlangıç ağacı: [OCM_PROJECT_TEMPLATE.md](OCM_PROJECT_TEMPLATE.md).

## 5. Merkezi bağımlılık deposu

- Aynı paket sürümü birden fazla proje için tek merkezi kopya olarak tutulur.
- Proje klasörlerinde paket kaynakları veya binary dependency kopyaları oluşturulmaz.
- Proje config'i bağımlılık referanslarını taşır; çözülmüş paket yolları OCM/host tarafından sağlanır.
- Aynı paketin farklı sürümleri yan yana tutulabilir.
- Platforma bağlı `.exf`/binary varyantları OS, architecture ve ABI bilgisiyle ayrıştırılabilir.
- Standart Nexus Flow modülleri harici paket deposu değildir: SDK `libs/` altında kalır. Harici bağımlılıklar SDK `dsplibs/` altında tutulur.

Önerilen paket dizinlemesi (henüz normatif değil):

```text
occ/dsplibs/
└── <package-id>/
    └── <version>/
        ├── package.ocm
        ├── nxf/
        ├── exf/
        └── targets/
```

## 6. OCC ile proje içi derleme akışı

1. Kullanıcı OCM ile proje oluşturur veya var olan proje klasörüne geçer.
2. OCC aktif proje `occ.ocf` dosyasını keşfeder ve proje kökünü belirler.
3. OCC proje kaynaklarını ve include köklerini kullanır; standart modülleri SDK `libs/` dizininden alır.
4. OCM dependency çözümlemesi tamamlandığında paket kökleri merkezi SDK `dsplibs/` deposundan sağlanır.
5. OCC Lexer → Parser/AST → Semantic → OIR → Verifier hattını çalıştırır.
6. Üretilen ara/nihai build çıktıları varsayılan olarak proje `build/` klasörüne yazılır; SDK binary dizinine yazılmaz.

Bu, hedeflenen çalışma modelidir. Bugünkü OCC host henüz proje OCF keşfi, filesystem source-provider, OCM entegrasyonu veya proje-local build output akışını tamamlamamıştır. Mevcut runtime `occ.ocf` okuması proje config entegrasyonu sayılmaz.

## 7. OCF ve dependency çözümleme sınırı

Proje `occ.ocf` dosyasındaki `[dependencies]` bölümü OCM'ye aittir. OCC host'un bounded `occ_project_config` okuyucusu `[include]` girdilerini okur; dependency bildirimlerinin sözdizimini yorumlamaz.

Dependency bildirimlerinin tam biçimi, sürüm aralığı grameri, optional/dev dependency ayrımı ve lockfile formatı ayrıca belirlenecektir. Şu aşamada örnek bir dependency syntax'ı normatif kabul edilmemelidir.

## 8. Güvenlik ve bütünlük gereksinimleri

OCM uygulandığında en az şu kontrolleri sağlamalıdır:

1. Proje adı, hedef yolu ve oluşturulacak dosya yolları doğrulanır; path traversal reddedilir.
2. Proje oluşturma mevcut dosyaları sessizce ezmez; geçici dizin + doğrulama + atomik taşıma yaklaşımı kullanır.
3. Paket manifesti ve sürüm kimliği doğrulanmadan paket etkinleştirilmez.
4. İndirme/ekleme işlemi geçici dizinde yapılır; doğrulama sonrası atomik olarak merkezi depoya taşınır.
5. Paket arşivi açılırken `..`, mutlak yol, sürücü öneki ve izin verilmeyen symlink kaçışları reddedilir.
6. Paket dosyaları için kriptografik checksum doğrulanır. Resolver'ın FNV-1a fingerprint'i güvenlik doğrulaması değildir.
7. Aynı package-id/version farklı içerikle tekrar eklenmeye çalışılırsa sessizce üzerine yazılmaz.
8. Kaldırma işlemi başka projelerin/lockfile'ların referans verdiği sürümleri korumalıdır.
9. Offline kullanımda daha önce doğrulanmış merkezi paketler kullanılabilir.

## 9. OCM için hedeflenen işlemler

Komut isimleri/argümanları henüz belirlenmedi; işlev kapsamı:
- Yeni proje oluşturma ve şablon seçme
- Proje metadata'sını görüntüleme/doğrulama
- Paket arama ve metadata görüntüleme
- Merkezi depoya paket ekleme/indirme
- Bağımlılık ağacını çözümleme
- Sürüm çakışmalarını raporlama
- Paket bütünlüğünü doğrulama
- Kullanılmayan sürümleri listeleme ve güvenli kaldırma
- Proje lockfile'ını üretme/güncelleme

## 10. Açık kararlar ve uygulama kapıları

- Proje keşif politikası ve açık proje yolu CLI seçeneği
- OCF package metadata ve geçerli minimal Nexus Flow entry source
- Proje-local build output / ara artifact dizinlerinin kesin sözleşmesi
- OCM proje şablonlarının C implementasyonu ve testleri
- Registry kaynağı ve güven modeli
- Package manifest alanları ve semver/constraint grameri
- Lockfile adı, formatı ve commit politikası
- Birden fazla proje için aynı sürümün kullanım/temizleme politikası
- `.nxf` source package ile `.exf`/binary package ilişkilendirmesi
- Include dizinleri ile standart libs ve merkezi paketlerin arama önceliği
- Aynı modül adı birden fazla pakette bulunursa ambiguous mi yoksa explicit package-qualified `use` mu kullanılacağı
- OCM ile OCC arasındaki process/API sözleşmesi

Bu kararlar kesinleşmeden OCM CLI, dependency syntax veya package metadata sözleşmesi tamamlanmış sayılmaz. Bu belge ve proje şablonu taslaktır; OCM'nin uygulandığını göstermez.
