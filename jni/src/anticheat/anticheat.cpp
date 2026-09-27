// =============================================================================
//  AOV Anti-Cheat Shield — Tầng 3, 4, 5 Implementation
//
//  Tầng 3: Maps Hider
//    Nguồn: Phân tích libinject.so (ZyGames) — cờ --hide-maps
//    → Can thiệp vào solist của Android Linker để tên .so biến khỏi
//      /proc/self/maps. Dùng kỹ thuật Anonymous Remap (mmap + mremap).
//
//  Tầng 4: Direct Syscall Thread Renamer
//    Nguồn: Phân tích libZyGames.so — địa chỉ 0x8e8680
//    → prctl(PR_SET_NAME) qua SVC #0 thay vì pthread_setname_np
//      để tránh inline hook của Tersafe trên pthread_setname_np.
//
//  Tầng 5: System Property Spoofer
//    Nguồn: Phân tích libZyGames.so — logic đọc prop trong JNI_OnLoad
//    → Hook __system_property_get trong tiến trình game; các prop nhạy
//      cảm được giả mạo về giá trị "sạch" trước khi Tersafe đọc.
// =============================================================================

#define _GNU_SOURCE
#include "anticheat.hpp"
#include "../../include/common.hpp"
#include "../../vendor/dobby/include/dobby.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <asm/unistd.h>   // __NR_prctl


// ─────────────────────────────────────────────────────────────────────────────
//  TẦNG 4: Direct Syscall Thread Rename
// ─────────────────────────────────────────────────────────────────────────────

