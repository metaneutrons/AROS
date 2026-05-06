#!/bin/bash
# Build Mesa 26 for AROS AArch64
# Called by mmake or manually.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AROS_SRCDIR="${AROS_SRCDIR:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
MESA_SRC="$AROS_SRCDIR/external/mesa"
CROSSTOOLS="${CROSSTOOLS:-$AROS_SRCDIR/../build-test/bin/linux-x86_64/tools/crosstools}"
BUILD_DIR="${MESA_BUILDDIR:-$AROS_SRCDIR/../build-mesa26}"
LIB_DIR="$SCRIPT_DIR/lib"

if [ ! -f "$MESA_SRC/VERSION" ]; then
    echo "ERROR: Mesa source not found. Run: git submodule update --init external/mesa"
    exit 1
fi

if [ ! -f "$CROSSTOOLS/aarch64-aros-gcc" ]; then
    echo "ERROR: Cross-compiler not found at $CROSSTOOLS"
    exit 1
fi

echo "=== Building Mesa $(cat $MESA_SRC/VERSION) for AROS ==="

# Create GCC wrappers that filter -pthread
for tool in gcc g++; do
    WRAPPER="$CROSSTOOLS/aarch64-aros-${tool}-wrapper"
    if [ ! -f "$WRAPPER" ]; then
        cat > "$WRAPPER" << EOF2
#!/bin/bash
ARGS=()
for arg in "\$@"; do [ "\$arg" != "-pthread" ] && ARGS+=("\$arg"); done
exec "$CROSSTOOLS/aarch64-aros-$tool" "\${ARGS[@]}"
EOF2
        chmod +x "$WRAPPER"
    fi
done

# Generate cross-file with resolved paths
sed "s|@SRCDIR@|$AROS_SRCDIR|g; s|@CROSSTOOLS@|$CROSSTOOLS|g" \
    "$SCRIPT_DIR/aros-aarch64.cross" > "$BUILD_DIR/.cross-file"

# Configure if needed
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    mkdir -p "$BUILD_DIR"
    meson setup "$BUILD_DIR" "$MESA_SRC" \
        --cross-file "$BUILD_DIR/.cross-file" \
        --default-library=static \
        -Dgallium-drivers=v3d,softpipe \
        -Dvulkan-drivers= -Dplatforms= \
        -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dllvm=disabled \
        -Dshared-glapi=disabled -Dgles1=disabled -Dgles2=disabled \
        -Dopengl=true -Dvalgrind=disabled -Dlibunwind=disabled \
        -Dshader-cache=disabled -Dbuildtype=release \
        -Dbuild-tests=false
fi

# Build library targets
ninja -C "$BUILD_DIR" -j$(nproc) \
    src/util/libmesa_util.a \
    src/compiler/libcompiler.a \
    src/compiler/nir/libnir.a \
    src/compiler/glsl/libglsl.a \
    src/broadcom/compiler/libbroadcom_compiler.a \
    src/broadcom/cle/libbroadcom_cle.a \
    src/broadcom/qpu/libbroadcom_qpu.a \
    src/gallium/auxiliary/libgallium.a \
    src/gallium/drivers/v3d/libv3d.a \
    src/gallium/drivers/v3d/libv3d-v42.a \
    src/gallium/drivers/v3d/libv3d-v71.a \
    src/gallium/drivers/softpipe/libsoftpipe.a \
    src/mesa/glapi/shared-glapi/libglapi.a \
    subprojects/expat-2.5.0/libexpat.a \
    2>/dev/null || true

# Archive with AROS ar (host ar produces incompatible format)
AR="$CROSSTOOLS/aarch64-aros-ar"
mkdir -p "$LIB_DIR"

$AR rcs "$LIB_DIR/libv3d.a" $(find "$BUILD_DIR/src/gallium/drivers/v3d" -name '*.o')
$AR rcs "$LIB_DIR/libbroadcom.a" $(find "$BUILD_DIR/src/broadcom" -name '*.o' -not -path '*disasm*')
$AR rcs "$LIB_DIR/libcompiler.a" $(find "$BUILD_DIR/src/compiler" -name '*.o' -not -path '*standalone*' -not -path '*gtest*')
$AR rcs "$LIB_DIR/libgallium.a" $(find "$BUILD_DIR/src/gallium/auxiliary" -name '*.o')
$AR rcs "$LIB_DIR/libsoftpipe.a" $(find "$BUILD_DIR/src/gallium/drivers/softpipe" -name '*.o')
$AR rcs "$LIB_DIR/libmesa_util.a" $(find "$BUILD_DIR/src/util" -name '*.o')
$AR rcs "$LIB_DIR/libglapi.a" $(find "$BUILD_DIR/src/mesa" -name '*.o')
$AR rcs "$LIB_DIR/libexpat.a" $(find "$BUILD_DIR/subprojects/expat"* -name '*.o')

# Build posix_compat
"$CROSSTOOLS/aarch64-aros-gcc" -c -O2 \
    -I "$SCRIPT_DIR/include" \
    -include "$SCRIPT_DIR/include/aros_mesa_posix.h" \
    "$SCRIPT_DIR/posix_compat.c" -o "$BUILD_DIR/posix_compat.o"
$AR rcs "$LIB_DIR/libposix_compat.a" "$BUILD_DIR/posix_compat.o"

echo "=== Mesa 26 libraries built in $LIB_DIR ==="
ls -lh "$LIB_DIR"/*.a
