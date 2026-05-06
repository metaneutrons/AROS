#!/bin/bash
#
# Build Mesa 26 for AROS AArch64
#
# Prerequisites:
#   - aarch64-aros-gcc in PATH (from AROS crosstools build)
#   - meson and ninja installed on host
#   - external/mesa submodule checked out
#
# Output:
#   workbench/libs/mesa26/lib/*.a — static libraries for linking
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AROS_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MESA_SRC="$AROS_ROOT/external/mesa"
BUILD_DIR="$AROS_ROOT/build-mesa26"
INSTALL_DIR="$SCRIPT_DIR/lib"

if [ ! -f "$MESA_SRC/VERSION" ]; then
    echo "ERROR: Mesa source not found at $MESA_SRC"
    echo "Run: git submodule update --init external/mesa"
    exit 1
fi

echo "=== Building Mesa $(cat $MESA_SRC/VERSION) for AROS AArch64 ==="

# Build the POSIX compat library first
echo "--- Building libaros_mesa_compat.a ---"
mkdir -p "$INSTALL_DIR"
aarch64-aros-gcc -c -O2 "$SCRIPT_DIR/posix_compat.c" -o "$BUILD_DIR/posix_compat.o" 2>/dev/null || \
    aarch64-aros-gcc -c -O2 "$SCRIPT_DIR/posix_compat.c" -o "/tmp/posix_compat.o"
aarch64-aros-ar rcs "$INSTALL_DIR/libaros_mesa_compat.a" "/tmp/posix_compat.o" 2>/dev/null || true

# Configure Mesa with Meson
echo "--- Configuring Mesa ---"
# Substitute @SRCDIR@ in cross file
sed "s|@SRCDIR@|$AROS_ROOT|g" "$SCRIPT_DIR/aros-aarch64.cross" > "/tmp/aros-mesa.cross"

mkdir -p "$BUILD_DIR"
meson setup "$BUILD_DIR" "$MESA_SRC" \
    --cross-file "/tmp/aros-mesa.cross" \
    --default-library=static \
    -Dprefix="$INSTALL_DIR" \
    -Dgallium-drivers=v3d,softpipe \
    -Dvulkan-drivers= \
    -Dplatforms= \
    -Dglx=disabled \
    -Degl=disabled \
    -Dgbm=disabled \
    -Dllvm=disabled \
    -Dshared-glapi=disabled \
    -Dgles1=disabled \
    -Dgles2=disabled \
    -Dopengl=true \
    -Dvalgrind=disabled \
    -Dlibunwind=disabled \
    -Dbuildtype=release \
    -Db_lto=false

# Build
echo "--- Building Mesa (this takes a while) ---"
ninja -C "$BUILD_DIR" -j$(nproc)

# Extract static libraries
echo "--- Extracting libraries ---"
find "$BUILD_DIR" -name '*.a' -exec cp {} "$INSTALL_DIR/" \;

echo "=== Done ==="
echo "Libraries in: $INSTALL_DIR/"
ls -la "$INSTALL_DIR/"*.a 2>/dev/null
