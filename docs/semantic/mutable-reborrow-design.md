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

## 2026-11-23 implementation audit

The current C implementation was re-read before introducing parent/child state. The following existing mechanics are confirmed:

- `BorrowOrigin` contains only the storage-owner `Slot*`, projection path, and borrow kind. It does not identify the immediate reference binding from which a reborrow was created.
- `semantic_install_borrow_origin()` allocates the replacement origin before releasing the old binding, then updates owner counters. This is the atomic-rebind pattern that must be retained.
- `semantic_release_borrow_binding()` releases each origin by decrementing the storage owner's shared count or clearing its mutable flag. It has no parent-resume transition.
- `exit_scope()` releases all bindings at the exiting depth before destroying any slots. The existing depth rule prevents a borrower from outliving its storage owner, but it does not encode nested mutable-reference suspension.
- Branch snapshots clone only owner/path/kind. `semantic_reference_init_snapshot_restore()` rebuilds owner counters from those cloned origins; the merge unions origins by owner/path/kind. A parent/child relationship would therefore be lost unless snapshot capture, clone, union, restore, and destruction are extended together.
- Conflict checks enumerate active borrower origins against the storage owner. They cannot distinguish a child borrow from a direct borrow or resume an immediate parent safely.

### Implementation consequence

Do not add a raw parent pointer to `BorrowOrigin` as a partial patch. Snapshot clones and branch unions currently duplicate origin nodes, while slot destruction is scope-driven; a raw parent pointer alone would create ambiguous ownership and restoration behavior.

The implementation must first establish a stable borrower identity and a relationship representation whose lifetime is bounded by the semantic context. Then all origin lifecycle operations (install, release, snapshot clone/restore, branch merge, and scope exit) must preserve that identity and its parent link. Only after those operations are covered by tests may the two mutable reborrow rejection paths be enabled.

No runtime behavior is changed by this audit. Mutable dereference and mutable projection reborrows remain rejected, and SEM-005 remains IN_PROGRESS.

## 2026-11-23 — stable borrow identity foundation

The C model now has a first identity layer:

- Each reference-binding `Slot` receives a monotonic, SemanticContext-local `borrow_identity` when it first acquires a tracked borrow.
- Each `BorrowOrigin` records the identity of the binding that owns it and reserves `parent_identity` for the immediate parent borrow (zero for the existing direct-borrow paths).
- Snapshot clone and origin-union operations preserve both identity fields; union equality includes them.
- Existing shared-reference copy and projection-origin construction assign the destination borrow identity before installing the origin set.
- Identity exhaustion fails conservatively instead of wrapping and reusing an identity.

This is identity scaffolding only: no parent-child edges are created yet, no suspension/resumption behavior is enabled, and mutable reborrow syntax remains rejected. The next step is to associate a child origin with its immediate mutable parent and make release/snapshot/merge operate on that relationship atomically.


## 2026-11-23 — identity initialization and snapshot audit correction

A follow-up audit found three gaps in the identity foundation and corrected them:

- semantic_init() now explicitly resets next_borrow_identity to zero. The counter is not allowed to inherit uninitialized or reused stack state.
- Legacy scalar borrow bindings copied into flow snapshots now carry the Slot's borrow_identity and an explicit zero parent identity.
- Shared projection reborrow assigns/validates the destination identity before constructing any BorrowOrigin nodes. Previously the nodes could capture zero before the later identity allocation.

A focused C regression poisons the counter with UINT64_MAX, calls semantic_init(), and verifies that initialization resets it. This validates initialization only; it does not prove identity graph lifecycle or mutable parent suspension. The CTest/CI result for this new regression is still pending.

The regression set also verifies that two simultaneously tracked shared-reference bindings receive distinct nonzero identities and that each origin records the matching borrower identity. Test commit: 4d32fd6; CI has not yet been observed.


## 2026-11-23 — CI regression fix: mutable projection rejection

The C Compiler Build workflow exposed one repeated Semantic regression beginning at run 5301: the negative test for `&mut parent.x` expected one diagnostic but received none. The implementation only routed `TOKEN_BIT_AND` member/index projections through shared-projection handling; `TOKEN_BORROW_MUT` projections fell through to generic lvalue borrowing and were incorrectly accepted. The initializer binder now detects member/index projections rooted in a mutable safe-reference Slot and emits the existing fail-closed parent-child tracking diagnostic before the generic lvalue path. The existing `check_mutable_projection_reborrow_rejection()` test covers this case plus `&mut *parent`. Fix commit: `4d00ef2`. The new workflow run is being checked across all five jobs; do not infer full-matrix success from the Ubuntu job alone.


## 2026-11-23 — parent/child registration and scoped mutable reborrow

The first behavioral implementation is now present in C:

- A mutable child BorrowOrigin records the immediate parent binding identity as well as the ultimate storage owner and composed projection path.
- Creating a child sets parent.borrow_suspended_by_identity to the child's stable identity. The parent remains reserved and is not counted as a second independent mutable owner borrow.
- Identifier access to a suspended parent is rejected, including reads, writes, reassignment, move/relocation and call argument use. Member/index access rooted at a suspended parent is rejected separately because member traversal temporarily suppresses ordinary identifier borrow checks.
- Releasing a child clears only the matching parent's suspension. It does not release the ultimate owner's mutable reservation. Releasing a parent recursively releases any active descendants before releasing the root owner reservation.
- The same identity chain supports nested reborrows; release proceeds in reverse nesting order. The member/index access guard was added separately so member traversal cannot bypass suspension checks.
- &mut *parent and mutable member/index projections such as &mut parent.x are now routed through this parent-child registration path. The implementation accepts only one known tracked exclusive origin and fails closed for unknown or branched provenance.

