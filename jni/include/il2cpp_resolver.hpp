#pragma once
// =============================================================================
//  il2cpp_resolver.hpp â€” By-name IL2CPP method/field resolver for AOV module.
//  Port of the original Mod's IL2Cpp.hpp + Il2cppApiScanner infrastructure.
//  Game: com.garena.game.kgvn (Arena of Valor / LiÃªn QuÃ¢n Mobile) 1.63.1.14
//  Offsets are resolved at RUNTIME via il2cpp metadata â€” no hardcoded RVAs.
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <string>
#include <map>
#include "dobby.h"

// ---- DLL + namespace constants (all features share these) -------------------
#define DLL_MAIN   "Project_d.dll"
#define DLL_PLUG   "Project.Plugins_d.dll"
#define NS_LOGIC   "Assets.Scripts.GameLogic"
#define NS_SYSTEM  "Assets.Scripts.GameSystem"
#define NS_FRAME   "Assets.Scripts.Framework"
#define NS_VAGE    "Kyrios.VAGE"
#define NS_ACTOR   "Kyrios.Actor"
#define NS_KERNAL  "NucleusDrive.Logic.GameKernal"
#define NS_NLOGIC  "NucleusDrive.Logic"

// ---- Raw il2cpp exports (filled by Init_Il2cpp_Symbol + ScanIl2cppApi) -----
namespace il2cppExports {
    extern void*       (*il2cpp_domain_get)();
    extern void*       (*il2cpp_thread_attach)(void* domain);
    extern int         (*il2cpp_is_vm_thread)(void* thread);
    extern void**      (*il2cpp_domain_get_assemblies)(void* domain, size_t* size);
    extern void*       (*il2cpp_assembly_get_image)(void* assembly);
    extern const char* (*il2cpp_image_get_name)(void* image);
    extern void*       (*il2cpp_class_from_name)(void* image, const char* ns, const char* name);
    extern void*       (*il2cpp_class_get_method_from_name)(void* klass, const char* name, int argsCount);
    extern void*       (*il2cpp_class_get_field_from_name)(void* klass, const char* name);
    extern size_t      (*il2cpp_field_get_offset)(void* field);
    extern void*       (*il2cpp_string_new)(const char* str);  // managed string factory
    extern void*       (*il2cpp_runtime_invoke)(void* method, void* obj, void** params, void** exc);
}

// ---- Standard il2cpp MethodInfo layout --------------------------------------
struct Il2CppMethodInfo { void* methodPointer; /* first field */ };

// Bind all exports + attach current thread to il2cpp domain.
void Init_Il2cpp_Symbol();

// ARM64 BL-histogram scanner: recover stripped il2cpp_* addresses from
// libil2cpp.so when .dynsym has been stripped. Returns true on success.
bool ScanIl2cppApi();

// ---- By-name resolver namespace --------------------------------------------
namespace IL2Cpp {
    extern std::map<std::string, uintptr_t> LOOP;   // resolve cache

    void* Il2CppGetImageByName(const char* dll);

    // True once il2cpp metadata is queryable (domain up, game class resolves).
    bool Il2cppReady();

    // Verify recovered API resolves a known class; re-scan up to maxAttempts times.
    bool EnsureApiHealthy(int maxAttempts);

    // Resolve managed method â†’ native code pointer (cached).
    uintptr_t Il2CppGetMethodOffset(const char* dll, const char* ns,
                                    const char* klass, const char* method,
                                    int argc, int token = 0);

    // Resolve managed instance field â†’ byte offset from object base (cached).
    uintptr_t Il2CppGetFieldOffset(const char* dll, const char* ns,
                                   const char* klass, const char* field);

    // ---- Runtime field access by name on a live il2cpp managed object -------
    // Object layout: *(void**)obj = Il2CppClass* (klass), then fields follow.
    inline void* ObjClass(void* obj) { return obj ? *(void**)obj : nullptr; }
    inline size_t FieldOffsetOf(void* obj, const char* field) {
        using namespace il2cppExports;
        void* k = ObjClass(obj);
        void* f = (k && il2cpp_class_get_field_from_name)
                      ? il2cpp_class_get_field_from_name(k, field) : nullptr;
        return f ? il2cpp_field_get_offset(f) : 0;
    }
    template <typename T> inline T GetField(void* obj, const char* field) {
        size_t o = FieldOffsetOf(obj, field);
        return o ? *(T*)((char*)obj + o) : T{};
    }
    template <typename T> inline void SetField(void* obj, const char* field, T v) {
        size_t o = FieldOffsetOf(obj, field);
        if (o) *(T*)((char*)obj + o) = v;
    }
    inline void* GetFieldObj(void* obj, const char* field) { return GetField<void*>(obj, field); }
}

// ---- Convenience macro: resolve + DobbyHook a method by name ----------------
#define HOOK_IL2CPP(dll, ns, cls, mtd, argc, detour, orig)                        \
    do {                                                                          \
        void* _p = (void*)IL2Cpp::Il2CppGetMethodOffset(dll, ns, cls, mtd, argc);\
        if (_p && !(orig)) DobbyHook(_p, (void*)(detour),            \
                          (void**)&(orig));                          \
    } while (0)

// Convenience shorthand used throughout feature files
using namespace IL2Cpp;
