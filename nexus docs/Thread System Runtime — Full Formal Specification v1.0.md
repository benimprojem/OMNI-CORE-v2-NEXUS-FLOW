---

````markdown
# Thread System Runtime — Full Formal Specification v1.0

## 1. Introduction

This specification defines the **OCC Thread System Runtime**, supporting both **OS-dependent** and **Bare-metal** targets.  
The runtime provides:

- Lock-free, deterministic task scheduling
- Priority-aware execution
- Timeout management with hierarchical timing wheel
- Target-aware adaptive design (32/64-bit, page-based ready map)
- Minimal memory footprint and deterministic latency

---

## 2. Target System

### 2.1 Target Definition

Each target has a `.target` file under `targets/`:

```ini
arch="x86_64"
abi="win64"
stack_alignment=16
scalar_regs=16
vector_regs=16
endianness="little"
max_threads=8
simd_width=512
max_slots=512
word_bits=64
priority_levels=4
scheduler_policy="strict"
````

* `word_bits` is CPU word size (32 or 64)
* `priority_levels` defines hierarchical ready map layers
* Target compiler selects appropriate OS or Bare-metal backend
* User-defined targets are supported

### 2.2 Target-aware Conditional Compilation

```nxf
!!=[target="linux"] { ... }
!!=[target="win64"] { ... }
!!=[target="bare-metal"] { ... }
```

---

## 3. Nexus Slot Matrix

### 3.1 Slot Definition

```c
struct Nexus_Slot {
    void* thread_handle;   // HANDLE (Win) / TID (Linux) / Stack Pointer (Bare)
    u8 status;             // 0: EMPTY, 1: RUNNING, 2: DONE, 3: TIMEOUT
    u8 priority;           // 0–255
    u16 timeout_ms;        // 0 = infinite
    void* payload_ptr;     // 1024-byte memory area
    u8 pldstatus;          // 1: RAW, 2: pointer, 3: file path
    u8 _padding[11];       // 32-byte cache alignment
};
```

* Memory aligned 32 bytes for cache-line efficiency

---

## 4. Ready Map — Word-size Adaptive

### 4.1 Pages

* Word size = target CPU (`word_bits`)
* Each page = word (32/64-bit)
* Slots are mapped to pages:

```c
page = slot_id / WORD_BITS
bit  = slot_id % WORD_BITS
```

* Ready map supports multiple pages:

```c
atomic_word ready_pages[PAGE_COUNT];
```

* PAGE_COUNT = `max_slots / WORD_BITS`

### 4.2 Multi-layer Priority Map

```c
struct ReadyLayer {
    ready_word pages[PAGE_COUNT];
};

struct Scheduler {
    ReadyLayer layers[PRIORITY_LEVELS];
    atomic_word global_ready_mask;  // highest layer ready
}
```

* Layer selected from `slot.priority` via mapping function

---

## 5. DONE & Timeout Semantics

### 5.1 State Machine

```text
RUNNING → DONE      (highest priority)
RUNNING → TIMEOUT   (only if slot still RUNNING)
```

* DONE always overrides TIMEOUT
* TIMEOUT is harmless if DONE already called

### 5.2 DONE Logic

```c
old = atomic_load(slot.state);
if(old == RUNNING){
    write(payload)
    atomic_thread_fence(release)
    atomic_compare_exchange(&slot.state, RUNNING, DONE)
    enqueue_ready(slot_id)
}
```

### 5.3 Timeout Logic

```c
if(atomic_load(slot.state) != RUNNING) return;
if(atomic_compare_exchange(&slot.state, RUNNING, TIMEOUT)){
    enqueue_ready(slot_id)
}
```

* CAS guarantees DONE > TIMEOUT
* Timeout bucket stale bits are harmless

---

## 6. Hierarchical Timing Wheel

### 6.1 Wheel Structure

* Multi-level wheel for O(1) timeout
* Level 0: 1ms tick, 256 buckets
* Level 1: 256ms, Level 2: 65s (example)

```c
struct TimerWheel {
    u32 current_tick;
    u32 bucket_count;
    u64 buckets[WHEEL_SIZE];  // slots bitmask
}
```

### 6.2 Add Timeout

```c
expire_tick = current_tick + timeout_ms
bucket = expire_tick % WHEEL_SIZE
atomic_fetch_or(&buckets[bucket], (1ULL << bit))
```

### 6.3 Tick Advancement

```c
current_tick++
bucket = current_tick % WHEEL_SIZE
expired_mask = atomic_exchange(&buckets[bucket], 0)
while(expired_mask){
    bit = ctz(expired_mask)
    slot_id = ...
    if(slot.state == RUNNING) enqueue_ready(slot_id)
}
```

---

## 7. Scheduler Algorithm

### 7.1 Listener (OS Mode)

```c
while(global_ready_mask != 0){
    highest_layer = highest_set_bit(global_ready_mask)
    for each page in layer:
        mask = atomic_exchange(page, 0)
        while(mask != 0){
            bit = ctz(mask)
            slot_id = layer*WORD_BITS + bit
            consume(slot_id)
        }
    clear bit in global_ready_mask if layer empty
}
```

### 7.2 !listen() (Bare Metal)

```c
while(true){
    if(global_ready_mask == 0) asm("wfi")
    else pop highest priority slot and consume
}
```

---

## 8. Spawn & Task Creation

```nxf
(h) <- spawn(task(args), async, timeout)
```

* Allocates slot
* Sets state = RUNNING
* Increments active counter
* OS backend → CreateThread / pthread
* Bare-metal → push task to queue or prepare stack

---

## 9. Soft Context Switch (Bare Metal)

### 9.1 Task Control Block

```c
struct TCB {
    void* stack_ptr;
    void (*entry)(void*);
    void* arg;
    u8 state;
}
```

### 9.2 Context Switch

* PendSV (ARM) or equivalent
* Save registers
* Load next TCB stack
* Deterministic preemption optional

---

## 10. Memory Layout

```c
struct Runtime {
    Nexus_Slot slots[MAX_SLOTS];
    alignas(64) ready_word ready_pages[PAGE_COUNT];
    atomic_u8 trigger_flag;
    TimerWheel timer_wheel[LEVELS];
}
```

* Cache-aligned
* Bounded memory
* No heap
* Deterministic latency

---

## 11. Target-Aware Compilation

* Compiler generates backend-specific code
* OS vs Bare-metal resolved at compile-time
* Word size, page count, max slots, priority levels configured per target

---

## 12. Deterministic Guarantees

1. DONE always overrides TIMEOUT
2. Ready map delivers slots O(1)
3. Priority layers respected
4. Timeout wheel O(1)
5. Memory bounded, no dynamic allocation
6. Lock-free, race-free with atomic CAS
7. Bare-metal uses IRQ and WFI, OS uses wait-on-address / futex

---

## 13. Summary

This specification defines a **production-grade, minimal, deterministic thread runtime**:

* Multi-target: OS and Bare-metal
* Priority-aware, lock-free, bounded memory
* Timeout subsystem: Hierarchical timing wheel
* Soft context switch for Bare-metal
* DONE > TIMEOUT rule ensures correctness
* Scanless ready map with word-size adaptation
* Target-aware compilation and configuration

> Ready for implementation and formal verification.

