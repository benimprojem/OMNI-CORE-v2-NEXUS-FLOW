# Hardware Target Profile — Device Topology and Machine Binding

**Status:** v2 normalized model and initial bounded OS/CPU parser implemented; backend integration remains incomplete  
**Schema direction:** Target descriptor v2  
**Related:** [TARGET_MODEL.md](TARGET_MODEL.md), [TARGET_PIPELINE_CONTRACT.md](TARGET_PIPELINE_CONTRACT.md)

## 1. Purpose

A Target is a complete description of the intended execution machine, not merely an operating-system/ISA selection. It may describe a single computer assembled from a particular CPU, GPU, NPU, FAPU, memory hierarchy, interconnect fabric, storage and peripheral set.

The same source program may therefore produce materially different code for two Target profiles even when both machines use the same CPU instruction-set family.

A profile may be:
- **Family profile:** describes a compatible hardware family or capability baseline.
- **Exact-machine profile:** describes one intended physical machine/configuration. This is the specialization mode and may intentionally make the resulting program unusable on other machines.

## 2. Descriptor structure

The v2 descriptor is organized around stable device IDs and explicit topology:

- `target_info`: schema version, profile ID, profile kind, OS/runtime and output format.
- `devices`: one record per CPU, GPU, NPU, FAPU, DSP, FPGA, memory controller, storage controller or other execution device.
- `memory`: physical/logical memory resources, capacities, address spaces, banks, cache levels, coherency and ownership.
- `interconnects`: links between device IDs, including direction, protocol, width, bandwidth/latency constraints and addressability.
- `platform`: boot/runtime services, interrupts, timers, DMA, IOMMU, security and power/thermal constraints.
- `software_abi`: calling convention, object format, runtime ABI, system libraries and entry-point contract.
- `specialization`: compile-time constants and permitted assumptions derived from the machine profile.
- `machine_binding`: optional exact-machine identity requirements and runtime verification policy.

Device records use stable IDs (for example `cpu0`, `gpu0`, `npu0`, `fapu0`) rather than relying on array position. Links reference these IDs. A validator must reject dangling references, duplicate IDs, impossible resource ownership and contradictory topology.

## 3. Device descriptions

A device record should support, as applicable:

### CPU
- ISA family, microarchitecture/model/revision and supported extensions.
- Word size, endianness, register files/classes, vector widths and execution units.
- Core/cluster counts, SMT topology, cache hierarchy and coherence properties.
- Atomic operations, memory ordering, interrupt and privilege model.
- ABI, reserved registers, stack alignment and calling convention.

### GPU
- Architecture/generation and execution topology (compute units, groups, lanes/lines).
- Scalar/vector/matrix capabilities and supported data types/precision modes.
- Register and shared/local memory capacities.
- Cache hierarchy, VRAM type/capacity and memory bus.
- Command submission, queue counts, synchronization and host/device addressability.
- Supported kernels, ISA profile and driver/runtime interface.

### NPU / FAPU / DSP / FPGA
- Execution blocks, lanes, matrix/vector units and supported operations.
- Numeric formats, accumulator widths, saturation/rounding behavior.
- Local memory, scratchpad, DMA engines and queue topology.
- Scheduling, synchronization, firmware/bitstream identity and runtime interface.
- Reconfiguration constraints and whether the device is fixed or programmable.

### Memory and storage
- Device ID, type, capacity, address range/address space and access permissions.
- Bank count, page/line size, ECC, cacheability, persistence and coherency.
- Read/write bandwidth and latency values, marked as specified, measured or estimated.
- Ownership and access paths from each compute device.

### Interconnect
- Source and destination device IDs.
- Link type/protocol, directionality, width, clock and lane count.
- Declared bandwidth/latency bounds and whether values are theoretical, measured or estimated.
- Routing, arbitration, DMA and cache-coherence properties.

A descriptor must distinguish vendor-specified facts, user-entered design assumptions and measured system values. The compiler must not treat an estimate as a guaranteed hardware fact.

## 4. Code-generation behavior

The compiler uses the profile at multiple levels:

1. **Semantic capability validation:** reject operations or numeric formats the selected devices cannot implement.
2. **Generic lowering:** express operations independently of physical registers where possible.
3. **Device-specific lowering:** map eligible work to CPU, GPU, NPU, FAPU or DSP based on explicit placement rules and available capabilities.
4. **Memory placement:** choose memory spaces and transfers using declared addressability, capacity, bandwidth and coherency.
5. **Scheduling and synchronization:** use declared queues, execution groups, DMA and barriers.
6. **Backend emission:** emit instructions, command streams, firmware interfaces or device-specific binaries for each selected device.
7. **Link/package:** produce the required multi-device package and runtime manifest.

