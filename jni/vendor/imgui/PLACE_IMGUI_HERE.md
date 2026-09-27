# AOV Zygisk Vendor — Dear ImGui (ĐÃ VENDORED, không tải lại)

Thư mục này đã chứa đầy đủ Dear ImGui **v1.91.5** + backend OpenGL3 stock.

## ⚠️ File project-owned — KHÔNG ghi đè

`backends/imgui_impl_android.h` / `.cpp` là **backend custom viết riêng cho
Zygisk** (nhận touch raw từ `nativeInjectEvent` hook + kích thước từ EGL).
KHÔNG tải bản stock/ALEX5402 đè lên — sẽ vỡ API (`Init()` không tham số,
`HandleInputEvent(action,x,y,count)`, `NewFrame(w,h)`) và build fail.

## Khôi phục khi cần (chỉ core + opengl3, KHÔNG đụng android backend)

```powershell
$tag = "v1.91.5"
$base = "https://raw.githubusercontent.com/ocornut/imgui/$tag"
$core = @("imgui.cpp","imgui.h","imgui_draw.cpp","imgui_internal.h",
  "imgui_tables.cpp","imgui_widgets.cpp","imconfig.h",
  "imstb_rectpack.h","imstb_textedit.h","imstb_truetype.h")
foreach ($f in $core) {
    Invoke-WebRequest "$base/$f" -OutFile "jni/vendor/imgui/$f"
}
$be = @("imgui_impl_opengl3.cpp","imgui_impl_opengl3.h","imgui_impl_opengl3_loader.h")
foreach ($f in $be) {
    Invoke-WebRequest "$base/backends/$f" -OutFile "jni/vendor/imgui/backends/$f"
}
```

Build với `IMGUI_IMPL_OPENGL_ES3` (đã set trong `jni/CMakeLists.txt`).