Regression coverage now includes successful dereference and field-projection reborrows, writes through the child, parent use rejection while suspended, parent resumption after child scope exit, nested reverse-order resumption, and parent rebinding rejection.

Verification: branch-head workflow run https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/45090068185 passed all five jobs (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). Ubuntu CTest and ASan/UBSan each report 74/74 tests passed; Valgrind reports zero errors in all test processes.

This closes the first scoped registration/release slice, not SEM-005. Remaining gates include branch snapshot/restore/join graph consistency tests, allocation-failure atomicity for child registration, early exits and loop/control-flow cleanup, call/move/relocate and projection conflict matrices, and explicit dangling-origin checks. OIR remains gated.


## 2026-11-23 — branch snapshot and join consistency

Mutable origins are no longer merged by set union. Shared origins retain their existing union behavior, but if either branch snapshot contains a mutable origin, the mutable origin sets must be exactly equal by storage owner, borrower identity, immediate parent identity, borrow kind and projection path. A divergent mutable graph emits a Semantic diagnostic instead of constructing a union that exists on neither path. Parent suspension identities are independently required to match at the join.

The regression pair exercises both outcomes: identical mutable child relationships on both if/els paths survive snapshot restore and join; differing field paths for the same child binding are rejected. The existing snapshot clone/restore path carries parent identities and parent suspension identities.

Verification: workflow run [45090600776](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/45090600776) passed all five jobs. Ubuntu and ASan/UBSan each passed 74/74 CTest cases; Valgrind completed with zero reported errors.

This is direct if/els snapshot/join coverage. Full nested-graph branch combinations, select/scan joins, loop back-edge invariants, and allocation-failure behavior during child registration/restore remain open.


## 2026-11-23 — graph-safe snapshot restoration

A restore-order defect was found while exercising nested parent/child chains across if/els branches. The old loop released and restored each Slot in one pass; if a child Slot appeared before its parent in the list, restoring the parent later recursively released the already-restored child. Snapshot restoration now uses two passes: release the entire current graph first, then install all cloned origin sets and suspension identities. After installation, a graph validator checks both directions of every mutable edge: each suspended parent must have its matching live child, and each child must point to a parent that is still suspended by that child and retains the same exclusive storage owner.

The nested branch regression now exercises the full sequence: root → child → grandchild, both if/els paths create the same grandchild relation, the grandchild is written after the join, then scope exit resumes child and root in reverse order. This checks restore, validation and cleanup together.

Verification: workflow run [45090990756](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/45090990756) passed all five jobs. Ubuntu and ASan/UBSan each passed 74/74 CTest cases; Valgrind completed successfully.

Remaining snapshot coverage: select/scan joins, loop back-edge and early-termination cases, and allocation-failure atomicity during child registration and snapshot restoration.


## 2026-11-23 — select/scan joins and loop cleanup

The same strict mutable-graph merge is now exercised through the other control-flow constructs that use Semantic snapshots:

- `select` with a default arm and `scan` with an else arm preserve the same parent/child relation on all paths; the child remains usable after the join.
- A child created inside a nested loop-body block is released at the end of each iteration, allowing the parent to be used after the loop.
- A `break` after the nested child block also leaves the parent resumed and usable.
- A loop that rebinds an outer child reference to a mutable reborrow of its parent changes the relationship between the pre-loop and back-edge states; the Semantic join rejects that persistent graph change.

Verification: workflow run [45091574297](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/45091574297) passed all five jobs. Ubuntu and ASan/UBSan each passed 74/74 CTest cases; Valgrind reports zero errors.

Remaining SEM-005 gates are now concentrated on allocation-failure atomicity for child registration/snapshot restoration, broader early-return/termination paths, projection overlap/disjointness matrices, and additional lifecycle/dangling-origin checks. OIR remains gated.


## 2026-11-23 — allocation-failure atomicity

Added a dedicated Semantic OOM fixture that first binds a mutable child to a separate storage owner, then attempts to rebind that same child through `&mut *parent`. The legacy allocation-failure injector now sweeps every Semantic allocation site in this fixture. A focused state assertion checks the failures reported by mutable-child registration: the child still owns its previous origin, the parent is not marked suspended, and both original exclusive owner reservations remain active. This verifies that suffix/path/origin metadata is fully allocated before the previous child binding is released.

The mutable reborrow registration path now propagates `SEMANTIC_RESULT_OUT_OF_MEMORY` for its metadata allocation failures; direct borrow-origin installation also reports OOM instead of silently returning a normal semantic status. Identity exhaustion remains a distinct invalid-data failure.

Verification: workflow run [45092219887](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/45092219887) passed all five jobs. Ubuntu and ASan/UBSan each passed 74/74 CTest cases; Valgrind reports zero errors.

Snapshot-clone/restore allocation-failure atomicity remains untested and is still an open SEM-005 gate.