A Target profile may define explicit placement constraints (for example, a function or operation must execute on `npu0`). The compiler must diagnose an unavailable or incompatible device rather than silently moving work to a different processor unless the source/build policy explicitly permits fallback.

The profile is not permission to invent hardware behavior. A backend may emit only operations supported by its implementation and validated against the descriptor.

## 5. Exact-machine specialization and runtime binding

An exact-machine profile can intentionally specialize output to one physical machine. This requires two separate mechanisms:

### Build-time specialization
The compiler may use fixed device topology, memory sizes, supported operations, address maps, firmware versions and other declared properties as compile-time assumptions. These values are recorded in the build provenance and runtime manifest.

### Runtime identity verification
If the binary must not run on another machine, the generated package must include a machine-binding policy. At startup, a trusted runtime component verifies the machine against that policy before enabling protected application code.

Prefer cryptographic identity over fragile identifiers such as CPU model strings or MAC addresses. A practical design may use a TPM/secure-element-backed device key, a signed machine enrollment record and a vendor/application signature. The private key must remain in hardware or a trusted service; it must never be embedded in the application binary.

The runtime should verify:
- The enrolled machine identity or public-key fingerprint.
- Required device IDs/models/revisions and firmware/driver compatibility.
- Required CPU/GPU/NPU/FAPU capabilities.
- Required memory topology and minimum capacities.
- The integrity/signature of the Target manifest and executable package.

If verification fails, the runtime refuses to activate the protected application. Offline verification, key rotation, recovery and hardware replacement need an explicit policy.

**Security boundary:** a Target descriptor by itself cannot prevent copying. Machine exclusivity requires a trusted runtime enforcement path and cryptographic enrollment. Software-only checks can be patched or spoofed and must not be described as unbreakable protection.

## 6. Example (illustrative only)

The following shows the intended shape, not a currently accepted parser format:

```toml
[target_info]
schema_version = 2
profile_id = "NEXSUS_WORKSTATION_A"
profile_kind = "exact_machine"
os = "custom_runtime"
package_format = "nexus_bundle"

[[devices]]
id = "cpu0"
kind = "cpu"
architecture = "nexsus_cpu_v1"
cores = 32
isa_profile = "nexsus-flow-v1"

[[devices]]
id = "gpu0"
kind = "gpu"
architecture = "nexsus_gpu_v1"
line_count = 512
group_width = 8
vram_bytes = 25769803776

[[devices]]
id = "npu0"
kind = "npu"
architecture = "nexsus_npu_v1"
numeric_formats = ["int8", "fp16", "fp32"]

[[devices]]
id = "fapu0"
kind = "fapu"
architecture = "nexsus_fapu_v1"
dds_channels = 64

[[memory]]
id = "system_ram"
kind = "ram"
capacity_bytes = 68719476736
accessible_by = ["cpu0", "gpu0", "npu0", "fapu0"]

[[interconnects]]
id = "fabric_cpu_gpu"
from = "cpu0"
to = "gpu0"
protocol = "nexsus_fabric"
bidirectional = true

[specialization]
require_exact_topology = true

[machine_binding]
mode = "secure_element_key"
manifest_signature_required = true
runtime_verification = "required"
```

The numeric values above are illustrative and do not define actual NEXSUS hardware specifications.

## 7. Parser and migration requirements

The current C parser accepts a narrow version-1 descriptor with fixed CPU-oriented fields. It does not yet model heterogeneous device arrays, topology links, GPU/NPU/FAPU capabilities, nested memory resources or machine binding.

Do not extend the existing fixed `TargetConfig` by adding a handful of GPU fields. The v2 parser should produce a bounded normalized profile with device/resource/link tables, explicit counts and validated references. The initial implementation scope is deliberately narrower: parse the OS/BareMetal platform fields and a complete CPU capability record first. GPU, NPU, FAPU, DSP, FPGA and other device records remain representable in the v2 model but are not required to be operational in this first parser milestone. Use stable IDs, bounded strings and explicit maximum counts. Keep the v1 reader available during migration; reject unknown required v2 fields and unsupported schema versions.

The normalized profile should be serialized into a dedicated versioned Target OIR artifact passed as a separate stage input. It must not be mixed into the target-neutral `OIR_MODULE` program representation.

## 8. Initial v2 text format and parser

