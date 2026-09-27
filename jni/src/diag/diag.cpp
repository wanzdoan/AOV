// =============================================================================
//  AOV Zygisk — Runtime Diagnostics Implementation
//
//  Why: the hardcoded inject_event byte pattern doesn't exist in AOV's
//  libunity.so (Unity version differs), so touch routing is dead. Instead of
//  guessing, we inspect the REAL library at runtime:
//
//    1. scanDynsym() — opens libunity.so from our own app dir, parses ELF
//       section headers, iterates .dynsym/.dynstr, logs every symbol whose
//       name contains input-related keywords (with runtime addresses).
//    2. reflectClasses() — attaches to the JVM, FindClass() on Unity player
//       classes, logs every declared method via Method.toString() (this
//       reveals exact native method names + signatures, no guessing).
//
//  Next step (after reading logcat): hook the exact entry found here.
// =============================================================================
#include "diag.hpp"
#include "../../include/globals.hpp"

#include <link.h>
#include <elf.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <jni.h>

namespace Diag {

// ── Keywords for symbol scan ─────────────────────────────────────────────────
static const char* kKeywords[] = {
    "nject", "nput", "ouch", "otion", "eyboard", "ensor", "tiveWindow",
};
static bool matchKeyword(const char* name) {
    if (!name || !*name) return false;
    for (size_t i = 0; i < sizeof(kKeywords) / sizeof(kKeywords[0]); i++) {
        if (strstr(name, kKeywords[i])) return true;
    }
    return false;
}

// ── Locate libunity.so path + load bias ──────────────────────────────────────
struct LibInfo {
    const char* want;
    char        path[512];
    uintptr_t   bias;
    bool        found = false;
};

static int phdrCallback(struct dl_phdr_info* info, size_t, void* data) {
    auto* li = static_cast<LibInfo*>(data);
    if (info->dlpi_name && strstr(info->dlpi_name, li->want)) {
        strncpy(li->path, info->dlpi_name, sizeof(li->path) - 1);
        li->path[sizeof(li->path) - 1] = '\0';
        li->bias  = static_cast<uintptr_t>(info->dlpi_addr);
        li->found = true;
        return 1; // stop
    }
    return 0;
}

// ── .dynsym scan via ELF section headers (exact count, no overrun) ───────────
static void scanDynsym() {
    LibInfo li{"libunity.so"};
    dl_iterate_phdr(phdrCallback, &li);
    if (!li.found) {
        LOGW("[diag] libunity.so not found in phdr list");
        return;
    }
    LOGI("[diag] libunity path=%s bias=0x%lx", li.path, (unsigned long)li.bias);

    FILE* fp = fopen(li.path, "rb");
    if (!fp) {
        LOGW("[diag] cannot open %s", li.path);
        return;
    }

    Elf64_Ehdr ehdr;
    if (fread(&ehdr, 1, sizeof(ehdr), fp) != sizeof(ehdr) ||
        memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
        ehdr.e_shoff == 0 || ehdr.e_shnum == 0) {
        LOGW("[diag] bad ELF header");
        fclose(fp);
        return;
    }

    // Read all section headers
    size_t shdrsSize = (size_t)ehdr.e_shnum * sizeof(Elf64_Shdr);
    Elf64_Shdr* shdrs = static_cast<Elf64_Shdr*>(malloc(shdrsSize));
    if (!shdrs) { fclose(fp); return; }
    fseek(fp, (long)ehdr.e_shoff, SEEK_SET);
    if (fread(shdrs, 1, shdrsSize, fp) != shdrsSize) {
        free(shdrs); fclose(fp); return;
    }

    // Section-name string table
    if (ehdr.e_shstrndx >= ehdr.e_shnum) {
        free(shdrs); fclose(fp); return;
    }
    Elf64_Shdr& shstr = shdrs[ehdr.e_shstrndx];
    char* shstrtab = static_cast<char*>(malloc(shstr.sh_size + 1));
    if (!shstrtab) { free(shdrs); fclose(fp); return; }
    fseek(fp, (long)shstr.sh_offset, SEEK_SET);
    if (fread(shstrtab, 1, shstr.sh_size, fp) != shstr.sh_size) {
        free(shstrtab); free(shdrs); fclose(fp); return;
    }
    shstrtab[shstr.sh_size] = '\0';

    // Locate .dynsym + .dynstr
    Elf64_Shdr *dynsym = nullptr, *dynstr = nullptr;
    for (int i = 0; i < ehdr.e_shnum; i++) {
        const char* name = shstrtab + shdrs[i].sh_name;
        if (strcmp(name, ".dynsym") == 0) dynsym = &shdrs[i];
        if (strcmp(name, ".dynstr") == 0) dynstr = &shdrs[i];
    }
    if (!dynsym || !dynstr || dynsym->sh_entsize == 0) {
        LOGW("[diag] .dynsym/.dynstr not found");
        free(shstrtab); free(shdrs); fclose(fp);
        return;
    }

    size_t count = (size_t)(dynsym->sh_size / dynsym->sh_entsize);
    LOGI("[diag] .dynsym entries=%zu", count);

    Elf64_Sym* syms = static_cast<Elf64_Sym*>(malloc(dynsym->sh_size));
    char* strs = static_cast<char*>(malloc(dynstr->sh_size));
    if (!syms || !strs) {
        free(syms); free(strs); free(shstrtab); free(shdrs); fclose(fp);
        return;
    }
    fseek(fp, (long)dynsym->sh_offset, SEEK_SET);
    size_t symsOk = fread(syms, 1, dynsym->sh_size, fp);
    fseek(fp, (long)dynstr->sh_offset, SEEK_SET);
    size_t strsOk = fread(strs, 1, dynstr->sh_size, fp);
    fclose(fp);
    if (symsOk != dynsym->sh_size || strsOk != dynstr->sh_size) {
        free(syms); free(strs); free(shstrtab); free(shdrs);
        return;
    }

    // Iterate + match (cap output to avoid log spam)
    int logged = 0;
    const int kCap = 150;
    for (size_t i = 0; i < count && logged < kCap; i++) {
        if (syms[i].st_name >= dynstr->sh_size) continue;
        const char* name = strs + syms[i].st_name;
        if (!matchKeyword(name)) continue;
        unsigned char type = ELF64_ST_TYPE(syms[i].st_info);
        if (type != STT_FUNC && type != STT_OBJECT && type != STT_NOTYPE) continue;
        if (syms[i].st_shndx == SHN_UNDEF) continue;
        uintptr_t runtime = li.bias + (uintptr_t)syms[i].st_value;
        LOGI("[diag] sym: %s @ 0x%lx (%s)", name, (unsigned long)runtime,
             type == STT_FUNC ? "FUNC" : (type == STT_OBJECT ? "OBJ" : "NOTYPE"));
        logged++;
    }
    LOGI("[diag] dynsym scan done, matched=%d", logged);

    free(syms); free(strs); free(shstrtab); free(shdrs);
}

// ── Reflect one class, log all declared methods ──────────────────────────────
static void reflectClass(JNIEnv* env, const char* className) {
    jclass cls = env->FindClass(className);
    if (!cls) {
        env->ExceptionClear();
        LOGW("[diag] FindClass %s FAILED", className);
        return;
    }
    LOGI("[diag] FindClass %s OK", className);

    jclass classCls = env->GetObjectClass(cls);
    jmethodID getDeclaredMethods = env->GetMethodID(
        classCls, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
    if (!getDeclaredMethods) {
        env->ExceptionClear();
        LOGW("[diag] getDeclaredMethods lookup failed");
        return;
    }
    auto methods = static_cast<jobjectArray>(
        env->CallObjectMethod(cls, getDeclaredMethods));
    if (!methods || env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGW("[diag] getDeclaredMethods call failed");
        return;
    }

    jsize n = env->GetArrayLength(methods);
    LOGI("[diag] %s declares %d methods:", className, (int)n);

    jclass methodCls = env->FindClass("java/lang/reflect/Method");
    jmethodID toString = env->GetMethodID(methodCls, "toString",
                                          "()Ljava/lang/String;");
    const int kCap = 300;
    for (jsize i = 0; i < n && i < kCap; i++) {
        if ((i % 25) == 0) env->PushLocalFrame(64);
        jobject m = env->GetObjectArrayElement(methods, i);
        auto s = static_cast<jstring>(env->CallObjectMethod(m, toString));
        const char* c = s ? env->GetStringUTFChars(s, nullptr) : "(null)";
        LOGI("[diag] m: %s", c ? c : "(null)");
        if (s) {
            env->ReleaseStringUTFChars(s, c);
            env->DeleteLocalRef(s);
        }
        env->DeleteLocalRef(m);
        if ((i % 25) == 24 || i == n - 1 || i == kCap - 1)
            env->PopLocalFrame(nullptr);
    }
}

// ── Entry ────────────────────────────────────────────────────────────────────
void run() {
    LOGI("[diag] ===== start =====");
    scanDynsym();

    // JavaVM qua preEnv (env lúc preAppSpecialize) — GetJavaVM là thread-safe.
    // (dlsym JNI_GetCreatedJavaVMs thất bại trên máy này nên không dùng.)
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    if (!Global::preEnv || Global::preEnv->GetJavaVM(&vm) != 0 || !vm) {
        LOGW("[diag] no JavaVM, skip reflection");
    } else if (vm->AttachCurrentThread(&env, nullptr) == 0 && env) {
        reflectClass(env, "com/unity3d/player/UnityPlayer");
        reflectClass(env, "com/unity3d/player/UnityPlayerActivity");
        vm->DetachCurrentThread();
    } else {
        LOGW("[diag] AttachCurrentThread failed");
    }
    LOGI("[diag] ===== done =====");
}

} // namespace Diag
