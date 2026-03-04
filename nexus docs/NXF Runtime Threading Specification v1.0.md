---

# 🧠 NXF Runtime Threading Specification v1.0

## 1. Amaç

Bu sistem:

* Deterministik async yürütme sağlar
* Kernel seviyesinde sıfır CPU bekleme hedefler
* Tek bir merkezi tablo ile state yönetir
* Bare-metal → Windows → Linux → macOS uyumlu çalışır
* Copy-free IPC sağlar
* Cache-line optimized tasarlanmıştır

---

# 2. Memory Layout

## 2.1 Nexus Slot Matrix

Her async görev için 1 adet slot ayrılır.
Tüm slotlar 32-byte hizalıdır.

```c
struct Nexus_Slot {
    u64 thread_handle;   // OS handle / TID / SP
    u8  status;          // 0=Empty,1=Running,2=Done,3=Timeout
    u8  priority;        // 0-255
    u16 timeout_ms;      // 0 = infinite
    u64 payload_ptr;     // 1024B area or pointer
    u8  pldstatus;       // 1=Raw,2=Ptr+Size,3=FilePath
    u8  reserved[11];    // padding (32B aligned)
}
```

### 🔒 Atomic Kurallar

* `status` değişimi atomiktir
* `payload_ptr` yazıldıktan sonra `status=2` yapılır
* Memory barrier zorunlu:

  * x86 → `mfence`
  * ARM → `dmb ish`

---

## 2.2 Listen Trigger Area

Bu yapı **tek instance** olarak bulunur.

```c
struct Listen_Trigger {
    u8  trigger_flag;  // 0=idle,1=event
    u16 counter;       // aktif task sayısı
    u32 last_thread_id;
    u8  pad;
}
```

### Atomic Kurallar

* `trigger_flag` CAS ile yazılır
* `counter` fetch_add / fetch_sub kullanır

---

# 3. Execution Model

## 3.1 spawn()

```
(handle) <- spawn(fn(args), async, timeout)
```

### Spawn Flow

1. Boş slot bulunur
2. Slot:

   * status = 1
   * timeout set edilir
3. OS thread başlatılır
4. counter++

---

## 3.2 done(type, payload, mem_addr)

### Internal Flow

1. Payload ilgili alana yazılır
2. `pldstatus` set edilir
3. Memory barrier
4. `status = 2`
5. `trigger_flag = 1`
6. `last_thread_id` güncellenir
7. counter--
8. Thread kapanır

⚠ Kritik Sıra:

```
write payload
write pldstatus
memory barrier
write status
set trigger
```

---

# 4. !listen() Mekanizması

## 4.1 Yüksek Seviye Mantık

`!listen(handle, interval_ms)`

### OS Mode

| Platform   | Mekanizma         |
| ---------- | ----------------- |
| Windows    | WaitOnAddress     |
| Linux      | futex(FUTEX_WAIT) |
| macOS      | ulock_wait        |
| Bare-metal | HLT / WFI         |

---

## 4.2 Dinleme Algoritması

Pseudo:

```c
while(true){

    if(trigger_flag == 0){
        sleep_or_wait();
    }

    while(buffer_has_done_tasks()){
        consume_task();
    }

    trigger_flag = 0;
}
```

### Önemli Optimizasyon

* Slot tarama O(N)
* N sabit tutulmalı (örn 256 max)
* Alternatif: Bitmask ready-map (opsiyonel v2)

---

# 5. CPU Optimizasyon Seviyeleri

## 5.1 x86

* Spin kısa süre: `PAUSE`
* Uzun bekleme: WaitOnAddress / futex
* Bare metal: `hlt`

## 5.2 ARM

* `WFI`
* `SEV` ile wake

---

# 6. Payload Türleri

| pldstatus | Anlam                    |
| --------- | ------------------------ |
| 1         | Raw Data (max 1024B)     |
| 2         | Pointer (8B) + Size (8B) |
| 3         | File Path                |

### Tavsiye:

1024B sabit alan → L1 cache dostu

---

# 7. IPC (Send / Receive)

## 7.1 Tasarım İlkesi

* Zero copy
* Ownership transfer
* Atomic pointer swap

---

## 7.2 send()

```
send(mailbox, ptr)
```

* Pointer ownership target’a geçer
* Sender artık erişemez

---

## 7.3 receive()

```
(msg) <- receive(mailbox)
```

* Veri gelene kadar bloklanır
* WaitOnAddress/futex kullanır

---

# 8. Bare-Metal Mode

## Kernel Loop

```c
while(true){

    if(counter == 0){
        asm("hlt");
    }

    if(trigger_flag == 1){
        scan_slots();
    }
}
```

### Avantaj

* %0 CPU
* Deterministik
* RTOS gerektirmez

---

# 9. Güvenlik ve Yarış Koşulları

### Risk 1: Double Done

Çözüm:

* status kontrolü
* CAS ile 1→2 geçişi

### Risk 2: ABA Problemi

Çözüm:

* slot generation id (v2 önerisi)

### Risk 3: Trigger Lost Wake

Çözüm:

* trigger_flag atomik
* WaitOnAddress expected value kontrolü

---

# 10. Performans Analizi

| Model         | CPU Idle | Context Switch | Latency  |
| ------------- | -------- | -------------- | -------- |
| Spinlock      | Yüksek   | Düşük          | Düşük    |
| WaitOnAddress | 0%       | Kernel         | Orta     |
| Bare HLT      | 0%       | Interrupt      | En düşük |

---

# 11. Gelecek İyileştirmeler (v2)

* Ready bitmask
* Lock-free queue
* NUMA aware slot partition
* Priority-based wake scheduling
* Slot reuse generation counter
* Per-core slot matrix

---

# 12. Sistem Özeti

Bu mimari:

✔ Tek merkezi state
✔ Kernel-level sleep
✔ Zero copy IPC
✔ Deterministik async
✔ Bare-metal uyumlu
✔ Cache line optimized

---



1. 🔥 Formal state diagram çıkaralım
2. 🔥 Memory ordering detaylarını akademik seviyede yazalım
3. 🔥 OCC compiler IR mapping tasarlayalım
4. 🔥 Gerçek C implementasyon skeleton’ı yazalım
5. 🔥 Lock-free ready queue varyantı tasarlayalım


