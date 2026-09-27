#!/bin/bash
# =============================================================================
#  AOV Zygisk — Offset Finder Script (diag + pull file phục vụ RE)
#  Thực tế đo được trên máy (Unity 2022.3.5f1, build 174743164):
#    - libmain.so trong APK chỉ 38KB (stub!) — code game thật nằm ở
#      libResources.so (56MB) và/hoặc file tải thêm trong
#      /data/user/0/<pkg>/files/Resources/<versionCode>/arm64-v8a/
#    - KHÔNG có libil2cpp.so trong APK → Il2CppDumper chưa chắc dùng được.
#      Game có libxlua.so (Lua scripting) — logic có thể nằm ở Lua.
#    - libunity.so (22MB) nằm ở Resources dir trên (không nằm trong APK).
#
#  Usage:
#    bash scripts/find_offsets.sh
#
#  Prerequisites:
#    - ADB connected, authorized
#    - AOV installed (com.garena.game.kgvn)
#    - Root (adb shell su -c ...) để đọc /data/*
# =============================================================================

PACKAGE="com.garena.game.kgvn"
OUT="./build/gamefiles"

echo "=== AOV Offset Finder ==="
echo "Target: $PACKAGE"
mkdir -p "$OUT"
echo ""

# ── Step 1: version + ABI ────────────────────────────────────────────────────
echo "[1] Package info:"
adb shell dumpsys package $PACKAGE 2>/dev/null | grep -E "versionName|versionCode|primaryCpuAbi"
echo ""

# ── Step 2: native libs trong APK ────────────────────────────────────────────
echo "[2] Native libraries shipped in APK:"
APK_LIB_DIR=$(adb shell su -c "ls -d /data/app/*/$PACKAGE*/lib/arm64/" 2>/dev/null | tr -d '\r' | head -1)
echo "    dir: $APK_LIB_DIR"
adb shell su -c "ls -l $APK_LIB_DIR 2>/dev/null" | awk '{print "    " $5 "  " $NF}'
echo ""

# ── Step 3: libs game tải thêm (Resources) ───────────────────────────────────
echo "[3] Downloaded game libs (Unity + resources):"
RES_DIR=$(adb shell su -c "ls -d /data/user/0/$PACKAGE/files/Resources/*/arm64-v8a/ 2>/dev/null" | tr -d '\r' | head -1)
echo "    dir: $RES_DIR"
adb shell su -c "ls -l $RES_DIR 2>/dev/null" | awk '{print "    " $5 "  " $NF}'
echo ""

# ── Step 4: pull file phục vụ RE offline ─────────────────────────────────────
echo "[4] Pulling key files to $OUT/ (can SUA doi vai phut)..."
adb shell su -c "cp ${APK_LIB_DIR}libmain.so /sdcard/aov_libmain.so 2>/dev/null && echo ok" | grep -q ok && \
    adb pull /sdcard/aov_libmain.so "$OUT/libmain.so" >/dev/null 2>&1 && \
    echo "    libmain.so OK" || echo "    libmain.so FAIL"
adb shell su -c "cp ${APK_LIB_DIR}libResources.so /sdcard/aov_libres.so 2>/dev/null && echo ok" | grep -q ok && \
    adb pull /sdcard/aov_libres.so "$OUT/libResources.so" >/dev/null 2>&1 && \
    echo "    libResources.so OK" || echo "    libResources.so FAIL"
adb shell su -c "cp ${RES_DIR}libunity.so /sdcard/aov_libunity.so 2>/dev/null && echo ok" | grep -q ok && \
    adb pull /sdcard/aov_libunity.so "$OUT/libunity.so" >/dev/null 2>&1 && \
    echo "    libunity.so OK" || echo "    libunity.so FAIL"
adb shell "rm -f /sdcard/aov_libmain.so /sdcard/aov_libres.so /sdcard/aov_libunity.so" >/dev/null 2>&1
APK_PATH=$(adb shell pm path $PACKAGE 2>/dev/null | grep base | cut -d: -f2 | tr -d '\r' | head -1)
adb pull "$APK_PATH" "$OUT/base.apk" >/dev/null 2>&1 && \
    echo "    base.apk OK" || echo "    base.apk FAIL"
echo ""

# ── Step 5: maps lúc game chạy ───────────────────────────────────────────────
echo "[5] Live process maps (can game chay truoc):"
PID=$(adb shell pidof $PACKAGE 2>/dev/null | tr -d '\r' | awk '{print $1}')
if [ -n "$PID" ]; then
    echo "    PID: $PID"
    adb shell su -c "cat /proc/$PID/maps" > "$OUT/aov_maps.txt" 2>/dev/null && \
        echo "    maps saved: $OUT/aov_maps.txt" || \
        echo "    [FAIL] need root"
    echo "    Key libs:"
    grep -E "libmain|libResources|libunity|libgpdeku|memfd" "$OUT/aov_maps.txt" 2>/dev/null | \
        awk '{print "    " $1 " " $NF}' | sort -u | head -20
else
    echo "    Game NOT running — mo game rồi chay lai script."
fi
echo ""

# ── Instructions ─────────────────────────────────────────────────────────────
cat <<'EOF'
=== Next Steps (dien offsets vao jni/include/offsets.hpp) ===

1. Xac dinh game code o dau:
   - Mo libResources.so (56MB) bang IDA/Ghidra, tim JNI_OnLoad / constructors.
   - Kiem tra base.apk: assets/bin/Data/Managed/Metadata/global-metadata.dat
     co ton tai khong (unzip -l). Neu KHONG co -> khong phai IL2CPP chuan,
     Il2CppDumper khong dung duoc; chuyen sang RE libResources + Lua (xlua).

2. Neu co global-metadata.dat + lib il2cpp (tim trong APK/maps):
     Il2CppDumper.exe base.apk <libil2cpp>.so ./build/dump/
   Doc dump.cs, tim: HeroController/HeroEntity, FogOfWar, Camera,
   Tower/Structure, HeroManager/BattleEntityManager.

3. Can offset can dien (ten bien trong offsets.hpp):
     Camera_get/set_orthographicSize, Camera_WorldToScreenPoint, Camera_get_main
     Hero_Transform, Hero_HP_Current/Max, Hero_Mana_Current/Max,
     Hero_Level, Hero_TeamId, Hero_IsAlive, Hero_HeroName, Hero_SkillCooldown[4]
     HeroManager_GetAllHeroes, HeroManager_GetLocalHero, HeroManager_Instance
     FogOfWar_IsVisible, Bush_IsHeroHidden
     Tower_GetPosition, Tower_GetAttackRange, Tower_GetTeamId

4. Verify runtime: fill 1 offset -> build -> push .so -> mo game ->
   adb logcat -s aov_zygisk (xem [feat] logs, khong crash).
EOF
