# Intent System — Tasarım Notu

**Durum:** Bilinçli olarak ertelendi; ayrıntılı tasarım yapılmadı  
**Dil işareti:** `@`  
**İlgili ayrım:** `!!=` derleyici talimatları

## Şimdilik kararlaştırılan tek ayrım

- `!!=`: Derleyiciye açık, daha katı sözdizimine sahip talimatlar vermek için kullanılır.
- `@`: Programcının niyetini, isteğini veya ulaşmak istediği amacı AI destekli bir katmana iletmek için ayrılmıştır.

`@` bir derleyici talimatının alternatifi değildir. Ancak bunun nihai grameri, kapsamı, davranışı ve taşıyacağı veri henüz tanımlanmamıştır.

## Gelecekte hedeflenen kullanım alanları

AI katmanı geliştirildiğinde programcı, örneğin:

1. **Algoritma desteği:** Bir yerde hangi algoritmanın gerektiğini bilmediğinde, ihtiyacını niyet olarak ifade edip AI'dan uygun algoritmayı ve kodu önermesini isteyebilir.
2. **Kod inceleme ve iyileştirme:** Kendi yazdığı kodu AI'ya inceletip daha iyi bir algoritma veya uygulama olup olmadığını sorabilir.
3. **Açık uçlu yardım:** İleride hata analizi, alternatif çözüm üretimi, bellek kullanımı, paralellik, performans ve hedef donanıma uyarlama gibi farklı istekler eklenebilir.

Bunlar örnek kullanım senaryolarıdır; sabit bir intent kategorisi listesi veya kesinleşmiş sözdizimi değildir.

## Bilinçli olarak ertelenen kararlar

AI bileşeni henüz bulunmadığı için aşağıdaki konular şimdi tasarlanmayacaktır:

- `@` işaretinin kesin grameri ve kullanım biçimi
- Niyetin kapsamı ve bağlam modeli
- Intent türleri, parametreleri ve veri formatı
- AI ile programcı arasındaki soru-cevap / öneri / onay akışı
- AST ve OIR içindeki nihai temsil
- AI önerilerinin kod tabanına uygulanma ve doğrulanma yöntemi

Bu konular, AI katmanının tasarımına ve gerçek kullanım ihtiyaçlarına gelindiğinde birlikte, uçtan uca netleştirilecektir. Şimdilik parser'a varsayımsal `@` sözdizimi eklenmeyecek ve bu başlık aktif geliştirme işi sayılmayacaktır.

## İlgili belgeler

- [Nexus Flow syntax referansı](omnicore-sdk/docs/syntax.md)
- [Nexus Flow spec inceleme raporu](SPEC_AUDIT.md)
