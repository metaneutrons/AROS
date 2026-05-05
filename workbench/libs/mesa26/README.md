# Mesa 26 for AROS AArch64

## Overview

Mesa 26.0.6 is integrated as a Git submodule at `external/mesa/`.
It is built separately using Meson with an AROS cross-compilation file,
producing static libraries that AROS drivers link against.

## Building

```bash
# First time: initialize submodule
git submodule update --init external/mesa

# Build Mesa (requires meson, ninja, aarch64-aros-gcc in PATH)
cd workbench/libs/mesa26
bash build.sh
```

## Architecture

```
external/mesa/                  ← Git submodule (mesa-26.0.6 tag)
workbench/libs/mesa26/
├── aros-aarch64.cross          ← Meson cross-compilation file
├── posix_compat.c              ← POSIX/pthread/mmap/DRM stubs
├── build.sh                    ← Build script (meson + ninja)
├── mesa26.cfg                  ← AROS mmake integration config
├── README.md                   ← This file
└── lib/                        ← Output: static .a libraries
    ├── libv3d.a
    ├── libbroadcom.a
    ├── libcompiler.a
    ├── libgallium.a
    ├── libmesa_util.a
    ├── libsoftpipe.a
    └── libaros_mesa_compat.a
```

## Why not build Mesa inside AROS?

The old approach (Mesa 20.0.8 with mmake) required:
- A large AROS-specific patch (mesa-20.0.8-aros.diff)
- Manually maintained file lists in mmakefiles
- Tight coupling to a specific Mesa version

The new approach:
- Mesa source is unmodified (submodule pointer to upstream tag)
- Built with Mesa's own build system (Meson) via cross-file
- POSIX stubs are separate, not patched into Mesa
- Upgrading = change submodule tag + rebuild

## Supported Gallium drivers

- **v3d** — VideoCore VI (RPi4, V3D 4.2) and VideoCore VII (RPi5, V3D 7.1)
- **softpipe** — Software fallback renderer