The first parser is intentionally a constrained INI-style format, not a general TOML implementation. It currently accepts exactly one `[target_info]` section and one `[cpu]` section. Unknown sections/keys, duplicate keys, overlong lines, overlong compound keys, invalid scalar values, empty or duplicate CSV tokens, unsupported enum tokens and missing required fields are rejected. Both `schema_major` and optional `schema_minor` are parsed and checked. This initial parser explicitly rejects `profile_kind = "exact_machine"`; exact-machine loading remains unavailable until machine-binding fields are part of the text format. The parser fills a zero-initialized `OccTargetProfile` and runs the structural validator before returning success.

- Parser API: `occ_target_profile_load()` in `omnicore-sdk/src/c/target/target_profile_parser.h`.
- CPU extension, atomic-width, floating-point-format and feature lists are comma-separated unquoted tokens.
- Initial profile files live under `omnicore-sdk/targets/v2/` with the `.target2` suffix.
- The v1 `.target` parser remains unchanged and available during migration.
- This initial format has no repeated device/memory/interconnect tables yet. Those are deliberately deferred until the CPU + platform path is established.

The four initial v2 descriptors are `BareMetal64`, `Linux64`, `Win64` and `macOS_AArch64`. Their `core_count = 0` and generic vendor/model labels mean “unspecified family baseline”, not a claim about a particular physical CPU. ISA profiles are capability declarations for the target family and must be matched to the actual machine or explicitly overridden; they are not automatic host detection.

## 9. Initial implementation scope

The first usable Target v2 milestone is **OS + BareMetal + CPU capability resolution**. The initial platform set is Windows x86-64, Linux x86-64, macOS AArch64 and BareMetal x86-64 (with the existing RV32 descriptor retained as a future backend target, not a promised codegen target).

The CPU record must describe at least:
- ISA architecture/profile and explicit extension set.
- Word width, address width and endianness.
- General, vector and predicate register-file counts; vector width where applicable.
- Atomic operation widths and floating-point formats.
- Core/topology counts and relevant execution capabilities.
- CPU feature flags such as atomics, hardware floating point, vector support and privilege levels.

OS/BareMetal platform records additionally define `os`, `abi`, `binary_format`, `entry_point`, `stack_alignment_bytes` and optional `page_size_bytes`, alongside runtime services. `os = "none"` represents the OS-less BareMetal path; it is not an operating-system name. BareMetal must not be treated as an OS: it is an OS-less platform/runtime profile with its own boot and entry contract.

Other device kinds stay in the schema from the beginning so future programmable chips (especially FPGA) do not require a schema redesign. Their parser, backend and code-generation support are deferred.

## 10. Implementation progress

- [x] Initial normalized v2 C data model added under `omnicore-sdk/src/c/target/target_profile.h`.
- [x] Structural validator added in `target_profile.c` for schema version, bounded table counts, device/resource identity uniqueness, memory ownership references, interconnect endpoints, CPU capability requirements and platform ABI/alignment fields.
- [x] The profile model is integrated as the `occ_target_profile` CMake static library.
- [x] Bounded initial v2 text parser for one platform + one CPU record, with strict keys, duplicate-key/section/list-token rejection, bounded lines, schema major/minor checks, explicit exact-machine rejection and validator handoff.
- [x] Focused CTest regression target covering all four initial family descriptors (Win64, Linux64, macOS AArch64 and BareMetal64), explicit schema minor, duplicate/empty list tokens, repeated sections, exact-machine rejection, unsupported schema minor, numeric overflow, malformed quoted strings and unsupported CPU kinds.
- [x] Initial OS/BareMetal v2 descriptor files for Windows, Linux, macOS AArch64 and BareMetal.
- [ ] Canonical serializer and repeated device/memory/interconnect tables.
- [ ] Target OIR artifact writer/reader and module handoff.
- [ ] Backend/device capability registry, heterogeneous lowering and runtime machine binding.

The C data model and initial parser are an implementation foundation only. The parser supports family profiles with one CPU/platform record; it does not yet parse heterogeneous device, memory or interconnect tables, and it cannot load exact-machine profiles. The validator does not establish backend support or physical machine identity. CTest coverage is registered for all four initial family descriptors and malformed-input cases, but the tests have not yet been built or executed.

## 11. Implementation order

1. Extend the parser to repeated device/memory/interconnect tables and add canonical serialization.
2. Add Target OIR artifact writer/reader and pass it through the module ABI.
3. Resolve v2 profiles into the compiler pipeline while preserving v1 compatibility.
4. Add CPU backend capability checks and platform-specific code-generation gates.
6. Extend parsing/backend support to GPU, NPU, FAPU, DSP and programmable devices such as FPGA.
7. Add heterogeneous lowering, multi-device packaging and signed machine-binding runtime.

The initial v2 parser exists, but it only handles one CPU and platform record. Repeated heterogeneous device tables, Target OIR handoff, multi-device backend and machine-lock runtime are not yet implemented.
