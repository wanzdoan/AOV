#!/system/bin/sh
# =============================================================================
#  AOV Anti-Cheat Shield — Tầng 1: Môi Trường Hệ Thống & Ẩn Root
#
#  Chạy sớm nhất (post-fs-data), trước khi Zygote fork game.
#  Nguồn: Phân tích ZyGames service.sh + post-fs-data.sh
#
#  Chức năng:
#    1. Gỡ bỏ dấu vết Recovery/Bootloader (ro.boot.mode)
#    2. Khoá chặt SELinux enforcement file permissions
#       → Tersafe đọc /sys/fs/selinux/* để kiểm tra can thiệp KernelSU
#    3. Vô hiệu hóa Hidden API Blacklist
#       → Tersafe dùng Java Reflection để tìm class lạ trong runtime
#    4. Chặn máy chủ AC của Tencent/Garena qua Private DNS
# =============================================================================

MODULE_DIR="${0%/*}"
MODPATH="${MODULE_DIR}"

# ── Helper: log với timestamp ─────────────────────────────────────────────────
ac_log() { log -p i -t "aov_ac_layer1" "$*"; }

ac_log "[Layer1] post-fs-data.sh START"

# ── 1. Gỡ bỏ dấu vết Recovery/Bootloader ─────────────────────────────────────
# Tersafe kiểm tra ro.boot.mode; "recovery" bị cấm
resetprop_if_match() {
    local prop="$1" match="$2" replacement="$3"
    local val
    val=$(getprop "$prop" 2>/dev/null)
    if [ "$val" = "$match" ]; then
        resetprop "$prop" "$replacement"
        ac_log "[Layer1] resetprop $prop: $match -> $replacement"
    fi
}
resetprop_if_match "ro.boot.mode"           "recovery"  "unknown"
resetprop_if_match "ro.bootmode"            "recovery"  "unknown"
resetprop_if_match "ro.boot.verifiedbootstate" "orange" "green"

# ── 2. Khoá chặt SELinux file permissions ────────────────────────────────────
# Tersafe mở /sys/fs/selinux/enforce và /sys/fs/selinux/policy để phát hiện
# trạng thái can thiệp. Đặt quyền 640/440 ngăn tiến trình game đọc.
for f in /sys/fs/selinux/enforce /sys/fs/selinux/policy; do
    if [ -f "$f" ]; then
        case "$f" in
            */enforce) chmod 640 "$f" 2>/dev/null && ac_log "[Layer1] chmod 640 $f" ;;
            */policy)  chmod 440 "$f" 2>/dev/null && ac_log "[Layer1] chmod 440 $f" ;;
        esac
    fi
done

# ── 3. Vô hiệu hóa Hidden API Blacklist ──────────────────────────────────────
# Tersafe dùng Java Reflection qua hidden API để tìm class lạ trong runtime.
# Xóa 2 property này vô hiệu hóa cơ chế kiểm tra đó.
for prop in hidden_api_policy hidden_api_blacklist_exemptions; do
    resetprop --delete "$prop" 2>/dev/null
    ac_log "[Layer1] deleted prop: $prop"
done

# ── 4. Chặn máy chủ Anti-Cheat Tencent/Garena bằng Private DNS ───────────────
# Chuyển Private DNS sang AdGuard để chặn các domain báo cáo vi phạm
# Tránh tắt DNS hoàn toàn — game vẫn cần kết nối server game (IP khác)
resetprop "persist.sys.dns_mode"    "private"   2>/dev/null
resetprop "persist.sys.private_dns_provider_name" "dns.adguard.com" 2>/dev/null
ac_log "[Layer1] DNS private mode set -> dns.adguard.com"

ac_log "[Layer1] post-fs-data.sh DONE"
exit 0
