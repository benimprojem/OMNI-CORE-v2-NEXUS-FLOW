# OCM — Merkezi Bağımlılık Yönetimi Taslağı

**Durum:** Mimari taslak. OCM uygulaması, paket metadata biçimi, registry protokolü ve CLI komutları henüz geliştirilmedi veya kesinleştirilmedi.

## 1. Amaç

OCM, OCC/Nexus Flow projelerinin harici bağımlılıklarını tek bir merkezi depoda yönetir. Bir bağımlılık her projeye ayrı ayrı kopyalanmaz. Projeler yalnızca bağımlılık kimliği ve sürüm gereksinimini bildirir; OCM paketi merkezi depoya ekler veya oradan çözer.

Kanonik depo:

- omnicore-sdk/occ/dsplibs/ — SDK geliştirme ağacındaki merkezi depo.
- occ/dsplibs/ — OCC paketinin dağıtımındaki ortak bağımlılık deposu.

## 2. Sorumluluk ayrımı

| Bileşen | Sorumluluk |
|---|---|
| OCM | Paket ekleme/indirme, sürüm yönetimi, metadata, bütünlük doğrulama, kaldırma ve merkezi depoda tutma |
| OCC host | Aktif proje config'ini okuma, OCM'den çözümlenmiş paket köklerini alma, derleme işini planlama |
| Module Resolver | Dotted use isteğini host source-provider üzerinden çözme, modül kimliği/bağımlılık grafiği/cycle yönetimi |
| Semantic | İçe aktarılan sembol, Group, görünürlük, alias ve namespace doğrulaması |
| Codegen | Yalnızca backend'e özgü işlemler |

OCM, Parser veya Semantic içine gömülmez. Module Resolver doğrudan paket indirmez ve filesystem'e sınırsız erişmez.

## 3. Depo ilkesi

- Aynı paket sürümü birden fazla proje için tek merkezi kopya olarak tutulur.
- Proje klasörlerinde paket kaynakları veya binary bağımlılık kopyaları oluşturulmaz.
- Proje config'i bağımlılık referanslarını taşır; çözülmüş paket yolları OCM/host tarafından sağlanır.
- Aynı paketin farklı sürümleri yan yana tutulabilir.
- Platforma bağlı .exf/binary varyantları OS, architecture ve ABI bilgisiyle ayrıştırılabilir.
- Standart Nexus Flow modülleri harici paket deposu değildir: occ/libs/ altında kalır. Harici bağımlılıklar occ/dsplibs/ altında tutulur.

Önerilen paket dizinlemesi (henüz normatif değil):

\`\`\`text
occ/dsplibs/
└── <package-id>/
    └── <version>/
        ├── package.ocm
        ├── nxf/
        ├── exf/
        └── targets/
\`\`\`

## 4. OCF ile ilişki

Proje occ.ocf dosyasındaki [dependencies] bölümü OCM'ye aittir. OCC host'un yeni occ_project_config okuyucusu yalnızca [include] dizinlerini okur; bağımlılık bildirimlerinin sözdizimini yorumlamaz.

Bağımlılık bildirimlerinin tam biçimi, sürüm aralığı grameri, optional/dev dependency ayrımı ve lockfile formatı ayrıca belirlenecektir. Şu aşamada örnek bir dependency syntax'ı normatif kabul edilmemelidir.

## 5. Güvenlik ve bütünlük gereksinimleri

OCM uygulandığında en az şu kontrolleri sağlamalıdır:

1. Paket manifesti ve sürüm kimliği doğrulanmadan paket etkinleştirilmez.
2. İndirme/ekleme işlemi geçici dizinde yapılır; doğrulama sonrası atomik olarak merkezi depoya taşınır.
3. Paket arşivi açılırken .., mutlak yol, sürücü öneki ve izin verilmeyen symlink kaçışları reddedilir.
4. Paket dosyaları için kriptografik checksum doğrulanır. Resolver'ın FNV-1a fingerprint'i güvenlik doğrulaması değildir.
5. Aynı package-id/version farklı içerikle tekrar eklenmeye çalışılırsa sessizce üzerine yazılmaz.
6. Kaldırma işlemi, başka projelerin/lockfile'ların referans verdiği sürümleri korumalıdır.
7. Offline kullanımda daha önce doğrulanmış merkezi paketler kullanılabilir.

## 6. OCM için hedeflenen işlemler

Komut isimleri/argümanları henüz belirlenmedi; işlev kapsamı:

- Paket arama ve metadata görüntüleme
- Merkezi depoya paket ekleme/indirme
- Bağımlılık ağacını çözümleme
- Sürüm çakışmalarını raporlama
- Paket bütünlüğünü doğrulama
- Kullanılmayan sürümleri listeleme ve güvenli kaldırma
- Proje lockfile'ını üretme/güncelleme

## 7. Açık kararlar

- Registry kaynağı ve güven modeli
- Package manifest alanları ve semver/constraint grameri
- Lockfile adı, formatı ve commit politikası
- Birden fazla proje için aynı sürümün kullanım/temizleme politikası
- .nxf source package ile .exf/binary package ilişkilendirmesi
- Include dizinleri ile merkezi paketlerin arama önceliği
- Aynı modül adı birden fazla pakette bulunursa ambiguous mi yoksa explicit package-qualified use mu kullanılacağı

Bu kararlar kesinleşmeden OCM'nin package metadata veya CLI sözleşmesi tamamlanmış sayılmaz.
