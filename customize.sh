#!/system/bin/sh

SKIPUNZIP=1

ui_print "- AOV Zygisk v2.6.0 (1.63.1.14 + Full Features)"
ui_print "- Target: com.garena.game.kgvn 1.63.1.14"

if [ "$KSU" = "true" ]; then
    ui_print "- Root provider: KernelSU"
elif [ -n "$MAGISK_VER" ]; then
    ui_print "- Root provider: Magisk $MAGISK_VER"
else
    abort "! Unsupported root environment"
fi

case "$(uname -m)" in
    aarch64) ui_print "- ABI: arm64-v8a" ;;
    *) abort "! This build supports arm64-v8a only" ;;
esac

if [ "${ZYGISK_ENABLED:-0}" != "1" ]; then
    ui_print "! Zygisk was not reported as enabled by the installer"
    ui_print "! KernelSU users need ZygiskNext and a reboot"
fi

ui_print "- Extracting module files"
unzip -o "$ZIPFILE" 'module.prop' -d "$MODPATH" >&2
unzip -o "$ZIPFILE" 'zygisk/*' -d "$MODPATH" >&2
unzip -o "$ZIPFILE" 'webroot/*' -d "$MODPATH" >&2
unzip -o "$ZIPFILE" 'post-fs-data.sh' 'post-mount.sh' 'service.sh' \
    -d "$MODPATH" >&2

[ -s "$MODPATH/zygisk/arm64-v8a.so" ] || \
    abort "! Missing zygisk/arm64-v8a.so"

set_perm_recursive "$MODPATH" root root 0755 0644
set_perm "$MODPATH/post-fs-data.sh" root root 0755
set_perm "$MODPATH/post-mount.sh" root root 0755
set_perm "$MODPATH/service.sh" root root 0755
set_perm "$MODPATH/zygisk/arm64-v8a.so" root root 0644

ui_print "- === FEATURES ==="
ui_print "- Anti-Cheat: 5-Layer (SELinux+Unmount+MapsHide+SVC0+PropSpoof)"
ui_print "- Hack Map V2: LVActorLinker.SetVisible il2cpp hook"
ui_print "- Time Hoi Chieu: Phu Tro VANG | C1/C2/C3 TRANG"
ui_print "- Aimbot: C1/C2/Ulti with lead calculation"
ui_print "- Auto Boc Pha: Flash when enemy HP <= 15% missing"
ui_print "- Auto Trung Tri: Smite epic/buff monsters"
ui_print "- Unlock All Skins: 14 il2cpp hooks"
ui_print "- Kill-Notify + Button Skin: in-lobby patch"
ui_print "- Unlock FPS 120: 10 GameSettings hooks"
ui_print "- Camera Zoom: CameraSystem.GetCameraHeightRateValue"
ui_print "- Reboot, open AOV, then tap the [A] button"
