# OCC Intermediate Representation (OIR) Specification

## 1. Introduction

This document defines the **OCC Intermediate Representation (OIR)** for the Thread System Runtime, designed for both **OS-dependent** and **Bare-metal** targets.  
The specification aims to outline:

- A formal instruction set (Ops)
- Type system
- Memory semantics and atomic operations
- Mapping to OS/Bare-metal backends
- The IR graph structure and determinism guarantees

---

## 2. Instruction Set

The OIR instruction set consists of operations for task management, synchronization, memory operations, and context switching.

### 2.1 `spawn slot_id, func, arg, timeout`

- **Operands**:
    - `slot_id`: The thread slot identifier (32/64-bit unsigned integer).
    - `func`: Pointer to the entry function of the thread.
    - `arg`: Argument for the function.
    - `timeout`: Timeout in milliseconds.
- **Semantics**: Allocates a new thread slot, sets it to `RUNNING`, and inserts it into the scheduler's ready map.
- **Target Mapping**:  
    - **OS**: Uses `CreateThread` (Windows) / `pthread_create` (Linux/macOS).
    - **Bare-metal**: Allocates stack, initializes TCB, and prepares execution.

### 2.2 `done slot_id, type, payload`

- **Operands**:
    - `slot_id`: The thread slot identifier.
    - `type`: Payload type (`RAW`, `pointer`, `file_path`).
    - `payload`: Data (memory address or path).
- **Semantics**: Marks the slot as `DONE`, enqueues the slot into the ready map, and signals that the task is complete.
- **Target Mapping**:
    - **OS**: CAS (Compare-and-swap) for state change, updates the ready map.
    - **Bare-metal**: Directly updates the slot state and the ready map.

### 2.3 `timeout_check slot_id`

- **Operands**:
    - `slot_id`: The thread slot identifier.
- **Semantics**: Checks whether the thread’s timeout has expired. If expired, attempts to change the state to `TIMEOUT` using CAS.
- **Target Mapping**:
    - **OS**: Uses `futex` or equivalent to check the timeout.
    - **Bare-metal**: Increments the tick count, checks the current slot’s timeout.

### 2.4 `listen layer_mask, max_slots`

- **Operands**:
    - `layer_mask`: A mask indicating which priority layers to check.
    - `max_slots`: The maximum number of slots to process.
- **Semantics**: Listens for the ready slots in the specified layers, consumes them in order of priority.
- **Target Mapping**:
    - **OS**: Uses `WaitOnAddress` (Windows) or `futex` (Linux).
    - **Bare-metal**: Polling using a custom IRQ system, WFI if no ready slots.

### 2.5 `!listen layer_mask`

- **Operands**:
    - `layer_mask`: Mask indicating which priority layers to listen for.
- **Semantics**: In Bare-metal mode, poll for ready slots in the specified layers. Uses IRQ to wake up the processor if needed.
- **Target Mapping**:
    - **Bare-metal**: Uses `WFI` (Wait For Interrupt) or direct polling of the ready map.

### 2.6 `atomic_cas addr, old, new`

- **Operands**:
    - `addr`: Memory address for the CAS operation.
    - `old`: Expected old value.
    - `new`: New value to set.
- **Semantics**: Performs an atomic compare-and-swap on the given memory address.
- **Target Mapping**:
    - **OS**: Platform-dependent atomic operations (e.g., `InterlockedCompareExchange` on Windows).
    - **Bare-metal**: Direct assembly-based CAS or platform-specific atomic operations.

### 2.7 `atomic_fetch_or addr, mask`

- **Operands**:
    - `addr`: Memory address for the atomic OR operation.
    - `mask`: Bitmask to OR with the value at `addr`.
- **Semantics**: Performs an atomic OR operation on the specified address.
- **Target Mapping**:
    - **OS**: Uses atomic fetch-or operation in the OS API.
    - **Bare-metal**: Direct bitwise manipulation on memory.

### 2.8 `advance_timer level`

- **Operands**:
    - `level`: The timer wheel level to advance (0: 1ms, 1: 256ms, 2: 65s).
- **Semantics**: Advances the hierarchical timing wheel by one tick for the given level.
- **Target Mapping**:
    - **OS**: Platform-specific timer system (e.g., `timerfd` in Linux).
    - **Bare-metal**: Uses hardware timers (e.g., `SysTick`, `PIT`).

### 2.9 `context_save tcb`

- **Operands**:
    - `tcb`: Pointer to the Task Control Block (TCB) where the context should be saved.
- **Semantics**: Saves the current task's registers and stack pointer into the provided TCB.
- **Target Mapping**:
    - **Bare-metal**: Uses platform-specific register-saving routines (e.g., `PendSV_Handler` on ARM).

### 2.10 `context_load tcb`