----


**Adım 1 → Formal State Diagram + Deterministic State Model**
---

# 🧠 NXF Thread Runtime

# Bölüm 1 — Formal State Model v1.0

---

# 1️⃣ Nexus_Slot Durum Makinesi

## State Kümesi

```
S = { EMPTY, RUNNING, DONE, TIMEOUT }
```

Binary karşılıkları:

| State   | Value |
| ------- | ----- |
| EMPTY   | 0     |
| RUNNING | 1     |
| DONE    | 2     |
| TIMEOUT | 3     |

---

# 2️⃣ State Transition Diagram

```
          spawn()
  EMPTY ----------> RUNNING
                       |
                       | done()
                       v
                     DONE
                       ^
                       |
                 timeout_expire()
                       |
                    TIMEOUT
```

---

# 3️⃣ Formal Transition Tanımı

Bir slot `σ` için:

### 3.1 Spawn

```
Precondition:
σ.state == EMPTY

Postcondition:
σ.state = RUNNING
counter++
```

---

### 3.2 Done

```
Precondition:
σ.state == RUNNING

Atomic Sequence:
write payload
write pldstatus
memory_barrier
σ.state = DONE
trigger_flag = 1
counter--
```

---

### 3.3 Timeout

```
Precondition:
σ.state == RUNNING
now >= start_time + timeout

Atomic:
σ.state = TIMEOUT
trigger_flag = 1
counter--
```

---

# 4️⃣ Determinism Kuralları

Bu sistem deterministic kabul edilir eğer:

1. DONE veya TIMEOUT yalnızca RUNNING’den gelir.
2. DONE ve TIMEOUT birbirini override edemez.
3. EMPTY → RUNNING geçişi CAS ile yapılır.
4. State asla geriye dönmez.

Yani:

```
EMPTY -> RUNNING -> (DONE | TIMEOUT)
```

Asla:

```
DONE -> RUNNING
TIMEOUT -> RUNNING
```

---

# 5️⃣ Listen State Machine

Listen global bir event-consumer’dır.

## Listen Durumları

```
L = { SLEEPING, SCANNING }
```

---

## Transition

```
trigger_flag == 0
   ↓
SLEEPING

(trigger_flag == 1 OR timeout interval)
   ↓
SCANNING

(no DONE slots left)
   ↓
trigger_flag = 0
SLEEPING
```

---

# 6️⃣ Listen Formal Algoritma

```
loop:

  wait_until(trigger_flag == 1 OR interval_elapsed)

  while ∃ slot.state ∈ {DONE, TIMEOUT}:
      consume(slot)
      slot.state = EMPTY

  trigger_flag = 0
```

---

# 7️⃣ Invariant’ler (Kritik)

Bu sistemde her zaman doğru olması gereken kurallar:

### I1

```
counter == |{σ | σ.state == RUNNING}|
```

### I2

```
trigger_flag == 1 
⇒ ∃ σ ∈ {DONE, TIMEOUT}
```

### I3

Her slot için:

```
σ.state ∈ S
```

Asla undefined state yok.

---

# 8️⃣ Memory Ordering Garantisi

DONE görünür olmadan önce payload görünür olmalı.

Bu yüzden:

```
payload write
pldstatus write
release fence
state = DONE
```

Listener tarafı:

```
acquire read of state
then read payload
```

Bu sayede:

* Torn read olmaz
* Stale payload okunmaz

---

# 9️⃣ Race Condition Analizi

## Case 1: Done vs Timeout yarışırsa?

Çözüm:

```
CAS(RUNNING → DONE)
CAS(RUNNING → TIMEOUT)
```

Sadece biri başarılı olur.

---

## Case 2: Listen scan ederken done gelirse?

Sorun değil.

Çünkü:

* trigger_flag 1 kalır
* next iteration’da tekrar scan edilir

Event kaybı olmaz.

---

# 🔟 Liveness Property

Eğer:

* Thread done çağırırsa
* trigger_flag set edilirse

O görev **eventually** consume edilir.

Deadlock yoktur çünkü:

* Listener kernel sleep kullanır
* Wake garantilidir (WaitOnAddress/futex/IRQ)

---

# 11️⃣ Sistem Karmaşıklığı

Slot sayısı N ise:

* spawn: O(N)
* done: O(1)
* listen scan: O(N)

Eğer N sabitse (örn 256), deterministic upper bound vardır.

---

# 12️⃣ State Model Sonucu

Bu runtime:

✔ Deterministic
✔ Lock-minimal
✔ Wait-free producer
✔ Kernel-sleep consumer
✔ Formal state limited

---


## 🔬 Memory Ordering & CPU-Level Model

* x86 TSO vs ARM Weak Ordering
* Release/Acquire pattern
* Futex wake visibility garantisi
* Lost wake problem matematiksel çözümü
* ABA riskini formalize etme


---


# 🔬 Bölüm 2 — Memory Ordering & CPU-Level Model (Formal)

Bu bölüm doğru değilse sistem *nadiren* bozulur.
Doğru yapılırsa sistem matematiksel olarak güvenlidir.

---

# 1️⃣ Donanım Bellek Modeli Gerçeği

Modern CPU’lar:

* Write’ları yeniden sıralayabilir
* Read’leri öne çekebilir
* Store buffer kullanır
* Cache coherence gecikmeli olabilir

Bu yüzden şu garanti **otomatik gelmez**:

> "state = DONE ise payload kesin yazılmıştır"

Bunu biz sağlamak zorundayız.

---

# 2️⃣ x86 vs ARM Bellek Modeli

## x86 (TSO – Total Store Order)

Özellikler:

* Store → Store sırası korunur
* Load → Load sırası korunur
* Store → Load reorder olabilir
* Güçlü model

Bu yüzden çoğu zaman sadece release store yeterlidir.

---

## ARM (Weak Ordering)

* Store → Store reorder olabilir
* Load → Load reorder olabilir
* Her şey reorder olabilir

ARM’da explicit barrier zorunlu.

---

# 3️⃣ DONE Operasyonunun Doğru Sırası

Amaç:

Listener DONE gördüğünde payload kesin görünür olmalı.

