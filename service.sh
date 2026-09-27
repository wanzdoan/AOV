#!/system/bin/sh
# =============================================================================
#  AOV Anti-Cheat Shield — Tầng 1 (tiếp) + Hệ Thống Ổn Định
#
#  Chạy sau Zygote init (service.sh). Lúc này game chưa khởi động.
#  Mục đích:
#    1. Bổ sung resetprop cho các prop build fingerprint mà Tersafe đọc
#       sau khi Zygote đã khởi (ro.build.* không thể reset từ post-fs-data)
#    2. Ẩn dấu vết debug.is-eng, ro.debuggable khỏi Tersafe
#    3. Vô hiệu hóa USB debugging flag (tắt các kênh debug Tersafe khai thác)
# =============================================================================

MODULE_DIR="${0%/*}"

ac_log() { log -p i -t "aov_ac_layer1b" "$*"; }

ac_log "[Layer1b] service.sh START"

# ── Ẩn dấu vết debug khỏi Tersafe ────────────────────────────────────────────
# Tersafe kiểm tra các prop này để phát hiện môi trường rooted/debug
resetprop "ro.debuggable"       "0"     2>/dev/null
resetprop "ro.secure"           "1"     2>/dev/null
resetprop "debug.atrace.tags.enableflags" "0" 2>/dev/null
ac_log "[Layer1b] debug props masked"

# ── Ẩn ro.build.tags khỏi "test-keys" ────────────────────────────────────────
# KernelSU/APatch thường xuất hiện với "test-keys" trong build.tags
# Tersafe dùng prop này để nhận diện thiết bị bị can thiệp
CURRENT_TAGS=$(getprop ro.build.tags 2>/dev/null)
if echo "$CURRENT_TAGS" | grep -q "test-keys"; then
    resetprop "ro.build.tags" "release-keys"
    ac_log "[Layer1b] ro.build.tags: test-keys -> release-keys"
fi

# ── Ẩn KernelSU-specific props ───────────────────────────────────────────────
for prop in ro.kernelsu.version ro.adb.secure; do
    VAL=$(getprop "$prop" 2>/dev/null)
    if [ -n "$VAL" ]; then
        resetprop --delete "$prop" 2>/dev/null
        ac_log "[Layer1b] deleted KSU prop: $prop"
    fi
done

# ── Đặt adb.secure = 1 (Tersafe kiểm tra xem ADB có bị mở không) ─────────────
resetprop "ro.adb.secure" "1" 2>/dev/null
resetprop "service.adb.root" "0" 2>/dev/null
ac_log "[Layer1b] adb.secure hardened"

# ── Chờ đến khi game process tồn tại (không block vô hạn) ───────────────────
# service.sh chạy async, không cần wait game — Zygisk hook sẽ chạy khi game fork
ac_log "[Layer1b] service.sh DONE"
exit 0
