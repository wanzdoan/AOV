#pragma once
// =============================================================================
//  AOV Anti-Cheat Shield — Tầng 3, 4, 5 Interface
//
//  Layer 3: Maps Hider  — gỡ soinfo node của our .so khỏi Android Linker list
//  Layer 4: Thread Rename — đổi tên thread bằng Direct Linux Syscall (svc #0)
//           để tránh pthread_setname_np bị Tersafe hook
//  Layer 5: Memory Scanning Countermeasure — vô hiệu hóa các luồng quét
//           bộ nhớ của Tersafe bằng cách đóng băng chúng với ptrace trước
//           khi ta thực hiện thao tác nhạy cảm
// =============================================================================
#include <cstdint>

namespace AntiCheat {

    // ── Tầng 3: Maps Hider ───────────────────────────────────────────────────
    // Xóa tên thư viện của ta khỏi /proc/self/maps bằng cách:
    //   1. Tìm node soinfo trong danh sách linker (__dl__ZL6solist)
    //   2. Unlink node đó khỏi linked list của linker
    //   3. Remap vùng nhớ sang [anon] để tên .so biến mất
    //
    // GỌI sau khi hackThread đã chạy ổn định (tránh gọi trong JNI_OnLoad).
    void hideSelfFromMaps();

    // ── Tầng 4: Direct Syscall Thread Rename ────────────────────────────────
    // Đổi tên thread hiện tại thành `newName` bằng Direct Linux Syscall
    // __NR_prctl (167 / 0xa7) + PR_SET_NAME, KHÔNG qua pthread_setname_np.
    // Tersafe đặt inline hook trên pthread_setname_np trong libc.so.
    void renameThreadDirect(const char* newName);

    // ── Tầng 5: Property Spoofer ─────────────────────────────────────────────
    // Hook __system_property_get để giả lập các giá trị prop nhạy cảm
    // (ro.build.tags, ro.debuggable, persist.sys.root_access, ...) ngay
    // trong không gian địa chỉ của tiến trình game.
    //
    // GỌI TRONG preAppSpecialize (trước khi game load thư viện anti-cheat).
    void installPropertyHook();

} // namespace AntiCheat