## Doğru Pattern (Producer)

```c
write(payload)
write(pldstatus)
atomic_thread_fence(release)
atomic_store(state, DONE)
```

Release fence garantisi:

> Bundan önceki tüm write’lar DONE’dan önce görünür olur.

---

## Listener Tarafı (Consumer)

```c
if(atomic_load(state, acquire) == DONE) {
    read(payload)
}
```

Acquire garantisi:

> DONE görüldüyse önceki write’lar görünürdür.

---

# 4️⃣ Release/Acquire Olmazsa Ne Olur?

Yanlış senaryo:

```c
state = DONE
payload = X
```

Listener:

```c
if(state == DONE)
   read(payload)  // stale olabilir
```

Bu ARM’da mümkündür.

Yani DONE görülür ama payload eski değer olabilir.

Bu data corruption’dır.

---

# 5️⃣ Futex / WaitOnAddress Visibility

Kritik soru:

Kernel sleep → wake olduğunda memory görünür mü?

Cevap:

✔ Evet, çünkü wake öncesi release store yapılmış olmalı
✔ Wake sonrası acquire load yapılmalı

WaitOnAddress/futex sadece uyandırma mekanizmasıdır.
Memory ordering’i **atomic semantics sağlar**, kernel değil.

---

# 6️⃣ Lost Wake Problemi

## Problem

Listener:

```c
if(trigger_flag == 0)
   sleep()
```

Tam bu anda producer:

```c
trigger_flag = 1
wake()
```

Eğer ordering yanlışsa:

* Listener uyur
* Wake kaybolur
* Deadlock oluşur

---

## Doğru Pattern

```c
while(true){

    if(atomic_load_acquire(trigger_flag) == 0) {
        wait_on_address(trigger_flag, 0);
    }

    scan();
}
```

WaitOnAddress şu mantıkla çalışır:

> Eğer değer hala expected ise uyur
> Değilse hemen döner

Bu yüzden:

* trigger 1 yapılmışsa sleep etmez
* Lost wake olmaz

---

# 7️⃣ trigger_flag Yazımı

Producer tarafı:

```c
atomic_store_release(trigger_flag, 1)
wake_one()
```

Release burada zorunlu.

---

# 8️⃣ Timeout Yarışı (DONE vs TIMEOUT)

İki thread aynı anda state’i değiştirmeye çalışabilir.

Çözüm:

```c
if(atomic_compare_exchange(state, RUNNING, DONE))
    success
```

Timeout thread’i:

```c
if(atomic_compare_exchange(state, RUNNING, TIMEOUT))
    success
```

Sadece biri kazanır.

Bu lock-free yarış çözümüdür.

---

# 9️⃣ ABA Problemi

Slot reuse edilirse:

1. Slot DONE olur
2. Listener EMPTY yapar
3. Yeni task aynı slotu RUNNING yapar
4. Eski referans hala varsa?

Bu ABA’dır.

---

## Çözüm (v2 önerisi)

State’i genişlet:

```c
struct {
    u8 state;
    u8 generation;
}
```

Her EMPTY dönüşünde generation++

Listener eski generation ile eşleşmezse ignore eder.

---

# 🔟 Cache Line False Sharing

Nexus_Slot 32 byte.

Ama modern cache line 64 byte.

Eğer iki slot aynı cache line’daysa:

* İki farklı thread false sharing yapar
* Performans düşer

Öneri:

✔ Slot’u 64 byte yap
✔ Ya da slot array’i 64 byte align et

---

# 11️⃣ Store Buffer Gecikmesi

Producer:

```c
payload write
state = DONE
```

Store buffer flush edilmeden interrupt olursa?

Acquire/Release bunu çözer.

Store buffer flush visibility guarantee verir.

---

# 12️⃣ Bare Metal Memory Model

Interrupt geldiğinde:

* ISR memory barrier içermeli
* DONE yazımı öncesi DMB

ARM bare metal:

```asm
str payload
dmb ish
str state
sev
```

Listener:

```asm
wfe
dmb ish
ldr state
```

---

# 13️⃣ Formal Guarantee

Bu pattern ile:

✔ Data race yok
✔ Lost wake yok
✔ Stale read yok
✔ Double done yok
✔ Deterministic visibility var

---

# 14️⃣ Performans Maliyeti

Acquire/Release:

* x86 → neredeyse ücretsiz
* ARM → hafif barrier maliyeti
* Futex wake → context switch maliyeti

Ama idle CPU = %0

---

# 15️⃣ Bu Runtime’ın Seviyesi

Bu model:

* Rust async executor seviyesinde
* Linux kernel wait queue mantığında
* Lock-free algorithm prensiplerinde
* RTOS class determinism içeriyor

---

# Sonraki Adım

Adım 3:

# 🧩 OCC Compiler IR Mapping

Şunları tasarlayacağız:

* spawn IR karşılığı
* done intrinsic mapping
* !listen lowering
* memory fence injection
* platform-specific backend emission
* bare-metal codegen


---


Yani iki ayrı yürütme modeli var:

* **`!listen()` → Bare-metal event loop**

  * IRQ tabanlı
  * `done()` zorunlu değil
  * Donanım kesmesi doğrudan akışı tetikler
  * Kernel yok, futex yok

* **`listen()` → OS user-space event loop**

  * IRQ’ya erişemez
  * `done()` ile yazılım seviyesi sinyal gerekir
  * WaitOnAddress / futex ile uyur

Bu çok önemli bir ayrım. O zaman formal modeli ikiye ayırıyoruz.

---

# 🧠 Bölüm 3 — Dual Listen Model (IRQ vs OS Signaled)

---

# 1️⃣ Mimari Ayrım

| Özellik       | `!listen()` | `listen()`            |
| ------------- | ----------- | --------------------- |
| Ortam         | Bare Metal  | OS                    |
| Uyanma        | Donanım IRQ | done()                |
| trigger_flag  | Gerekmez    | Zorunlu               |
| done()        | Opsiyonel   | Zorunlu               |
| Uyku          | HLT / WFI   | futex / WaitOnAddress |
| Event Kaynağı | ISR         | Worker thread         |

---

# 2️⃣ !listen() — Bare Metal Model