- **Operands**:
    - `tcb`: Pointer to the Task Control Block (TCB) from which the context will be loaded.
- **Semantics**: Loads the saved context (registers, stack pointer) from the TCB and restores the execution state.
- **Target Mapping**:
    - **Bare-metal**: Uses platform-specific restore operations after context switch.

### 2.11 `yield`

- **Operands**:
    - None.
- **Semantics**: Explicit cooperative yield point for cooperative scheduling.
- **Target Mapping**:
    - **Bare-metal**: Context switch initiated by calling `PendSV_Handler` (ARM).

---

## 3. Type System

- **slot_id**: A 32/64-bit unsigned integer identifying a thread.
- **state**: Enum value representing the state of a slot (`EMPTY`, `RUNNING`, `DONE`, `TIMEOUT`).
- **payload**: A pointer to the task’s data or file path (`RAW`, `pointer`, `file_path`).
- **layer_id**: 8-bit identifier for the priority layer.
- **mask**: 32/64-bit bitmask used for operations like `atomic_fetch_or` or `atomic_cas`.
- **tick**: 32-bit value representing the current tick in the timer system.

---

## 4. Memory Semantics

- **Atomic Operations**: Every operation involving shared memory (e.g., `ready_map`, `trigger_flag`, `timer_buckets`) must use atomic operations (`atomic_cas`, `atomic_fetch_or`, etc.) to ensure safe concurrency.
- **Memory Fences**: The `done` operation uses release-acquire memory fences to ensure proper synchronization between threads. This guarantees that all writes to the payload are visible to the consumer when `done` is called.
- **No Heap Allocation**: The system does not use dynamic memory allocation (except for statically defined memory regions like TCB and stack space). All memory is either statically allocated or uses fixed-size regions.
- **Alignment**: Memory is aligned according to the platform's word size (`32-bit` or `64-bit`), ensuring that operations on aligned data types are efficient.

---

## 5. Backend Mapping

### 5.1 OS Mode

- **Thread Creation**: `CreateThread` (Windows) or `pthread_create` (Linux/macOS).
- **Synchronization**: Uses `WaitOnAddress` (Windows), `futex` (Linux), or `ulock_wait` (macOS).
- **Timer Management**: Uses OS-level timers (`timerfd`, `SetWaitableTimer`).
- **Context Switching**: Uses OS thread scheduling (preemptive or cooperative).

### 5.2 Bare-metal Mode

- **Thread Creation**: Allocates a stack and a task control block (TCB) manually.
- **Synchronization**: Uses IRQ and hardware timers.
- **Timer Management**: Uses platform-specific timers (`SysTick`, `PIT`, `HPET`).
- **Context Switching**: Software-based context switching using a custom interrupt service routine (ISR) and PendSV handler (for ARM).

---


## 6. IR Graph

The scheduler/task dependency view in this section is represented as a **directed acyclic graph (DAG)**, where nodes are runtime task operations and edges represent execution or data dependencies. This DAG is not the canonical per-function control-flow representation used by the OCC compiler. Function control flow is represented by a control-flow graph (CFG) with basic blocks and explicit terminators; CFG cycles created by `loop` or `jump(label)` are valid when their edges and SSA/block-parameter relationships pass verification. The scheduler dependency DAG and function CFG are distinct graph models.

* Deterministic execution
* Precise scheduling
* Analysis for deadlocks and race conditions

### 6.1 Node Types

| Node Type      | Description                                                                                             |
| -------------- | ------------------------------------------------------------------------------------------------------- |
| `spawn_node`   | Represents a thread/task creation (`spawn`) instruction. Produces a `slot_id`.                          |
| `done_node`    | Represents task completion (`done`). Consumes the `slot_id` and updates the ready map.                  |
| `timeout_node` | Represents a timeout check. May enqueue the slot if RUNNING and expired.                                |
| `listen_node`  | Represents `listen()` consumption of ready slots. Consumes multiple `slot_id`s based on priority layer. |
| `!listen_node` | Represents Bare-metal polling of ready slots. Wakes via IRQ if no ready slots.                          |
| `atomic_node`  | Represents atomic operations (`CAS`, `fetch_or`) on memory locations.                                   |
| `context_node` | Represents context save/load operations for bare-metal tasks.                                           |
| `yield_node`   | Represents cooperative yield points in bare-metal scheduling.                                           |

### 6.2 Edges

* **Control edges**: Define execution order between instructions. For example, `spawn` → `done`.
* **Data edges**: Represent dependencies for payload, `slot_id`, or memory locations. For example, the `done` node depends on the `slot_id` produced by a `spawn_node`.

### 6.3 Example IR Graph

