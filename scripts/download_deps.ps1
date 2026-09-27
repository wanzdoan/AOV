# Verify (khong tai de) vendor dependencies cho AOV Zygisk
# Run tu: D:\projects\KernelsuNextModule\AOV\
#
# Script nay CHI KIEM TRA file da vendored. Khong tai de cac file
# project-owned (custom android backend, dobby lib) de tranh vo build.
# Muon tai lai tu dau: xem huong dan trong jni/vendor/*/PLACE files.

param(
    [switch]$Force = $false
)

$ErrorActionPreference = "Stop"
$JNI = "jni\vendor"
$fail = 0

function Check-File($desc, $path) {
    if (Test-Path $path) {
        $size = (Get-Item $path).Length
        Write-Host "  [OK] $desc ($size bytes)"
    } else {
        Write-Host "  [MISS] $desc ($path)"
        $script:fail++
    }
}

Write-Host ""
Write-Host "============================================"
Write-Host "  AOV Zygisk - Vendor Dependency Check"
Write-Host "============================================"
Write-Host ""

Write-Host "[1/3] Zygisk API v5 header (official)..."
Check-File "zygisk.hpp" "$JNI\zygisk\include\zygisk.hpp"

Write-Host ""
Write-Host "[2/3] Dobby hook library..."
Check-File "dobby.h" "$JNI\dobby\include\dobby.h"
Check-File "libdobby.a (arm64-v8a)" "$JNI\dobby\lib\arm64-v8a\libdobby.a"

Write-Host ""
Write-Host "[3/3] Dear ImGui v1.91.5..."
$coreFiles = @("imgui.cpp","imgui.h","imgui_draw.cpp","imgui_internal.h",
    "imgui_tables.cpp","imgui_widgets.cpp","imconfig.h",
    "imstb_rectpack.h","imstb_textedit.h","imstb_truetype.h")
foreach ($f in $coreFiles) { Check-File $f "$JNI\imgui\$f" }
$beFiles = @("imgui_impl_opengl3.cpp","imgui_impl_opengl3.h",
    "imgui_impl_opengl3_loader.h")
foreach ($f in $beFiles) { Check-File $f "$JNI\imgui\backends\$f" }
Check-File "android backend H (CUSTOM)" "$JNI\imgui\backends\imgui_impl_android.h"
Check-File "android backend CPP (CUSTOM)" "$JNI\imgui\backends\imgui_impl_android.cpp"

Write-Host ""
if ($fail -eq 0) {
    Write-Host "  All dependencies present. Run scripts\build.bat"
} else {
    Write-Host "vendor files missing - xem PLACE files trong jni/vendor"
    exit 1
}
Write-Host ""