Burada sistemin kalbi **interrupt controller**’dır.

Akış:

```
CPU → HLT
IRQ gelir
ISR çalışır
Event işaretlenir
Main loop uyanır
!listen bloğu çalışır
```

---

## 2.1 Bare Metal Event Model

IRQ handler:

```c
ISR_Handler() {
    event_register |= DEVICE_EVENT;
}
```

Ana döngü:

```c
while(1) {

    if(event_register == 0) {
        asm("hlt");   // WFI
    }

    if(event_register & DEVICE_EVENT) {
        event_register &= ~DEVICE_EVENT;
        handle_device();
    }
}
```

Burada:

* `done()` yok
* trigger_flag yok
* Slot tarama zorunlu değil
* IRQ zaten wake garantili

---

## 2.2 !listen() Formal Tanım

```text
Precondition:
Interrupt source exists

Wake Condition:
Hardware interrupt

Invariant:
CPU sleeps only when no pending IRQ
```

Bu modelde:

✔ Lost wake imkansız (çünkü IRQ latch edilir)
✔ Atomic flag yeterlidir
✔ Memory fence ISR sonunda yapılmalı

ARM ISR sonunda:

```asm
dmb ish
```

---

# 3️⃣ listen() — OS Model

User space IRQ’ya erişemez.

Bu yüzden yazılım sinyali gerekir → `done()`.

Akış:

```
Worker thread
  ↓
write payload
release
state = DONE
trigger_flag = 1
wake syscall
  ↓
listen() uyanır
scan slots
consume
```

---

## 3.1 listen() Formal Model

```text
Wake Condition:
trigger_flag == 1

Signal Source:
done()
```

Pseudo:

```c
while(true){

    if(trigger_flag == 0)
        wait_on_address(trigger_flag, 0);

    scan_slots();

    trigger_flag = 0;
}
```

---

# 4️⃣ En Kritik Fark

## Bare Metal

Event kaynağı: **Donanım**

## OS

Event kaynağı: **Yazılım (done)**

---

# 5️⃣ Çok Önemli Soru

Bare metal modda spawn edilen async task ne olur?

Cevap:

İki model var:

### Model A — ISR Driven Tasks

Task donanım interrupt içinde tamamlanır
`done()` gerekmez

### Model B — Soft Task (Thread Simülasyonu)

Eğer cooperative task varsa
`done()` yine gerekir
Ama wake SEV ile olur

---

# 6️⃣ Birleşik Runtime Modeli

Artık sistem iki backend'e ayrılıyor:

```
Runtime Core
    |
    |--- BareMetalBackend (!listen)
    |
    |--- OSBackend (listen)
```

Compiler burada kritik.

---

# 7️⃣ OCC Compiler Davranışı

### Eğer target == baremetal

* `!listen()` → IRQ event loop
* `listen()` compile edilmez (error veya alias)

### Eğer target == OS

* `listen()` → futex/wait
* `!listen()` illegal

Bu ayrım compile-time yapılmalı.

---

# 8️⃣ Artık Netleşti

### `!listen()`

* Kernel yok
* IRQ var
* done zorunlu değil
* trigger_flag gereksiz
* HLT/WFI kullanır

### `listen()`

* Kernel var
* IRQ yok
* done zorunlu
* trigger_flag zorunlu
* WaitOnAddress/futex kullanır

---

# 9️⃣ Performans Kıyas

| Model       | Latency  | CPU Idle | Determinism |
| ----------- | -------- | -------- | ----------- |
| !listen IRQ | En düşük | %0       | En yüksek   |
| listen OS   | Orta     | %0       | Yüksek      |

---

# 🔥 Şimdi Çok Önemli Bir Tasarım Kararı

Nexus_Slot Matrix:

Bare metal modda gerekli mi?

Seçenekler:

1. ✔ Sadece OS modda kullan
2. ✔ Bare metalde de kullan (soft-task için)
3. ✔ Conditional compile

Bence doğru çözüm:

> Core runtime slot sistemi hep var
> !listen IRQ modeli sadece wake mekanizmasını değiştirir

Yani veri modeli ortak, wake modeli farklı.

---

# 🚀 Sonraki Adım

Adım 4:

# 🧩 OCC Compiler IR & Lowering Model

Tasarlayacağız:

* spawn → IR expansion
* done → intrinsic mapping
* listen vs !listen codegen farkı
* fence injection logic
* backend abstraction layer
* syscall emission planı


----

> `no:CoreModule;` varsa → Bare Metal Mode
> `CoreModule` yoksa → OS Mode
> İkisi aynı anda **imkansız** (compile-time ayrışma)

---

# 🧩 Bölüm 4 — OCC Compiler IR & Backend Lowering Model

---

# 1️⃣ Compilation Mode Resolution

## 1.1 Mode Determination

```text
if (no:CoreModule;)
    TARGET = BARE_METAL
else
    TARGET = OS
```

Bu karar:

* IR generation aşamasında alınır
* Backend sabitlenir
* Cross-call yasaklanır

---

# 2️⃣ Language Construct → IR Mapping

| Dil Yapısı | IR Primitive  | Backend   |
| ---------- | ------------- | --------- |
| spawn()    | IR_SPAWN      | OS + Bare |
| done()     | IR_DONE       | OS + Bare |
| listen()   | IR_LISTEN_OS  | OS only   |
| !listen()  | IR_LISTEN_IRQ | Bare only |

---

# 3️⃣ spawn() Lowering

## Source

```nxf
(h) <- spawn(task(args), async, timeout);
```

---

## IR Expansion

```c
IR_ALLOC_SLOT
IR_SET_RUNNING
IR_INC_COUNTER
IR_BACKEND_THREAD_CREATE
```

---

## OS Backend Emission

```c id="l1os2q"
slot = alloc_slot()
slot.state = RUNNING
atomic_fetch_add(counter, 1)

CreateThread(task_wrapper, slot)
```

Linux:

```c
clone() or pthread_create()
```

Windows:

```c
CreateThread()
```

---

## Bare Metal Backend Emission

Thread yok.

İki seçenek:

### Model 1 — Cooperative

```c id="o9j2dk"
task_queue_push(fn_ptr, args)
```