```text
spawn(t1) ──> worker_func ──> done(t1)
                  |
                  v
             ready_map enqueue ──> listen() consumer
                  |
                  v
            hierarchical timing wheel check ──> timeout_node
```

* `spawn_node` allocates a slot and produces `slot_id`.
* `worker_func` executes task logic.
* `done_node` sets the state to `DONE` and enqueues the slot.
* `listen_node` consumes ready slots in priority order.
* `timeout_node` only triggers if `DONE` has not been set (CAS-based).

### 6.4 Concurrency Semantics in IR Graph

* Multiple `spawn_nodes` can exist concurrently.
* `done_node` has **priority over `timeout_node`** on the same slot.
* Atomic operations (`atomic_node`) enforce safe memory ordering.
* Ready map enqueues are idempotent and lock-free.
* Graph guarantees deterministic consumption of slots by the scheduler.

### 6.5 Graph Properties

* DAG: The scheduler/task dependency graph in this section is acyclic. This restriction does not apply to function CFGs, which may contain valid cycles.
* Deterministic: Execution of consumer nodes follows priority order.
* Target-agnostic: Graph semantics do not depend on OS or Bare-metal backend.
* Scalable: Supports `max_slots` > 256 using word-adaptive pages and multi-layer ready maps.

---


## 7. Deterministic Guarantees & Race Resolution (OIR Level)

### 7.1 Öncelik ve State Semantiği

| Slot State | Description                    | Priority in Race    |
| ---------- | ------------------------------ | ------------------- |
| RUNNING    | Task is executing              | Base state          |
| DONE       | Task successfully finished     | **Highest**         |
| TIMEOUT    | Task exceeded its allowed time | **Lower than DONE** |
| EMPTY      | Slot free                      | N/A                 |

* **Kural:** Eğer DONE ve TIMEOUT aynı slot için yarışırsa, **DONE her zaman kazanır**.
* Timeout CAS yalnızca RUNNING state’inde başarılı olur; eğer DONE daha önce gerçekleştiyse CAS başarısız olur ve TIMEOUT etkisiz hale gelir.

---

### 7.2 Race Resolution — IR Semantics

#### 7.2.1 DONE vs TIMEOUT

```oir
atomic_load(slot.state) -> old
if(old == RUNNING){
    payload_write(payload)
    atomic_thread_fence(release)
    atomic_cas(slot.state, RUNNING, DONE) -> success
    if(success) enqueue_ready(slot_id)
}
```

```oir
atomic_load(slot.state) -> old
if(old != RUNNING) ignore
else atomic_cas(slot.state, RUNNING, TIMEOUT) -> success
if(success) enqueue_ready(slot_id)
```

* CAS mekanizması sayesinde **DONE > TIMEOUT garantisi** sağlanır.
* Eğer DONE gecikmeli olarak tetiklenirse bile, CAS TIMEOUT’un üzerine yazamaz.

---

#### 7.2.2 Ready Map Consumption

```oir
while(global_ready_mask != 0){
    layer = highest_priority_layer(global_ready_mask)
    for each page in layer:
        mask = atomic_exchange(page, 0)
        while(mask != 0){
            slot_bit = ctz(mask)
            slot_id = page*WORD_BITS + slot_bit
            consume(slot_id) -> run consumer logic
        }
}
```

* Atomic exchange ile sayfa bazlı bit temizleme yapılır.
* Priority layers her zaman scanless ve deterministic bir şekilde taranır.
* !listen() veya listen() farkı backend bağımlıdır:

  * OS → WaitOnAddress / futex
  * Bare-metal → WFI veya IRQ polling

---

### 7.3 Timer Wheel Race Semantiği

* Timer wheel, **O(1) bucket lookup** ile çalışır.
* Timeout tick ISR tetiklenir → yalnızca RUNNING slotlar için CAS uygulanır.
* DONE slotları zaten state değiştirmiştir → timeout etkisiz olur.
* Multi-level hierarchical wheel sayesinde timeout gecikmeleri amortized O(1) ile işlenir.

```oir
advance_timer(level)
bucket = current_tick % WHEEL_SIZE
expired_mask = atomic_exchange(bucket, 0)
while(expired_mask){
    slot_bit = ctz(expired_mask)
    slot_id = bucket_base + slot_bit
    timeout_check(slot_id)
}
```

---

### 7.4 Context Switch & Concurrency

* Bare-metal soft context switch:

  * TCB save/load atomic ve deterministik
  * Yield noktalarında veya IRQ ile tetiklenir
* OS backend → OS scheduler tarafından preemptive veya cooperative kontrol edilir

```oir
context_save(current_tcb)
context_load(next_tcb)
```

* En yüksek öncelikli slot her zaman seçilir
* Multi-layer priority map sayesinde **deterministic slot selection** sağlanır

---

