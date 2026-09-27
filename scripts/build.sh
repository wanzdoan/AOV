#!/bin/bash
# =============================================================================
#  AOV Zygisk — Linux/macOS Build Script
#  Builds the Zygisk .so and packages it into a flashable ZIP
# =============================================================================

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
JNI_DIR="$PROJECT_DIR/jni"
OUT_DIR="$PROJECT_DIR/build"
ZYGISK_DIR="$PROJECT_DIR/zygisk"

# ── Configuration ─────────────────────────────────────────────────────────────
# Set your NDK path:
NDK_PATH="${ANDROID_NDK:-$HOME/Android/Sdk/ndk/26.3.11579264}"
ABI="arm64-v8a"
MODULE_ID="aov_zygisk"
MODULE_VERSION="$(sed -n 's/^version=//p' "$PROJECT_DIR/module.prop" | head -n 1)"

echo "============================================"
echo "  AOV Zygisk Build Script"
echo "  NDK: $NDK_PATH"
echo "  ABI: $ABI"
echo "============================================"

# ── Validate NDK ─────────────────────────────────────────────────────────────
if [ ! -f "$NDK_PATH/build/cmake/android.toolchain.cmake" ]; then
    echo "[ERROR] NDK not found at: $NDK_PATH"
    echo "Set ANDROID_NDK env or update NDK_PATH in this script"
    exit 1
fi

# ── Validate Vendor Files ─────────────────────────────────────────────────────
check_vendor() {
    local path="$1"; local desc="$2"; local url="$3"
    if [ ! -e "$path" ]; then
        echo "[WARN] Missing: $desc"
        echo "       Get from: $url"
        echo "       Place at: $path"
    fi
}

check_vendor "$JNI_DIR/vendor/imgui/imgui.cpp" \
    "Dear ImGui sources" \
    "https://github.com/ocornut/imgui"

check_vendor "$JNI_DIR/vendor/dobby/lib/$ABI/libdobby.a" \
    "Dobby hook library" \
    "https://github.com/jmpews/Dobby/releases"

check_vendor "$JNI_DIR/vendor/zygisk/include/zygisk.hpp" \
    "Zygisk header" \
    "https://github.com/LSPosed/LSPosed"

# ── Build ─────────────────────────────────────────────────────────────────────
mkdir -p "$OUT_DIR/cmake_$ABI"

echo ""
echo "[1/3] Configuring CMake..."
cmake -S "$JNI_DIR" -B "$OUT_DIR/cmake_$ABI" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-21" \
    -DANDROID_STL="c++_static" \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja

echo ""
echo "[2/3] Building $MODULE_ID.so..."
cmake --build "$OUT_DIR/cmake_$ABI" --config Release -j$(nproc)

# ── Package ───────────────────────────────────────────────────────────────────
echo ""
echo "[3/3] Packaging..."

mkdir -p "$ZYGISK_DIR"
cp "$OUT_DIR/cmake_$ABI/lib$MODULE_ID.so" "$ZYGISK_DIR/$ABI.so"
echo "Copied: $ZYGISK_DIR/$ABI.so"

# Create flashable ZIP
ZIP_NAME="${MODULE_ID}_${MODULE_VERSION}.zip"
ZIP_PATH="$PROJECT_DIR/$ZIP_NAME"

rm -f "$ZIP_PATH"
cd "$PROJECT_DIR"
zip -r "$ZIP_PATH" \
    module.prop customize.sh post-mount.sh post-fs-data.sh service.sh \
    zygisk/ \
    webroot/ \
    META-INF/ \
    --exclude "*.git*" "build/*" "jni/*" "scripts/*" "*.zip"

echo ""
echo "============================================"
echo "  BUILD COMPLETE"
echo "  .so:  $ZYGISK_DIR/$ABI.so"
echo "  ZIP:  $ZIP_NAME"
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. adb push $ZIP_NAME /sdcard/"
echo "  2. Flash via KernelSU Manager"
echo "  3. Reboot → Launch AOV → tap [M] to open menu"