Main loop içinde çağrılır.

### Model 2 — Soft Scheduler

Stack pointer hazırlanır
Context switch minimal yapılır

---

# 4️⃣ done() Lowering

## Source

```nxf
done(type, payload, mem_addr);
```

---

## IR

```c
IR_WRITE_PAYLOAD
IR_RELEASE_FENCE
IR_SET_DONE
IR_SIGNAL
IR_DEC_COUNTER
IR_EXIT_THREAD
```

---

## OS Backend

```c id="qz0b4n"
write(payload)
atomic_thread_fence(release)
atomic_store(slot.state, DONE)

atomic_store(trigger_flag, 1)
wake_waiters()

atomic_fetch_sub(counter, 1)
thread_exit()
```

wake_waiters():

* Windows → WakeByAddressSingle
* Linux → futex(FUTEX_WAKE)

---

## Bare Metal Backend

```c id="yzb4w3"
write(payload)
dmb ish
slot.state = DONE

counter--
```

Wake yok.

Çünkü IRQ zaten wake kaynağı.

Eğer cooperative ise:

```c
software_event_flag = 1
sev   // ARM
```

---

# 5️⃣ listen() vs !listen() Lowering

---

# 5.1 listen() (OS Mode)

## Source

```nxf
listen(h, 10) >> { ... }
```

---

## IR

```c
IR_WAIT_TRIGGER
IR_SCAN_SLOT(h)
IR_EXEC_BLOCK
IR_CLEAR_TRIGGER
```

---

## OS Emission

```c id="as9d20"
while(true){

    if(atomic_load(trigger_flag) == 0)
        wait_on_address(trigger_flag, 0)

    if(slot.state == DONE){
        execute_block()
        slot.state = EMPTY
    }

    trigger_flag = 0
}
```

---

# 5.2 !listen() (Bare Metal Mode)

## Source

```nxf
!listen(h, 0) >> { ... }
```

---

## IR

```c
IR_IRQ_WAIT
IR_CHECK_EVENT_REGISTER
IR_EXEC_BLOCK
```

---

## Bare Emission

```c id="9lmf03"
while(true){

    if(event_register == 0)
        asm("wfi")

    if(slot.state == DONE){
        execute_block()
        slot.state = EMPTY
    }
}
```

trigger_flag YOK.

Wake = IRQ.

---

# 6️⃣ Compile-Time Validation Rules

Compiler şunları yasaklamalı:

### Rule 1

```text
TARGET = BARE_METAL
listen() kullanılamaz
```

### Rule 2

```text
TARGET = OS
!listen() kullanılamaz
```

### Rule 3

```text
no:CoreModule; + OS import → compile error
```

---

# 7️⃣ Fence Injection Logic

Compiler şu noktada otomatik fence ekler:

### DONE öncesi

```c
atomic_thread_fence(release)
```

### LISTEN read sırasında

```c
atomic_load_acquire(state)
```

Programcı barrier yazmaz.

---

# 8️⃣ Backend Abstraction Layer

Compiler şu interface’i üretir:

```c id="gaxq8p"
backend_thread_create()
backend_sleep()
backend_wake()
backend_fence_release()
backend_fence_acquire()
```

OS backend bunları syscall ile doldurur.
Bare backend bunları asm ile doldurur.

---

# 9️⃣ Deterministic Compilation Property

Bu tasarım sayesinde:

* Runtime branching yok
* if(OS) vs if(Bare) yok
* Dead code yok
* Binary saf
* Size minimal

---

# 🔟 Binary Boyut Avantajı

Bare metal build:

✔ libc yok
✔ futex yok
✔ thread lib yok
✔ sadece core runtime

OS build:

✔ IRQ kodu yok
✔ HLT yok
✔ ISR yok

---

# 11️⃣ Artık Sistem Net

Bu mimari:

* Dual backend
* Compile-time ayrışmış
* Deterministic
* Lock-minimal
* IRQ-aware
* Zero-runtime polymorphism

---

# 🚀 Bir Sonraki Adım

Şimdi kritik aşamaya geldik.

Adım 5 seçenekleri:

1. 🔥 Lock-free Ready Queue tasarımı (O(N) scan kaldırılır)
2. 🔥 Soft Scheduler (Bare Metal Context Switch modeli)
3. 🔥 Timeout Wheel / Timer Subsystem
4. 🔥 Priority-aware scheduling
5. 🔥 Full minimal C runtime skeleton

---



# 🚀 Bölüm 6 — Word-Size Adaptive Ready Map Architecture

---

# 1️⃣ Temel Fikir

Ready queue’yu ring buffer yerine:

> Word-sized Bitmask Pages

olarak tasarlıyoruz.

---

## Word Size = Target'a bağlı

| Arch   | Word Size | Page Capacity |
| ------ | --------- | ------------- |
| x86_64 | 64 bit    | 64 slot       |
| ARM64  | 64 bit    | 64 slot       |
| x86    | 32 bit    | 32 slot       |
| ARM32  | 32 bit    | 32 slot       |

---

# 2️⃣ Slot Organizasyonu

Her **page** bir word ile temsil edilir.

Örnek (64-bit CPU):

```
Page 0 → slots 0-63
Page 1 → slots 64-127
Page 2 → slots 128-191
...
```

Her page:

```c
atomic_u64 ready_mask;
```

---

# 3️⃣ DONE İşlemi (Producer)

Slot ID = 130 diyelim.

Hesap:

```c
page = slot_id / WORD_BITS
bit  = slot_id % WORD_BITS
```

Sonra:

```c
atomic_fetch_or(&ready_page[page], (1ULL << bit));
```

Son olarak:

```c
trigger_flag = 1;
wake();
```

---

# 4️⃣ LISTEN Tarafı (Consumer)

```c
for each page:

    mask = atomic_exchange(&ready_page[i], 0);

    while(mask != 0){
        bit = ctz(mask);     // count trailing zeros
        slot_id = i*WORD_BITS + bit;
        mask &= ~(1ULL << bit);

        consume(slot_id);
    }
```

---

# 5️⃣ Avantajlar

✔ Ring buffer yok
✔ Overflow imkansız
✔ Memory sabit
✔ Lock-free
✔ Atomic OR yeterli
✔ O(K) complexity
✔ False sharing minimal