### 7.5 Deterministic Guarantees Summary

1. **DONE her zaman TIMEOUT’a baskın** → slot-level determinism.
2. **Ready map tüketimi scanless ve layer-priority-aware** → deterministic consumption order.
3. **Timer wheel** → O(1) timeout insert & consume, stale bits harmless.
4. **Atomic CAS & memory fences** → race-free payload visibility.
5. **Bare-metal / OS backend agnostic** → determinism guaranteed at IR level.
6. **No dynamic memory allocation** → deterministic latency and bounded memory footprint.

---

### 7.6 IR-Level Race Resolution DAG

```text id="oir-race-dag"
spawn(t1) ──> worker_func
                  |
                  +──> done(t1) ──> enqueue_ready(slot_id)
                  |
                  +──> timeout_node(t1) [CAS fails if DONE]
```

* DONE ve timeout yarışları **atomic CAS ile çözülür**.
* Ready map tüketimi **priority layers** tarafından deterministik olarak gerçekleştirilir.
* Graph, **deadlock ve starvation risklerini ortadan kaldırır**.

---

---

# OCC Intermediate Representation (.oir) — Formal Syntax & Example

## 1. File Structure

```

header
targets
types
instructions
entry_points

````

- **header**: OIR version, target word size, alignment
- **targets**: OS / Bare-metal / architecture definitions
- **types**: slot_id, payload, mask, tick, layer_id, state
- **instructions**: sequence of OIR instructions
- **entry_points**: functions/tasks to execute

---

## 2. Header Example

```oir
OIR_VERSION 1.0
WORD_BITS 64
STACK_ALIGNMENT 16
MAX_SLOTS 512
PRIORITY_LAYERS 4
````

---

## 3. Targets Definition

```oir
target linux_x86_64 {
    arch "x86_64"
    abi "win64"
    max_threads 8
    endianness "little"
    backend "os"
}

target bare_metal_arm {
    arch "armv7m"
    abi "none"
    max_threads 16
    endianness "little"
    backend "bare-metal"
}
```

---

## 4. Types Definition

```oir
type slot_id u64
type state enum { EMPTY=0, RUNNING=1, DONE=2, TIMEOUT=3 }
type payload struct { ptr u64, size u32, type u8 }
type layer_id u8
type mask u64
type tick u32
type tcb struct { sp u64, entry_ptr u64, arg_ptr u64, state u8 }
```

---

## 5. Instructions Syntax

```
<opcode> <operands> [metadata]
```

* Opcodes: `spawn`, `done`, `timeout_check`, `listen`, `!listen`, `atomic_cas`, `atomic_fetch_or`, `advance_timer`, `context_save`, `context_load`, `yield`
* Operands: defined in Type section
* Metadata: optional, e.g., priority layer

---

## 6. Example IR Program — Thread Download Task

```oir
# Spawn a download task
spawn t1 func=download_task arg=addr_nexus timeout=5000 priority=3

# Worker function executes
worker_func t1 {
    # do work...
    write payload to nexus
    done t1 type=file_path payload=addr_nexus
}

# OS listener
listen layer_mask=0xF max_slots=10

# Bare-metal listener
!listen layer_mask=0xF
```

* `spawn` produces `slot_id` t1
* `worker_func` uses payload and marks DONE
* `listen` consumes ready slots deterministically
* `!listen` polls for ready slots in bare-metal mode

---

## 7. Atomic Operations

```oir
atomic_cas addr=slot.state old=RUNNING new=DONE
atomic_fetch_or addr=ready_pages[0] mask=0x01
```

* Ensures DONE overrides TIMEOUT
* Guarantees deterministic slot consumption

---

## 8. Timer Wheel Operations

```oir
advance_timer level=0
timeout_check slot_id=t1
```

* Hierarchical timing wheel O(1) insert/consume
* DONE slots skip timeout processing

---

## 9. Context Switch (Bare-metal)

```oir
context_save tcb_ptr=current_tcb
context_load tcb_ptr=next_tcb
yield
```

* Saves/restores TCB registers & stack pointer
* Yield allows cooperative preemption

---

## 10. IR Graph Representation (Optional DAG Metadata)

```oir
graph t1 {
    spawn -> worker_func -> done -> enqueue_ready -> listen
    worker_func -> timeout_check [CAS fails if DONE]
}
```

* Represents deterministic execution flow
* Shows DONE vs TIMEOUT race resolution
* Supports static analysis and formal verification

---

## 11. Deterministic Guarantees

1. DONE > TIMEOUT priority on same slot
2. Atomic CAS and memory fences guarantee race-free payload visibility
3. Priority layers and ready map ensure deterministic consumption
4. Timer wheel operations O(1)
5. No dynamic memory allocation → bounded latency
6. OS and Bare-metal backend-agnostic