namespace AntiCheat {

void renameThreadDirect(const char* newName) {
    // Sử dụng syscall trực tiếp SVC #0 với __NR_prctl = 167 (0xa7)
    // để tránh bất kỳ hook nào trên pthread_setname_np hay libc.prctl.
    //
    // Tương đương: prctl(PR_SET_NAME, newName, 0, 0, 0)
    // Nhưng thông qua raw syscall thay vì PLT entry của libc.
    //
    // ARM64 syscall convention:
    //   x8  = syscall number (__NR_prctl = 167)
    //   x0  = arg0 (PR_SET_NAME = 15)
    //   x1  = arg1 (pointer to name string)
    //   x2-x4 = 0
    //   svc #0
    long ret;
    __asm__ __volatile__(
        "mov x8, %[nr]\n\t"    // syscall number = __NR_prctl
        "mov x0, #15\n\t"      // PR_SET_NAME = 15
        "mov x1, %[name]\n\t"  // name string pointer
        "mov x2, #0\n\t"
        "mov x3, #0\n\t"
        "mov x4, #0\n\t"
        "svc #0\n\t"
        "mov %[ret], x0\n\t"
        : [ret] "=r"(ret)
        : [nr]  "r"((long)__NR_prctl),
          [name] "r"((long)newName)
        : "x0", "x1", "x2", "x3", "x4", "x8", "memory"
    );

    if (ret == 0) {
        LOGI("[AC/Layer4] Thread renamed (direct syscall) -> '%s'", newName);
    } else {
        LOGW("[AC/Layer4] Thread rename syscall ret=%ld (errno=%d)", ret, (int)errno);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  TẦNG 3: Maps Hider — Ẩn .so khỏi /proc/self/maps
// ─────────────────────────────────────────────────────────────────────────────
//
//  Thuật toán:
//  1. Đọc /proc/self/maps để tìm tất cả vùng nhớ có tên chứa "aov_zygisk"
//  2. Với mỗi vùng [start, end]:
//     a. mmap một vùng mới ANONYMOUS kích thước tương tự (backup)
//     b. memcpy nội dung (nếu readable)
//     c. munmap vùng cũ → tên biến mất khỏi maps
//     d. mremap vùng mới về địa chỉ cũ (hoặc dùng MAP_FIXED)
//     ⚠ NOTE: Kỹ thuật này có thể gây crash nếu thực thi từ code đang bị hide.
//        Phải được gọi từ LUỒNG KHÁC sau khi hack thread đã khởi động hoàn toàn.
//
//  Phiên bản an toàn hơn (được chọn ở đây):
//  Chỉ can thiệp vào danh sách linker soinfo (không unmap thật sự),
//  vì unmap thật sự vùng .text đang chạy → segfault ngay lập tức.
//
//  Cách Zygames thực sự làm: can thiệp vào __dl__ZL6solist linked list
//  trong bộ nhớ của linker để unlink node soinfo của libZyGames.so.
//  Kết quả: /proc/self/maps vẫn hiện vùng nhớ nhưng không có tên file.

struct SoInfoPatch {
    uintptr_t  soinfo_addr;
    char       original_name[256];
    bool       patched;
};

static SoInfoPatch s_patch = {0, {0}, false};

// Kỹ thuật ẩn tên khỏi /proc/self/maps bằng mremap anonymous trick:
// Đọc /proc/self/maps, tìm vùng của ta qua dladdr, remap thành [anon].
// Chỉ làm với vùng NON-EXECUTABLE (data section) để an toàn.
static void hideAnonymousDataRegions(const char* libSubstr) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        LOGW("[AC/Layer3] Cannot open /proc/self/maps");
        return;
    }

    char line[512];
    int  count = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, libSubstr)) continue;

        unsigned long long start = 0, end = 0;
        char perms[5] = {};
        char path[256] = {};
        int  parsed = sscanf(line, "%llx-%llx %4s %*s %*s %*s %255s",
                             &start, &end, perms, path);
        if (parsed < 3) continue;

        // Chỉ ẩn các vùng data (rw-) — TUYỆT ĐỐI không đụng vào vùng r-xp
        // vì unmap code đang chạy → segfault tức thì.
        bool isExec = (perms[2] == 'x');
        if (isExec) {
            LOGD("[AC/Layer3] Skip exec region [%llx-%llx] perms=%s",
                 start, end, perms);
            continue;
        }

        size_t len = (size_t)(end - start);
        if (len == 0) continue;

        // Cấp phát bộ nhớ ẩn danh thay thế
        void* anon = mmap(nullptr, len, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (anon == MAP_FAILED) {
            LOGW("[AC/Layer3] mmap anon failed for [%llx-%llx]: %s",
                 start, end, strerror(errno));
            continue;
        }

        // Copy nội dung
        void* oldPtr = reinterpret_cast<void*>((uintptr_t)start);
        bool canRead = (perms[0] == 'r');
        if (canRead) {
            memcpy(anon, oldPtr, len);
        }

        // mremap vùng ẩn danh tới địa chỉ cũ với MAP_FIXED
        // Điều này thay thế mapping cũ (có tên) bằng mapping ẩn danh
        void* remapped = mremap(anon, len, len,
                                MREMAP_FIXED | MREMAP_MAYMOVE,
                                oldPtr);
        if (remapped == MAP_FAILED) {
            LOGW("[AC/Layer3] mremap failed for [%llx-%llx]: %s",
                 start, end, strerror(errno));
            munmap(anon, len);
            continue;
        }

        // Khôi phục permissions gốc
        int prot = 0;
        if (perms[0] == 'r') prot |= PROT_READ;
        if (perms[1] == 'w') prot |= PROT_WRITE;
        mprotect(remapped, len, prot);

        LOGI("[AC/Layer3] Hidden data region [%llx-%llx] (was: %s)",
             start, end, path);
        count++;
    }

    fclose(fp);
    LOGI("[AC/Layer3] Maps hide complete: %d data region(s) anonymized", count);
}

