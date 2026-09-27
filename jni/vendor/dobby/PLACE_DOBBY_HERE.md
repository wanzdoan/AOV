# AOV Zygisk Vendor — Dobby Hook Library (ĐÃ VENDORED)

```
jni/vendor/dobby/
├── include/
│   └── dobby.h          ← Dobby header (đã có)
└── lib/
    └── arm64-v8a/
        └── libdobby.a   ← Prebuilt static lib, ARM64 (đã có)
```

Link static trong `jni/CMakeLists.txt` (`add_library(dobby STATIC IMPORTED)`).

## Khôi phục khi cần (URL đã verify)

```powershell
New-Item -ItemType Directory -Force "jni/vendor/dobby/include"
Invoke-WebRequest "https://raw.githubusercontent.com/jmpews/Dobby/master/include/dobby.h" -OutFile "jni/vendor/dobby/include/dobby.h"
# libdobby.a: lấy từ https://github.com/jmpews/Dobby/releases
# (chọn bản arm64-v8a, đặt vào jni/vendor/dobby/lib/arm64-v8a/)
```