---

## ✅ Summary

* `.oir` files define **target-agnostic, deterministic, thread-safe execution**
* Supports both **OS-dependent** and **Bare-metal** execution
* Fully compatible with **Nexus Slot Matrix runtime**
* Enables **formal verification, deterministic scheduling, and safe concurrency**

---

Bu format ile artık:

- OIR **syntax ve types** tanımlı  
- Atomic ve concurrency ops **formalize edilmiş**  
- DONE/TIMEOUT deterministik çözümü **IR seviyesinde güvence altında**  
- OS ve Bare-metal backend’leri **direct mapping** ile destekleniyor  

---


# Example: Multi-task Thread System (.oir)
# Target: Generic 64-bit / OS or Bare-metal agnostic
```
OIR_VERSION 1.0
WORD_BITS 64
STACK_ALIGNMENT 16
MAX_SLOTS 512
PRIORITY_LAYERS 4

# -----------------------
# Target Definitions
# -----------------------
target linux_x86_64 {
    arch "x86_64"
    abi "win64"
    backend "os"
}

target bare_metal_arm {
    arch "armv7m"
    abi "none"
    backend "bare-metal"
}
```


```
# -----------------------
# Type Definitions
# -----------------------
type slot_id u64
type state enum { EMPTY=0, RUNNING=1, DONE=2, TIMEOUT=3 }
type payload struct { ptr u64, size u32, type u8 }
type layer_id u8
type mask u64
type tick u32
type tcb struct { sp u64, entry_ptr u64, arg_ptr u64, state u8 }

# -----------------------
# Instructions
# -----------------------

# --- Allocate Nexus memory areas ---
spawn t1 func=download_task arg=addr_nexus1 timeout=5000 priority=3
spawn t2 func=sensor_reader arg=addr_nexus2 timeout=2000 priority=2

# --- Worker: download task ---
worker_func t1 {
    # Simulate download
    write payload to addr_nexus1
    done t1 type=file_path payload=addr_nexus1
}

# --- Worker: sensor reader ---
worker_func t2 {
    read sensor port=0xAF into payload
    done t2 type=raw payload=addr_nexus2
}

# --- OS listener (consume ready slots) ---
listen layer_mask=0xF max_slots=10

# --- Bare-metal listener ---
!listen layer_mask=0xF

# --- Timer wheel operations ---
advance_timer level=0
timeout_check slot_id=t1
timeout_check slot_id=t2

# --- Atomic operations for ready map updates ---
atomic_cas addr=slot[t1].state old=RUNNING new=DONE
atomic_cas addr=slot[t2].state old=RUNNING new=DONE
atomic_fetch_or addr=ready_pages[0] mask=0x03  # set bits for t1 & t2

# --- Context switch (bare-metal) ---
context_save tcb_ptr=current_tcb
context_load tcb_ptr=next_tcb
yield

# -----------------------
# IR Graph (DAG)
# -----------------------
graph tasks {
    spawn(t1) -> worker_func(t1) -> done(t1) -> enqueue_ready -> listen
    spawn(t2) -> worker_func(t2) -> done(t2) -> enqueue_ready -> listen
    worker_func(t1) -> timeout_check(t1) [CAS fails if DONE]
    worker_func(t2) -> timeout_check(t2) [CAS fails if DONE]
}
```

---

### ✅ Açıklamalar

1. `t1` → download task, priority 3
2. `t2` → sensor reader, priority 2
3. `done` her zaman `timeout` üzerinde baskın, CAS mekanizması ile çözülür
4. `listen` / `!listen` → ready map’i tarar, önceliğe göre slotları tüketir
5. Timer wheel O(1) ile çalışır, stale bits harmless
6. Atomic operations → race-free ve deterministic
7. DAG gösterimi → DONE/TIMEOUT yarışları ve slot tüketimi net bir şekilde ortaya konmuş

---

Bu IR artık **derlenebilir ve doğrudan runtime’a uygulanabilir** seviyede.

* OS’de `listen` + timer + CAS mekanizmaları çalışır
* Bare-metal’de `!listen` + WFI + soft context switch çalışır

---


## 12. Function CFG — Basic block ve cross-block JUMP semantiği

Bu bölüm, §6'daki scheduler/task dependency DAG modelinden ayrıdır. Fonksiyonların yürütme akışı **Control-Flow Graph (CFG)** ile gösterilir.

### 12.1 Basic block

- Basic block, tek girişli bir instruction dizisidir.
- Her basic block en fazla bir terminator ile biter; geçerli tamamlanmış OIR block'u tam olarak bir terminator içermelidir.
- Terminator örnekleri: `JUMP`, `BRANCH`, `RETURN` ve ayrı hata/çıkış sözleşmelerinde tanımlanan diğer terminal transferler.
- Bir label, OIR Builder tarafından bir basic block girişine bağlanır. Bir label'a atlama, hedef block'un girişine transferdir.
- Farklı label'lar gerektiğinde farklı basic block'lar başlatır. Kaynak satır sırası, CFG kenarlarının yönünü belirlemez.