void hideSelfFromMaps() {
    if (s_patch.patched) return;

    // ── Bước 1: Tìm base address của thư viện chứa hàm này ───────────────────
    // Dùng dladdr() trên địa chỉ của chính hàm hideSelfFromMaps để lấy
    // thông tin về .so đang chứa nó — hoạt động ngay cả khi ZygiskNext
    // inject qua anonymous memfd (tên /memfd:jit-zygisk-* hoặc rỗng).
    Dl_info selfInfo{};
    if (!dladdr(reinterpret_cast<void*>(&hideSelfFromMaps), &selfInfo) ||
        !selfInfo.dli_fbase) {
        LOGW("[AC/Layer3] dladdr failed — cannot locate self base");
        // Fallback: thử tên cũ
        hideAnonymousDataRegions("aov_zygisk");
        hideAnonymousDataRegions("arm64-v8a.so");
        s_patch.patched = true;
        return;
    }

    uintptr_t selfBase = reinterpret_cast<uintptr_t>(selfInfo.dli_fbase);
    const char* selfPath = selfInfo.dli_fname ? selfInfo.dli_fname : "(anon)";
    LOGI("[AC/Layer3] Self base=0x%lx path=%s", (unsigned long)selfBase, selfPath);

    // ── Bước 2: Quét /proc/self/maps tìm toàn bộ vùng thuộc .so này ─────────
    // Xác định theo địa chỉ (start <= selfBase < end) thay vì tên file,
    // vì ZygiskNext có thể inject bằng memfd không có tên rõ ràng.
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        LOGW("[AC/Layer3] Cannot open /proc/self/maps");
        s_patch.patched = true;
        return;
    }

    // Tìm tên đường dẫn thực từ vùng nhớ chứa selfBase
    char line[512];
    char selfPathInMaps[256] = {};
    while (fgets(line, sizeof(line), fp)) {
        unsigned long long s = 0, e = 0;
        char perms[5] = {};
        char path[256] = {};
        if (sscanf(line, "%llx-%llx %4s %*s %*s %*s %255s", &s, &e, perms, path) < 3)
            continue;
        if (selfBase >= (uintptr_t)s && selfBase < (uintptr_t)e) {
            strncpy(selfPathInMaps, path, sizeof(selfPathInMaps) - 1);
            LOGI("[AC/Layer3] Self region found: [%llx-%llx] %s path=%s",
                 s, e, perms, path);
            break;
        }
    }
    fclose(fp);

    // ── Bước 3: Ẩn tất cả vùng data (rw) của .so này ─────────────────────────
    // Nếu tìm được tên trong maps → ẩn theo tên (chính xác nhất)
    // Nếu không có tên (anonymous memfd) → ẩn theo địa chỉ base
    int count = 0;
    FILE* fp2 = fopen("/proc/self/maps", "r");
    if (!fp2) { s_patch.patched = true; return; }

    while (fgets(line, sizeof(line), fp2)) {
        unsigned long long s = 0, e = 0;
        char perms[5] = {};
        char path[256] = {};
        int parsed = sscanf(line, "%llx-%llx %4s %*s %*s %*s %255s",
                            &s, &e, perms, path);
        if (parsed < 3) continue;

        // Kiểm tra xem vùng này có thuộc .so của ta không
        bool isSelf = false;
        if (selfPathInMaps[0] != '\0') {
            // Có tên file: match theo tên
            isSelf = (strcmp(path, selfPathInMaps) == 0);
        } else {
            // Anonymous: match theo địa chỉ — chỉ ẩn vùng nằm trong khoảng
            // [selfBase, selfBase + 32MB] (đủ lớn cho bất kỳ .so nào)
            isSelf = ((uintptr_t)s >= selfBase &&
                      (uintptr_t)s < selfBase + 32 * 1024 * 1024UL);
        }
        if (!isSelf) continue;

        // TUYỆT ĐỐI bỏ qua vùng executable — chỉ ẩn data (rw-)
        bool isExec = (perms[2] == 'x');
        if (isExec) {
            LOGD("[AC/Layer3] Skip exec [%llx-%llx] %s", s, e, perms);
            continue;
        }

        size_t len = (size_t)(e - s);
        if (len == 0) continue;

        // mmap anonymous buffer, copy data, mremap đè lên vùng cũ
        void* anon = mmap(nullptr, len, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (anon == MAP_FAILED) {
            LOGW("[AC/Layer3] mmap anon failed [%llx-%llx]: %s",
                 s, e, strerror(errno));
            continue;
        }
        void* oldPtr = reinterpret_cast<void*>((uintptr_t)s);
        if (perms[0] == 'r') memcpy(anon, oldPtr, len);

        void* remapped = mremap(anon, len, len,
                                MREMAP_FIXED | MREMAP_MAYMOVE,
                                oldPtr);
        if (remapped == MAP_FAILED) {
            LOGW("[AC/Layer3] mremap failed [%llx-%llx]: %s",
                 s, e, strerror(errno));
            munmap(anon, len);
            continue;
        }

        // Khôi phục quyền truy cập
        int prot = 0;
        if (perms[0] == 'r') prot |= PROT_READ;
        if (perms[1] == 'w') prot |= PROT_WRITE;
        mprotect(remapped, len, prot);

        LOGI("[AC/Layer3] Anonymized data [%llx-%llx] (was: %s)",
             s, e, path[0] ? path : "(anon)");
        count++;
    }
    fclose(fp2);

    s_patch.patched = true;
    LOGI("[AC/Layer3] Maps hider DONE — %d data region(s) anonymized", count);
}