---

# 6️⃣ Dinamik Ölçekleme (64+64+64 Modeli)

Slot sayısı sabit değil.

Target belirler.

---

## Target Dosyasından Gelen Parametre

Örnek:

```
arch="x86_64"
max_threads=8
max_slots=512
```

Compiler hesaplar:

```
WORD_BITS = 64
PAGE_COUNT = max_slots / 64
```

---

# 7️⃣ 32-bit Sistem

```
WORD_BITS = 32
```

Page kapasitesi 32 olur.

Aynı kod compile edilir ama:

```c
typedef atomic_uint32_t ready_word;
```

Compiler bunu target dosyasından bilir.

---

# 8️⃣ Bellek Yapısı

```c
struct Runtime {

    Nexus_Slot slots[MAX_SLOTS];

    alignas(64)
    ready_word ready_pages[PAGE_COUNT];

    atomic_u8 trigger_flag;
}
```

---

# 9️⃣ Bu Modelin Matematiksel Garantisi

Her slot DONE olmadan tekrar DONE olamaz.

Bu yüzden:

* Aynı bit tekrar set edilmez
* Queue overflow olmaz
* Ring buffer taşması imkansız

---

# 🔟 Performans

Bit-scan CPU instruction kullanır:

x86:

```
tzcnt
bsf
```

ARM:

```
rbit + clz
```

Bu O(1).

---

# 11️⃣ Ring Buffer vs Ready Map

| Özellik        | Ring   | Ready Map       |
| -------------- | ------ | --------------- |
| Overflow riski | Var    | Yok             |
| FIFO           | Var    | Yok (bit order) |
| Memory         | QSIZE  | Word pages      |
| Determinism    | Yüksek | Yüksek          |
| Complexity     | Orta   | Daha basit      |

FIFO kaybolur ama determinism korunur.

Eğer priority sistemi gelecekse bu daha avantajlı.

---

# 12️⃣ Priority ile Uyum

Hazır mimari.

Eğer slot.priority kullanacaksak:

* Her priority için ayrı ready_page
* Ya da page scanning high→low

Bu model çok esnek.

---

# 13️⃣ Trigger Flag Hala Gerekli mi?

OS modda evet.

Bare metal IRQ modda:

* trigger_flag gereksiz
* IRQ zaten wake ediyor

Ama ready_pages her iki modda da ortak kalabilir.

Bu çok temiz bir birleşim.

---

# 14️⃣ Target-Aware Compilation

Compiler şunu yapar:

```text
WORD_BITS = sizeof(void*) * 8
```

Ama override edilebilir:

```
word_bits=64
```

.target dosyası ultimate authority.

---

# 15️⃣ Büyük Ölçek (Örn 4096 Slot)

x86_64:

4096 / 64 = 64 page
64 adet atomic_u64

Toplam:

64 * 8 byte = 512 byte

Çok küçük.

---

# 16️⃣ Şimdi Sistem Seviyesi

Bu noktada runtime:

✔ Scanless
✔ Overflow-proof
✔ Word-size adaptive
✔ Target-aware
✔ Lock-free
✔ Deterministic
✔ IRQ ve OS uyumlu

Bu artık kernel-class tasarım.

---

# 🔥 Sıradaki Kritik Hamle

Artık iki büyük alan kaldı:

1. 🔥 Priority-aware scheduling (ready map ile entegre)
2. 🔥 Timeout Wheel (slot bazlı zamanlayıcı sistemi)
3. 🔥 Gerçek soft scheduler (bare-metal mini RTOS seviyesi)
4. 🔥 Full formal spec dökümanı üretme

---

# 🚀 Bölüm 7 — Priority-Aware Ready Map Architecture

---

# 1️⃣ Problem

Şu an:

```text
DONE olan slot → ready_page[page] bit set
```

Listener bit-scan ile tüketiyor.

Ama:

> priority alanı aktif kullanılmıyor.

---

# 2️⃣ Hedef

✔ Yüksek öncelikli görev önce çalışmalı
✔ O(1) kalmalı
✔ Lock-free kalmalı
✔ Target-aware kalmalı
✔ Scan yok

---

# 3️⃣ Basit Ama Güçlü Çözüm

Tek ready map yerine:

> Çok katmanlı ready map (Priority Buckets)

---

# 4️⃣ Priority Bucket Model

Priority 0–255 arası.

Ama 256 seviye pahalı olur.

Onun yerine:

## Configurable Priority Levels

Target dosyasından:

```ini
priority_levels=4
```

Örnek:

| Level | Slot Priority Aralığı |
| ----- | --------------------- |
| 0     | 192–255               |
| 1     | 128–191               |
| 2     | 64–127                |
| 3     | 0–63                  |

Compiler mapping yapar.

---

# 5️⃣ Veri Yapısı

```c
struct ReadyLayer {
    ready_word pages[PAGE_COUNT];
};

struct Scheduler {

    ReadyLayer layers[PRIORITY_LEVELS];

    atomic_word global_ready_mask; 
}
```

---

# 6️⃣ global_ready_mask Nedir?

Her bit = bir layer’da iş var.

64-bit sistemde:

* 64 priority layer desteklenebilir
* Biz genelde 4–8 kullanacağız

DONE sırasında:

```c
layer = map_priority(slot.priority)

atomic_fetch_or(&layers[layer].pages[page], bitmask)

atomic_fetch_or(&global_ready_mask, (1ULL << layer))
```

---

# 7️⃣ Listener Tarafı

```c
while(global_ready_mask != 0){

    highest_layer = highest_set_bit(global_ready_mask)

    for each page in that layer:

        mask = atomic_exchange(page, 0)

        while(mask != 0){
            bit = ctz(mask)
            slot_id = ...
            consume(slot_id)
        }

    if(layer empty)
        clear bit in global_ready_mask
}
```

---

# 8️⃣ En Kritik Özellik

Bu tasarım:

✔ O(K)
✔ O(1) highest priority selection
✔ Scan yok
✔ FIFO gerekmiyor
✔ Deterministic

---

# 9️⃣ FIFO Gerekli mi?

Gerçek zamanlı sistemlerde:

> Öncelik > FIFO

Eğer FIFO istenirse:

* Aynı layer içinde slot_id sırası deterministik kalır
* Word içindeki bit sırası sabittir

Bu yeterlidir.

---

# 🔟 Starvation Problemi

Yüksek priority sürekli gelirse
Düşük priority aç kalabilir.

Çözüm opsiyonları:

### A) Aging

Her X tick’te layer düşür.

### B) Quantum Limiti

```c
max_high_prio_per_cycle=8
```

Sonra lower layer’a geç.

Target dosyasından ayarlanabilir.

---

# 11️⃣ Target-Aware Priority Config

.target dosyası:

```ini
priority_levels=8
scheduler_policy="strict"
```

Alternatif:

```ini
scheduler_policy="fair"
```

Compiler buna göre aging kodu ekler.

---

# 12️⃣ Memory Footprint

Örnek:

max_slots = 512
64-bit system

PAGE_COUNT = 512 / 64 = 8

priority_levels = 4

Toplam ready map:

4 * 8 * 8 byte = 256 byte

Çok küçük.

---

# 13️⃣ OS ve Bare Uyumu

### OS Mode:

wake → trigger_flag

### Bare Mode:

wake → IRQ

Ama scheduler core aynıdır.

Bu önemli.

---

# 14️⃣ Determinism Korundu mu?

Evet.

Worst-case:

* Layer sayısı sabit
* Page sayısı sabit
* Word scan bounded
* No dynamic allocation
* No unbounded queue

---

# 15️⃣ Gerçek Scheduler Seviyesi

Bu noktada sistem:

✔ Lock-free multi-layer
✔ Priority aware
✔ Bounded
✔ Target adaptive
✔ IRQ aware
✔ Zero malloc
✔ Zero spin

Bu artık:

> Embedded RTOS sınıfı bir scheduler

Ama daha minimal.

---

# 16️⃣ Bir Üst Seviye İyileştirme

Şimdi sistem şu özelliklere hazır:

* priority
* ready map
* slot reuse
* dual backend

---


> ⏱ Timeout sistemi
> Ama O(N) değil.
> O(1) deterministic olacak.
> Priority ve ready-map ile entegre olacak.
> OS ve Bare backend ile uyumlu olacak.

---

# 🚀 Bölüm 8 — Hierarchical Timing Wheel (Deterministic O(1) Timeout)

---

# 1️⃣ Problem

Şu an timeout şöyle çalışıyor varsayalım:

```text
Her tick → tüm RUNNING slotları kontrol et
```

Bu O(N).

512 slot varsa her tick 512 kontrol.

Kabul edilemez.

---

# 2️⃣ Hedef

✔ Timeout ekleme O(1)
✔ Timeout tetikleme O(1)
✔ Deterministic
✔ No heap
✔ No dynamic allocation
✔ Target-aware

---

# 3️⃣ Çözüm: Timing Wheel

Linux kernel’in kullandığı mantık:

> Zamanı bucket’lara böl
> Her tick bir bucket ilerle
> Süresi dolanlar o bucket’ta hazırdır

---

# 4️⃣ Basit Timing Wheel (Level 0)

Örnek:

* Tick = 1ms
* Wheel size = 256 bucket

```text
[0][1][2]...[255]
```

current_tick pointer ilerler.

---

# 5️⃣ Veri Yapısı

```c id="k9wdh2"
struct TimerWheel {

    u32 current_tick;

    u32 bucket_count;

    u64 buckets[WHEEL_SIZE];  // slot bitmask
}
```

Her bucket → word-size ready bitmask.

---

# 6️⃣ Timeout Ekleme

Slot timeout_ms = 37

```c id="pq5t3d"
expire_tick = current_tick + timeout_ms
bucket = expire_tick % WHEEL_SIZE
bit = slot_id % WORD_BITS

atomic_fetch_or(&buckets[bucket], (1ULL << bit))
```

O(1)

---

# 7️⃣ Tick İlerlemesi

Her 1ms:

```c id="znz2y3"
current_tick++
bucket = current_tick % WHEEL_SIZE

expired_mask = atomic_exchange(&buckets[bucket], 0)

while(expired_mask){
    bit = ctz(expired_mask)
    slot_id = ...
    mark TIMEOUT
}
```

---

# 8️⃣ Ama Problem Var

Wheel 256 ise:

Max timeout = 255ms

Daha büyük timeout?

---

# 9️⃣ Çözüm: Hierarchical Timing Wheel

Level 0 → 256 x 1ms = 256ms
Level 1 → 256 x 256ms
Level 2 → 256 x 256² ms
...

---

# 🔟 3-Level Örnek

```text
Level 0 → 1ms resolution
Level 1 → 256ms resolution
Level 2 → 65s resolution
```

Bu yeterli çoğu sistem için.

---

# 11️⃣ Veri Yapısı

```c id="hff6xk"
struct TimingWheel {

    Wheel L0;
    Wheel L1;
    Wheel L2;

}
```

---

# 12️⃣ Timeout Yerleştirme

Eğer timeout < 256ms → L0
Eğer < 65536ms → L1
Yoksa L2

---

# 13️⃣ Cascade Mekanizması

L1 bucket dolunca:

Slot L0’a indirilir.

Bu da O(1) amortized.

---

# 14️⃣ Timeout Olayı

Timeout tetiklenince:

```c id="v6re2f"
if(CAS(state, RUNNING → TIMEOUT)){

    enqueue_ready_map(slot_id)
}
```

DONE ile yarışır.

Hangisi kazanırsa o geçerli.

---

# 15️⃣ OS vs Bare Tick Kaynağı

## OS Mode

Tick kaynağı:

* timerfd (Linux)
* WaitableTimer (Windows)
* ualarm
* POSIX timer

listen() thread tick üretir.

---

## Bare Metal Mode

Tick kaynağı:

* SysTick
* PIT
* HPET
* ARM generic timer

ISR:

```c id="h5v6ux"
SysTick_Handler(){

    advance_wheel();
}
```

IRQ zaten wake eder.

---

# 16️⃣ Determinism

Worst case tick cost:

* 1 bucket scan
* Word-size bit scan
* Ready enqueue

Bounded.