### 12.2 JUMP terminator

Soyut biçim:

```oir
block source:
    JUMP target
```

Semantik:
1. `JUMP`, source block'un terminator'ıdır.
2. Kontrol, target basic block'un girişine aktarılır.
3. Source block'tan target dışında doğal fallthrough successor üretilmez.
4. Target aynı fonksiyonun geçerli block tablosunda bulunmalıdır.
5. Target'ın kaynak kodda source'tan önce veya sonra bulunması fark yaratmaz.
6. Target block farklı bir kontrol akışı bölgesinde olabilir; kaynak dilin fonksiyon ve lexical-scope güvenlik kuralları geçerlidir.
7. Target block parameter alıyorsa jump kenarı gerekli argümanları taşımalıdır. Argüman sayısı ve tipleri hedef block imzasıyla eşleşmelidir.

### 12.3 CFG kenar tutarlılığı

Her `JUMP source -> target` için:
- target, source'un successor kümesinde bulunmalıdır.
- source, target'ın predecessor kümesinde bulunmalıdır.
- Terminator ve edge argümanları aynı hedefi göstermelidir.
- CFG dönüşümünden sonra bu ilişkiler tekrar doğrulanmalıdır.

### 12.4 İleri/geri ve cross-block aktarım

- İleri jump: Kaynak metinde daha sonra tanımlanmış block'a transfer.
- Geri jump: Kaynak metinde daha önce tanımlanmış block'a transfer; CFG çevrimi oluşturabilir.
- Cross-block jump: Kaynak ve hedef ayrı basic block'lardır. Bu, `jump(label)` komutunun temel kullanım biçimidir.
- Jump threading, block merging ve unreachable block elimination gibi optimizer dönüşümleri label hedeflerini, edge argümanlarını ve SSA ilişkilerini korumalıdır.

**Önemli:** Fonksiyon CFG'si DAG olmak zorunda değildir. Scheduler/task dependency DAG kuralı fonksiyon CFG'sine uygulanmaz.



### 12.5 Çoklu predecessor ve edge-specific block parameter aktarımı

Bir basic block birden fazla predecessor tarafından hedeflenebilir. Bu durumda her giriş kenarı, hedef block'un ihtiyaç duyduğu block parameter değerlerini kendi üzerinden taşımalıdır.

Kavramsal örnek:

```oir
block left:
    JUMP join(value_left)

block right:
    JUMP join(value_right)

block join(value):
    USE value
```

Kurallar:
1. Her predecessor kenarı, hedef block parameter listesiyle uyumlu argüman sayısı ve tipleri taşımalıdır.
2. Hedefteki parameter, o çalıştırmada kontrolün geldiği kenarın argüman değerini alır.
3. Farklı predecessor'lardan gelen değerler, eşdeğer oldukları kanıtlanmadıkça tek bir değer olarak birleştirilemez.
4. Merge/phi eşlemesi gerçek CFG edge'lerine bağlıdır; yalnızca kaynak satır sırasına veya predecessor listesi sırasına güvenilemez.
5. CFG dönüşümleri predecessor/successor listelerini, edge argümanlarını, block parameter/SSA merge kayıtlarını ve ilgili metadata'yı birlikte güncellemelidir.
6. Bir block'un tek predecessor'ı olduğu varsayılamaz. Verifier ve optimizer çoklu girişli block'ları desteklemelidir.
7. Aynı source-target block çifti arasında birden fazla edge bulunmasına izin verilip verilmeyeceği ve edge kimliklerinin serileştirilmesi OIR format sürümünde açıkça tanımlanmalıdır.

Bu bölüm, MR-OIR-013 testinin temel OIR sözleşmesidir. Buradaki gösterim soyuttur; nihai opcode ve dosya formatı kararı değildir.



### 12.6 Karmaşık CFG bileşimi ve çoklu giriş ölçeklenebilirliği

Bir fonksiyon CFG'si; koşullu branch, çok seçenekli branch, loop, ileri/geri JUMP, ortak hedefte merge ve merge sonrasında yeniden dallanma yapılarının bileşiminden oluşabilir.