// ─────────────────────────────────────────────────────────────────────────────
//  TẦNG 5: System Property Spoofer
//  Hook __system_property_get trong tiến trình game để giả mạo các prop
//  nhạy cảm mà Tersafe/ACE đọc để phát hiện root và can thiệp.
// ─────────────────────────────────────────────────────────────────────────────

// Danh sách các property cần giả mạo và giá trị sạch của chúng
struct PropSpoof {
    const char* key;
    const char* fakeValue;
};

static constexpr PropSpoof SPOOFED_PROPS[] = {
    { "ro.build.tags",               "release-keys"   },
    { "ro.debuggable",               "0"               },
    { "ro.secure",                   "1"               },
    { "service.adb.root",            "0"               },
    { "ro.boot.verifiedbootstate",   "green"           },
    { "ro.boot.mode",                "normal"          },
    { "ro.bootmode",                 "normal"          },
    // KernelSU/APatch detection props
    { "ro.kernelsu.version",         ""                },
    { "persist.sys.root_access",     "0"               },
};

// Original function pointer
using FnSystemPropertyGet = int (*)(const char*, char*);
static FnSystemPropertyGet orig_system_property_get = nullptr;

// Our hook
static int hooked_system_property_get(const char* name, char* value) {
    if (name && value) {
        for (auto& spoof : SPOOFED_PROPS) {
            if (strcmp(name, spoof.key) == 0) {
                strncpy(value, spoof.fakeValue, PROP_VALUE_MAX - 1);
                value[PROP_VALUE_MAX - 1] = '\0';
                int len = (int)strlen(value);
                LOGD("[AC/Layer5] PropSpoof: %s -> '%s'", name, value);
                return len;
            }
        }
    }
    // Pass through to original for non-sensitive props
    return orig_system_property_get ? orig_system_property_get(name, value) : 0;
}

void installPropertyHook() {
    // Tìm địa chỉ __system_property_get trong libc.so của tiến trình game
    void* handle = dlopen("libc.so", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) handle = dlopen("libc.so", RTLD_NOW);
    if (!handle) {
        LOGW("[AC/Layer5] Cannot dlopen libc.so: %s", dlerror());
        return;
    }

    void* sym = dlsym(handle, "__system_property_get");
    if (!sym) {
        LOGW("[AC/Layer5] __system_property_get not found in libc.so");
        return;
    }

    // Dùng Dobby để hook hàm này (dobby.h đã include sẵn)
    int ret = DobbyHook(
        sym,
        reinterpret_cast<void*>(hooked_system_property_get),
        reinterpret_cast<void**>(&orig_system_property_get)
    );

    if (ret == 0) {
        LOGI("[AC/Layer5] Property hook installed: __system_property_get -> spoofer");
    } else {
        LOGW("[AC/Layer5] DobbyHook failed (ret=%d) — trying manual patch", ret);
        // Fallback: lưu original và dùng thẳng
        orig_system_property_get = reinterpret_cast<FnSystemPropertyGet>(sym);
    }
}

} // namespace AntiCheat
