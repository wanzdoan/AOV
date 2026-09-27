# AOV Zygisk Vendor — Zygisk Header (ĐÃ VENDORED, không tải lại)

`include/zygisk.hpp` là **official Zygisk API v5**, byte-exact từ:

```
https://raw.githubusercontent.com/topjohnwu/zygisk-module-sample/master/module/jni/zygisk.hpp
```

## ⚠️ Cảnh báo từ kinh nghiệm thực tế

- KHÔNG dùng header trôi nổi (ALEX5402/custom): sai entry signature
  `(Api*,env)` và sai layout `AppSpecializeArgs` → crash/fail trên máy thật.
- Header official có `DO NOT MODIFY`. Trước đó URL `Dr-TSNG/ZygiskNext/...
  /zygisk.hpp` là **404** — URL đúng duy nhất là `topjohnwu/zygisk-module-sample`
  ở trên.
- `zygisk.hpp` cần `<sys/types.h>` include trước (cho `dev_t`/`ino_t`) —
  đã làm trong `jni/main.cpp`.
