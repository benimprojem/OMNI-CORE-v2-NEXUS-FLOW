# OCC / NEXUS Flow SDK Documentation

This directory is for OCC users and SDK integrators. It explains how to install, configure and use the toolchain rather than duplicating normative language/compiler specifications.

| Section | Contents |
|---|---|
| [Getting started](getting-started/README.md) | Installation and first-project guidance |
| [Commands](commands/) | OCC CLI usage and build workflow |
| [Configuration](configuration/) | Project layout and target setup |
| [API references](api/) | Core API, standard library, OS bridge and primordial core |
| [Examples](examples/README.md) | Practical SDK examples |
| [Errors](errors/README.md) | Error codes and diagnostic handling |
| [Warnings](warnings/README.md) | Warning codes and non-fatal diagnostics |
| [Limits](limits/README.md) | Supported limits and implementation constraints |
| [Troubleshooting](troubleshooting/README.md) | Setup/build diagnostics and recovery |

Normative language semantics, OIR rules, compiler architecture, ABI contracts and security rules are maintained in the repository's [engineering documentation](../../docs/README.md). This SDK guide links to those documents rather than keeping divergent copies.

A guide or example describes intended usage; it does not by itself prove that a compiler feature is implemented in the current executable. Check the project status before relying on a feature in production.
