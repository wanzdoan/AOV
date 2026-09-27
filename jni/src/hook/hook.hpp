#pragma once
// =============================================================================
//  AOV Zygisk — Hook Utility Module
//  Provides: maps_resolve, sym_resolve, hookAt, hookRva
// =============================================================================
#include <cstdint>

namespace Hook {
    bool     isExecutable(uintptr_t addr);
    uintptr_t mapsResolve(const char* libName);
    void*    symResolve(const char* libName, const char* symbol);
    bool     hookAt(void* target, void* hookFn, void** origFn);
    bool     hookRva(uintptr_t base, uintptr_t rva, void* hookFn, void** origFn);
    uintptr_t findPattern(const char* libName, const char* pattern);
    // Hook game RVA có guard offset 0 (chưa điền offset → warn + skip, no-op).
    bool     hookGameRva(const char* name, uintptr_t base, uintptr_t rva,
                         void* hookFn, void** origFn);
}
