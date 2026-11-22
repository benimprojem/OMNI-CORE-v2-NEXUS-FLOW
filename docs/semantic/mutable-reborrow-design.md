# Mutable Reborrow State Model (SEM-005)

Status: **Design decision; implementation pending**

This document defines the minimum state model needed before enabling mutable reborrow (`&mut *parent`) in Semantic. It does not enable the syntax or change the current fail-closed rejection.

## Existing model and why it is insufficient

`Slot` stores owner borrow counters and a borrower's `BorrowOrigin` list. Each origin currently records the storage owner, a projection path, and shared/exclusive kind. The relationship is owner-rooted; it does not represent a mutable reference temporarily suspending another mutable reference.

The current release and flow-snapshot restore paths independently toggle owner exclusive flags. Relaxing the mutable-origin rejection under that model can incorrectly reactivate a parent while a child is live, or leave it suspended after child cleanup/branch restoration.

## Required invariants

1. **Exclusive chain:** each active mutable child has exactly one immediate mutable parent (or the storage owner when it is the first mutable reference).
2. **Immediate suspension:** creating a child suspends only its immediate parent. Suspended references cannot be read, written, reassigned, moved/relocated, passed to calls, or used to create another borrow.
3. **Owner reservation:** while a mutable chain exists, the storage owner remains exclusively reserved. Suspension does not release the owner-level exclusive borrow.
4. **Nested resume order:** child release unlinks the child and resumes its immediate parent only if the parent is alive and has no other live child. Nested chains resume in reverse order.
5. **Scope cleanup:** releasing a child due to reassignment or scope exit follows the same transition as ordinary release. Cleanup order must not depend on Slot declaration order.
6. **No dangling parent:** a child cannot outlive its parent. Parent destruction with a live child must be rejected or invalidated through a defined checked cleanup path; it must never leave a dangling Slot pointer.
7. **Atomic rebinding:** allocate and validate the replacement relationship before releasing the target’s current relationship. Failed allocation must leave the original binding and suspension graph unchanged.
8. **Branch snapshots:** snapshots capture the complete parent-child suspension graph, not just owner/path/kind. Restore reconstructs graph state and owner reservation counts from canonical relationship state, avoiding double activation/deactivation.
9. **Branch joins:** incoming branches must agree on compatible active parent-child chains. Conflicting or ambiguous relationships are conservatively rejected; no branch may duplicate exclusive access.
10. **Provenance:** unknown/external origins cannot participate until their lifetime and parent identity are represented explicitly.

## Model shape

Implement an explicit borrow relationship token with stable identity, not an unvalidated raw parent `Slot*` alone. The token records borrower identity, immediate-parent identity (when present), storage-owner identity, projection path, exclusive/shared kind, active/suspended state, and child count. Slot destruction unregisters relationships before Slot memory is freed. IDs should be scoped to the SemanticContext and checked for overflow.

The existing `BorrowOrigin` owner/path set remains useful for multi-origin shared borrows and projection overlap analysis, but it must not be the sole representation for mutable parent suspension.

A central transition API should own the state changes:

- `borrow_create`: validate parent active, type/path and exclusivity; reserve owner; link child; suspend parent.
- `borrow_release`: unlink child; resume parent when its child count reaches zero; release owner reservation only when the root relationship ends.
- `borrow_snapshot_capture`: capture canonical relationship IDs and states.
- `borrow_snapshot_restore`: validate and rebuild all links and owner counts atomically from captured state.
- `borrow_join`: compare incoming branch graphs and retain only relationships valid on every path; reject incompatible exclusive chains.

## Implementation sequence

1. Add relationship/token structs and lifecycle helpers with no language behavior change; test allocation failures and destruction.
2. Integrate mutable reference binding/reassignment with relationship registration and release. Keep `&mut *parent` rejected.
3. Integrate scope cleanup and verify nested cleanup independent of declaration order.
4. Integrate branch snapshot/restore and merge; test loops and if/else joins.
5. Enable syntax in narrow supported cases, with explicit positive and negative tests.
6. Expand to projections, parameter/external origins and escapes only after the core model passes.

## Minimum behavioral tests before enabling

- Child mutates referent; parent use while child is active is rejected.
- Parent resumes after child lexical scope ends.
- Nested child suspension and reverse resume.
- Child released by reassignment resumes parent exactly once.
- Early exit / return / loop scope cleanup.
- Branches with equivalent chains merge; incompatible chains reject.
- Overlapping child projections conflict; disjoint projections are allowed only when path analysis proves disjointness.
- Allocation-failure injection leaves the preexisting chain unchanged.
- Parent/child scope ordering cannot cause dangling relationship references.

Until all these tests and CI pass, mutable reborrow support is **not implemented**.
