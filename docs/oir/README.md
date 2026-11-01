# OCC Intermediate Representation

The OIR documentation distinguishes semantic IR rules from the legacy binary container currently used by the SDK.

- [OCC OIR Specification](occ-oir-specification.md) — runtime/task semantics and IR concepts.
- [OIR2 Binary Container Contract](oir-v2-binary-format.md) — exact 48-byte header, symbol records, fixed-width instruction records and structural validation of the current Writer/Reader.
- [Legacy OIR v2 and Target Specification](legacy-oir-v2-target-specification.md) — historical architecture notes; use the binary contract above for the current checked-in container layout.

The OIR2 binary container is not yet the complete serialized representation for newer semantic OIR features such as CFG/SSA metadata and defer-bearing OIR 1.1 artifacts. Do not treat those semantic contracts as implemented by the legacy container.

Numbered MR-OIR tests and control-flow semantics are tracked in the [Multi-Rule Engine specification](../language/multi-rule-engine.md).