No heap.

No linked list.

No malloc.

---

# 17️⃣ Memory Footprint

Örnek:

WHEEL_SIZE=256
WORD_BITS=64

256 * 8 byte = 2KB per level

3 level → 6KB

Embedded için gayet makul.

---

# 18️⃣ Slot ile Entegrasyon

Slot’a şu alan eklenir:

```c id="vpxy5x"
u32 timeout_expire_tick;
```

done() olursa:

Timeout bucket’tan silmeye gerek yok.

Çünkü:

CAS(RUNNING→TIMEOUT) başarısız olur.

Bit temizlenmese bile harmless.

---

# 19️⃣ Artık Sistem Tam

Şu an runtime:

✔ Word-adaptive ready map
✔ Multi-layer priority
✔ Hierarchical timing wheel
✔ IRQ aware
✔ OS aware
✔ Lock-free
✔ Deterministic
✔ Bounded memory
✔ Zero malloc
---


> ⏱ Timeout sistemi
> Ama O(N) değil.
> O(1) deterministic olacak.
> Priority ve ready-map ile entegre olacak.
> OS ve Bare backend ile uyumlu olacak.

---

# 🚀 Bölüm 8 — Hierarchical Timing Wheel (Deterministic O(1) Timeout)

---

# 1️⃣ Problem

Şu an timeout şöyle çalışıyor varsayalım:

```text
Her tick → tüm RUNNING slotları kontrol et
```

Bu O(N).

512 slot varsa her tick 512 kontrol.

Kabul edilemez.

---

# 2️⃣ Hedef

✔ Timeout ekleme O(1)
✔ Timeout tetikleme O(1)
✔ Deterministic
✔ No heap
✔ No dynamic allocation
✔ Target-aware

---

# 3️⃣ Çözüm: Timing Wheel

Linux kernel’in kullandığı mantık:

> Zamanı bucket’lara böl
> Her tick bir bucket ilerle
> Süresi dolanlar o bucket’ta hazırdır

---

# 4️⃣ Basit Timing Wheel (Level 0)

Örnek:

* Tick = 1ms
* Wheel size = 256 bucket

```text
[0][1][2]...[255]
```

current_tick pointer ilerler.

---

# 5️⃣ Veri Yapısı

```c id="k9wdh2"
struct TimerWheel {

    u32 current_tick;

    u32 bucket_count;

    u64 buckets[WHEEL_SIZE];  // slot bitmask
}
```

Her bucket → word-size ready bitmask.

---

# 6️⃣ Timeout Ekleme

Slot timeout_ms = 37

```c id="pq5t3d"
expire_tick = current_tick + timeout_ms
bucket = expire_tick % WHEEL_SIZE
bit = slot_id % WORD_BITS

atomic_fetch_or(&buckets[bucket], (1ULL << bit))
```

O(1)

---

# 7️⃣ Tick İlerlemesi

Her 1ms:

```c id="znz2y3"
current_tick++
bucket = current_tick % WHEEL_SIZE

expired_mask = atomic_exchange(&buckets[bucket], 0)

while(expired_mask){
    bit = ctz(expired_mask)
    slot_id = ...
    mark TIMEOUT
}
```

---

# 8️⃣ Ama Problem Var

Wheel 256 ise:

Max timeout = 255ms

Daha büyük timeout?

---

# 9️⃣ Çözüm: Hierarchical Timing Wheel

Level 0 → 256 x 1ms = 256ms
Level 1 → 256 x 256ms
Level 2 → 256 x 256² ms
...

---

# 🔟 3-Level Örnek

```text
Level 0 → 1ms resolution
Level 1 → 256ms resolution
Level 2 → 65s resolution
```

Bu yeterli çoğu sistem için.

---

# 11️⃣ Veri Yapısı

```c id="hff6xk"
struct TimingWheel {

    Wheel L0;
    Wheel L1;
    Wheel L2;

}
```

---

# 12️⃣ Timeout Yerleştirme

Eğer timeout < 256ms → L0
Eğer < 65536ms → L1
Yoksa L2

---

# 13️⃣ Cascade Mekanizması

L1 bucket dolunca:

Slot L0’a indirilir.

Bu da O(1) amortized.

---

# 14️⃣ Timeout Olayı

Timeout tetiklenince:

```c id="v6re2f"
if(CAS(state, RUNNING → TIMEOUT)){

    enqueue_ready_map(slot_id)
}
```

DONE ile yarışır.

Hangisi kazanırsa o geçerli.

---

# 15️⃣ OS vs Bare Tick Kaynağı

## OS Mode

Tick kaynağı:

* timerfd (Linux)
* WaitableTimer (Windows)
* ualarm
* POSIX timer

listen() thread tick üretir.

---

## Bare Metal Mode

Tick kaynağı:

* SysTick
* PIT
* HPET
* ARM generic timer

ISR:

```c id="h5v6ux"
SysTick_Handler(){

    advance_wheel();
}
```

IRQ zaten wake eder.

---

# 16️⃣ Determinism

Worst case tick cost:

* 1 bucket scan
* Word-size bit scan
* Ready enqueue

Bounded.

No heap.

No linked list.

No malloc.

---

# 17️⃣ Memory Footprint

Örnek:

WHEEL_SIZE=256
WORD_BITS=64

256 * 8 byte = 2KB per level

3 level → 6KB

Embedded için gayet makul.

---

# 18️⃣ Slot ile Entegrasyon

Slot’a şu alan eklenir:

```c id="vpxy5x"
u32 timeout_expire_tick;
```

done() olursa:

Timeout bucket’tan silmeye gerek yok.

Çünkü:

CAS(RUNNING→TIMEOUT) başarısız olur.

Bit temizlenmese bile harmless.

---

# 19️⃣ Artık Sistem Tam

Şu an runtime:

✔ Word-adaptive ready map
✔ Multi-layer priority
✔ Hierarchical timing wheel
✔ IRQ aware
✔ OS aware
✔ Lock-free
✔ Deterministic
✔ Bounded memory
✔ Zero malloc

Bu artık mini-kernel seviyesi runtime.

---

1️⃣ Soft context switch (bare-metal mini RTOS)
2️⃣ Full Formal Specification v1.0 