// =============================================================================
//  AOV Zygisk — Hook Utility Implementation
//
//  Provides:
//    - isExecutable():  validate pointer is executable
//    - mapsResolve():   find library load base via dl_iterate_phdr
//    - symResolve():    dlsym wrapper
//    - hookAt():        Dobby hook at absolute address
//    - hookRva():       Dobby hook at base+RVA
//    - findPattern():   byte-pattern scan in a loaded library
// =============================================================================
#include "hook.hpp"
#include "../../include/common.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <link.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include "../../vendor/dobby/include/dobby.h"

namespace Hook {

// ── isExecutable ─────────────────────────────────────────────────────────────
bool isExecutable(uintptr_t addr) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return false;

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        unsigned long long start, end;
        char perms[5];
        if (sscanf(line, "%llx-%llx %4s", &start, &end, perms) != 3) continue;
        if (addr >= start && addr < end) {
            found = (perms[2] == 'x');
            break;
        }
    }
    fclose(fp);
    return found;
}

// ── mapsResolve ──────────────────────────────────────────────────────────────
struct FindLibData {
    const char* name;
    uintptr_t   base;
};

static int findLibCallback(struct dl_phdr_info* info, size_t, void* data) {
    auto* d = static_cast<FindLibData*>(data);
    if (info->dlpi_name && strstr(info->dlpi_name, d->name)) {
        d->base = static_cast<uintptr_t>(info->dlpi_addr);
        return 1; // stop
    }
    return 0;
}

uintptr_t mapsResolve(const char* libName) {
    FindLibData data{libName, 0};
    dl_iterate_phdr(findLibCallback, &data);
    return data.base;
}

// ── symResolve ────────────────────────────────────────────────────────────────
void* symResolve(const char* libName, const char* symbol) {
    void* handle = dlopen(libName, RTLD_NOW | RTLD_NOLOAD);
    if (!handle) handle = dlopen(libName, RTLD_NOW);
    if (!handle) {
        LOGE("[hook] dlopen(%s) failed: %s", libName, dlerror());
        return nullptr;
    }
    void* sym = dlsym(handle, symbol);
    if (!sym) {
        LOGE("[hook] dlsym: %s not found in %s", symbol, libName);
    } else {
        LOGD("[hook] sym %s::%s = %p", libName, symbol, sym);
    }
    return sym;
}

// ── hookAt ───────────────────────────────────────────────────────────────────
bool hookAt(void* target, void* hookFn, void** origFn) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(target);

    if (!isExecutable(addr)) {
        LOGE("[hook] REJECT non-executable: %p", target);
        return false;
    }

    int ret = DobbyHook(target, hookFn, origFn);
    if (ret == 0) {
        LOGI("[hook] DobbyHook OK: %p → %p (orig=%p)", target, hookFn, *origFn);
        return true;
    } else {
        LOGE("[hook] DobbyHook FAIL: %p → %p (ret=%d)", target, hookFn, ret);
        return false;
    }
}

// ── hookRva ──────────────────────────────────────────────────────────────────
bool hookRva(uintptr_t base, uintptr_t rva, void* hookFn, void** origFn) {
    void* target = reinterpret_cast<void*>(base + rva);
    return hookAt(target, hookFn, origFn);
}

// ── hookGameRva ──────────────────────────────────────────────────────────────
// Hook có guard: RVA 0 (offset chưa điền) → warn + skip, không chạm bộ nhớ.
bool hookGameRva(const char* name, uintptr_t base, uintptr_t rva,
                 void* hookFn, void** origFn) {
    if (rva == 0) {
        LOGW("[hook] %s: offset = 0 (chua dien) — skip", name ? name : "?");
        return false;
    }
    if (!base) {
        LOGW("[hook] %s: lib base = 0 — skip", name ? name : "?");
        return false;
    }
    bool ok = hookRva(base, rva, hookFn, origFn);
    LOGI("[hook] %s @ base+0x%lx: %s", name ? name : "?",
         (unsigned long)rva, ok ? "OK" : "FAIL");
    return ok;
}

// ── findPattern ──────────────────────────────────────────────────────────────
// Scans a library's executable pages for a byte pattern.
// Pattern format: "FF 83 01 D1 ? ? 03 A9" (space-separated hex, '?' = wildcard)
uintptr_t findPattern(const char* libName, const char* pattern) {
    // Find library boundaries from /proc/self/maps
    char line[512];
    uintptr_t start = 0, end = 0;

    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, libName)) {
            unsigned long long s, e;
            char perms[5];
            if (sscanf(line, "%llx-%llx %4s", &s, &e, perms) == 3 && perms[2] == 'x') {
                if (start == 0) start = (uintptr_t)s;
                end = (uintptr_t)e;
            }
        }
    }
    fclose(fp);

    if (!start || !end) {
        LOGW("[hook] findPattern: %s not found in maps", libName);
        return 0;
    }

    // Parse pattern into bytes + mask
    const size_t maxLen = 512;
    uint8_t patBytes[maxLen];
    bool    patMask[maxLen];
    size_t  patLen = 0;

    const char* p = pattern;
    while (*p && patLen < maxLen) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '?') {
            patBytes[patLen] = 0x00;
            patMask[patLen]  = false; // wildcard
            patLen++;
            p++;
            if (*p == '?') p++; // skip second '?'
        } else {
            patBytes[patLen] = (uint8_t)strtoul(p, nullptr, 16);
            patMask[patLen]  = true;  // must match
            patLen++;
            p += 2;
        }
    }

    // Scan
    const uint8_t* mem = reinterpret_cast<const uint8_t*>(start);
    const size_t   len = end - start;
    for (size_t i = 0; i + patLen <= len; i++) {
        bool match = true;
        for (size_t j = 0; j < patLen; j++) {
            if (patMask[j] && mem[i+j] != patBytes[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            uintptr_t found = start + i;
            LOGI("[hook] findPattern match in %s at 0x%lx", libName, (unsigned long)found);
            return found;
        }
    }

    LOGW("[hook] findPattern: pattern not found in %s", libName);
    return 0;
}

} // namespace Hook