Kurallar:
1. `if / elsif / els`, `select`, `loop` ve `jump(label)` kaynak yapıları OIR'de aynı fonksiyon CFG'sinin block/edge yapıları olarak temsil edilir.
2. Her basic block tek bir terminator ile biter. Terminator'ın türü ve tanımladığı successor edge sayısı birbiriyle uyumlu olmalıdır.
3. Bir block için predecessor sayısına, fonksiyonun block/edge sayısına veya toplam jump sayısına keyfî sabit bir dil limiti konulmaz.
4. Her incoming edge kendi hedef block parameter argümanlarını taşır. Çoklu giriş merge semantiği MR-OIR-013'te tanımlanmıştır.
5. Merge sonrasında branch ve loop oluşturulması serbesttir; CFG'nin tek bir merge seviyesine veya yalnızca tek dallanma katmanına sahip olduğu varsayılamaz.
6. Builder, Verifier ve Optimizer değişken sayıda block/edge/predecessor desteklemelidir. İç veri yapıları güvenli büyüyebilmeli veya kapasite sınırını açıkça raporlamalıdır.
7. OIR formatındaki integer alanları, adres alanı ve uygulama kaynakları pratik sınır oluşturabilir. Bunlar açık format/uygulama limitleridir; kaynak dil için keyfî semantik limit olarak sunulamaz.
8. Kapasite veya kaynak sınırına ulaşıldığında işlem atomik biçimde başarısız olmalı; kısmi, sessizce kırpılmış veya tutarsız CFG dışarı verilmemelidir.
9. Her CFG dönüşümünden sonra terminator, edge, predecessor/successor, block parameter, SSA ve metadata ilişkileri birlikte güncellenip doğrulanmalıdır.

Bu sözleşme, OIR'nin yalnızca basit if/else grafikleri değil, birden fazla birleşme ve yeniden dallanma içeren karmaşık fonksiyon akışlarını da temsil edebilmesini zorunlu kılar.

Bu sözleşme, Multi-Rule formal specification içindeki **MR-OIR-014** karmaşık kontrol akışı ve çoklu giriş ölçeklenebilirlik testinin OIR tarafındaki karşılığıdır.



### 12.7 Nested loop context, SELECT merge ve jump ayrımı

Bir fonksiyon birden fazla iç içe loop içerebilir. Her loop'un header, body, latch/continue ve exit ilişkileri diğer loop'lardan ayrı tutulmalıdır.

Kurallar:
1. `break` en yakın kapsayan loop'un exit hedefine bağlanır.
2. `continue` en yakın kapsayan loop'un devam/latch akışına bağlanır. Loop biçiminin increment adımı varsa continue semantiği bu adımı korur.
3. `jump(label)` loop-context çözümlemesiyle değil, fonksiyon label tablosu ve lexical-scope güvenliğiyle çözülür; break/continue ile eşdeğer değildir.
4. `select` branch'leri normal CFG block'larıdır. Bir branch içindeki unconditional JUMP, o branch block'unu sonlandırır ve hedef block'a edge üretir.
5. İç içe loop'lar iç içe CFG döngüleri oluşturabilir; loop analizleri her loop'un kendi header/latch/exit ve backedge ilişkisini korumalıdır.
6. Bir select'in birden fazla branch'i aynı hedefe jump ediyorsa her giriş edge'i ve edge-specific block parameter argümanları korunmalıdır.
7. Optimizer loop unroll/rotation, jump threading veya block merge sırasında loop context, terminator, edge, predecessor/successor, SSA ve debug/source metadata ilişkilerini birlikte güncellemelidir.
8. Jump ile etkin loop lexical scope'larının dışına çıkılması halinde resource cleanup/implicit close davranışı ayrı kaynak dil sözleşmesiyle tanımlanmalıdır; OIR bu davranışı varsayımla üretmemelidir.

Bu kurallar MR-OIR-015'in OIR tarafındaki karşılığıdır. Loop-context alanlarının kesin binary encoding'i ve nihai OIR opcode isimleri henüz sabitlenmemiştir.


### 12.8 Jump scope-exit cleanup ve canlı scope aktivasyonu

Bu bölüm, kaynak dilde tanımlanan lexical-scope kaynak ömrünün OIR sözleşmesidir. Cleanup kararı yalnızca CFG'de hedef block'tan kaynak block'a bir yol bulunup bulunmadığına göre verilmez; lexical scope kimliği ve scope ancestry esas alınır.

Kurallar:

1. Her basic block, kaynakta bulunduğu lexical scope kimliğini veya eşdeğer scope ancestry bilgisini taşımalıdır. Bu bilginin OIR'de hangi kesin alan adı/encoding ile saklanacağı format kararıdır.
2. `JUMP` hedefi kaynak block'un scope'u ile aynı scope'ta veya kaynak scope'un bir ancestor'ında olmalıdır. Daha derin scope'a veya kardeş scope'a atlayarak scope giriş/başlatma adımlarını atlamak geçersizdir.
3. Kaynak ve hedef aynı lexical scope içindeyse jump yalnızca basic block değişimidir. Scope activation ve ona bağlı canlı kaynaklar korunur. Aynı scope içindeki ileri/geri jump döngü oluşturabilir; kaynakların ömrü scope sonuna kadar devam eder.
4. Hedef bir ancestor scope içindeyse kaynak ile hedef arasındaki terk edilen scope'lar kapanır. Bu scope'lara bağlı otomatik cleanup işlemleri, normal lexical-scope çıkışında olduğu gibi JUMP yoluna eklenmelidir.
5. Bir JUMP cleanup gerektiren scope'tan çıkıyorsa cleanup tam bir kez yürütülmelidir. Hedefe ulaştıktan sonra aynı eski scope activation'ına geri girilemez; daha derin scope'a doğrudan jump zaten geçersizdir.
6. İç içe birden fazla scope terk ediliyorsa cleanup, iç scope'tan dış scope'a doğru yürütülür. Aynı scope içindeki kaynakların cleanup sırası için kaynak edinim sırasına ters (LIFO) yaklaşımı temel kural olarak kullanılır; kaynak türüne özel istisnalar ayrıca tanımlanmalıdır.
7. `_>` ile sahipliği devredilmiş kaynak, kaynak scope'un cleanup listesinde artık sahip olunan kaynak olarak tutulmamalıdır. Sahiplik transferi kaynak temizliğinin iki kez yapılmasına neden olmamalıdır.
8. `}?;` blok kapsamındaki toplu kaynak/masa temizliğiyle ilişkilidir. Yalın `?;` Halt/sonlandırma davranışıdır; bir kaynak/masa/DLL hedefine uygulanmış biçimi hedefi serbest bırakır. Bu hedefli kaynak bırakma işlemi, OIR'de JUMP scope-exit cleanup yerine kullanılamaz.
9. Cleanup işlemlerinin kesin OIR opcode/record biçimi, cleanup hatasının raporlanması ve cleanup sırasında yeni kontrol aktarımına izin verilip verilmeyeceği bu testte kesin binary ABI kararı olarak sabitlenmez. Builder ve Verifier bu konuları tanımlanmamış davranışa bırakmamalı; desteklenmeyen cleanup akışını açık hata olarak raporlamalıdır.
10. Optimizer JUMP edge'lerini veya block'ları dönüştürürken scope ancestry, cleanup listeleri, sahiplik durumu ve kaynak/debug metadata ilişkilerini korumalıdır.

Bu sözleşme, CFG'de geri dönüş yolu bulunmasını tek başına cleanup'ı iptal eden bir ölçüt saymaz. Aynı canlı lexical scope'a geri dönülebiliyorsa kaynak korunur; scope gerçekten terk ediliyorsa cleanup yapılır.


### 12.9 Defer kayıtları ve scope-exit yürütme

OIR, her lexical scope aktivasyonu için defer kayıtlarını ve yürütülme sırasını temsil edebilmelidir. Kesin opcode ve binary alan adları henüz sabitlenmiş değildir.

1. Defer kaydı runtime control-flow noktasında etkinleşir; Builder kayıt noktasını ve ait olduğu lexical scope activation'ını korur.
2. Her scope activation kendi defer kayıt dizisine sahiptir. Farklı loop iterasyonları veya yeniden girilen scope activation'ları kayıtları paylaşamaz.
3. Scope içindeki normal block geçişi ve aynı-scope JUMP defer yürütme noktası değildir.
4. Scope'tan normal çıkış, BREAK, CONTINUE, RETURN veya ancestor-scope JUMP, terk edilen scope'un aktif defer kayıtlarını çalıştırır.
5. Birden çok kayıt LIFO sırasıyla yürütülür.
6. Defer gövdeleri, aynı scope activation'ının otomatik kaynak/masa cleanup adımlarından önce yürütülür.
7. İç içe scope'lar terk edilirken önce en içteki scope'un defer kayıtları LIFO sırasıyla, ardından o scope'un otomatik cleanup'ı yürütülür; sonra dış scope kapanışına geçilir.
8. Defer gövdeleri non-local terminator üretemez; nested defer bu sürümde reddedilir.
9. Optimizer defer kayıtlarının etkinleşme noktasını, LIFO sırasını, scope activation bağını ve cleanup sırasını korumalıdır.
10. Defer gövdesindeki hata/panic ve cleanup hatası politikası açık alt sözleşmedir; Builder sahte normal continuation üretemez.

Kavramsal scope-exit sırası:

```text
defer kayıtlarını LIFO sırayla yürüt
scope'a ait otomatik kaynak/masa cleanup işlemlerini yürüt
scope activation'ını kapat
hedef CFG block'una devam et (varsa)
```

Bu bir OIR opcode dizisi değil, semantik sözleşmedir.
