# OCC centralized dependency store (dsplibs)

This directory is the shared store for external Nexus Flow dependency packages managed by OCM.

- Dependencies are not copied into individual projects.
- OCM adds packages to this store and resolves packages from it.
- Project-specific version and dependency selection will be represented by OCM metadata.
- This directory is separate from the built-in language modules in `occ/libs/`.

The package metadata format and OCM command contract will be specified separately.
