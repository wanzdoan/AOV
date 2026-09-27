#pragma once
// =============================================================================
//  AOV Zygisk — ESP Rendering Module Header
// =============================================================================
#include <jni.h>

namespace zygisk { struct Api; }

namespace Esp {
    bool init();
    // Patch bảng hàm JNI để chặn RegisterNatives (gọi trong preAppSpecialize).
    // Pure memory write — an toàn trong fork, không gây "post fork hooks" abort.
    bool patchJniTable(JNIEnv* env);
    // Đảm bảo hook còn trong bảng (ZygiskNext có thể ghi đè sau mình).
    // Gọi đầu hackThread: attach JVM, kiểm tra + vá lại tới 15s.
    void ensureTouchHook();
    // Lấy kích thước màn hình thật qua WindowManager (1 lần, attach thread).
    void fetchRealDisplaySize();
    // true khi nativeInjectEvent đã hook xong (touch đi vào ImGui được)
    bool isTouchReady();
}
