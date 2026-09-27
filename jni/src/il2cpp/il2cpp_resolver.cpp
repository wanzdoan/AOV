// =============================================================================
//  il2cpp_resolver.cpp — Runtime il2cpp API binding + by-name resolver.
//  Port of Mod's IL2Cpp.cpp + Il2cppApiScanner.cpp.
//  This file implements two things:
//   1. Init_Il2cpp_Symbol(): bind il2cpp_* exports from libil2cpp.so (stripped
//      on this game → falls back to BL-histogram scanner).
//   2. IL2Cpp::Il2CppGetMethodOffset/Il2CppGetFieldOffset(): cached by-name
//      resolver used by every feature hook.
// =============================================================================

#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"

#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <set>
#include <vector>
#include <algorithm>
#include <elf.h>
#include <android/log.h>

#define IL2CPP_SO "libil2cpp.so"

// ============================================================
// il2cppExports — definitions
// ============================================================
namespace il2cppExports {
    void*       (*il2cpp_domain_get)() = nullptr;
    void*       (*il2cpp_thread_attach)(void*) = nullptr;
    int         (*il2cpp_is_vm_thread)(void*) = nullptr;
    void**      (*il2cpp_domain_get_assemblies)(void*, size_t*) = nullptr;
    void*       (*il2cpp_assembly_get_image)(void*) = nullptr;
    const char* (*il2cpp_image_get_name)(void*) = nullptr;
    void*       (*il2cpp_class_from_name)(void*, const char*, const char*) = nullptr;
    void*       (*il2cpp_class_get_method_from_name)(void*, const char*, int) = nullptr;
    void*       (*il2cpp_class_get_field_from_name)(void*, const char*) = nullptr;
    size_t      (*il2cpp_field_get_offset)(void*) = nullptr;
    void*       (*il2cpp_string_new)(const char*) = nullptr;
    void*       (*il2cpp_runtime_invoke)(void*, void*, void**, void**) = nullptr;
}

// ============================================================
//  ARM64 BL-histogram scanner (ScanIl2cppApi)
// ============================================================
namespace {

struct Region {
    uintptr_t start = 0, end = 0, off = 0;
    bool read = false, exec = false;
};
struct Module {
    uintptr_t base = 0, loadBias = 0;
    uintptr_t textLo = 0, textHi = 0;
    bool hasText = false;
    std::vector<Region> regions;
    const Region* findReadable(uintptr_t va) const {
        for (const auto& r : regions)
            if (r.read && va >= r.start && va < r.end) return &r;
        return nullptr;
    }
    bool inText(uintptr_t va) const {
        return hasText ? (va >= textLo && va < textHi) : (findReadable(va) != nullptr);
    }
};

bool readMem(const Module& m, uintptr_t va, void* out, size_t n) {
    uintptr_t cur = va; size_t done = 0;
    while (done < n) {
        const Region* r = m.findReadable(cur);
        if (!r) return false;
        size_t avail = r->end - cur;
        size_t chunk = std::min(n - done, avail);
        memcpy((char*)out + done, (const void*)cur, chunk);
        done += chunk; cur += chunk;
    }
    return true;
}
inline bool read32(const Module& m, uintptr_t va, uint32_t* out) { return readMem(m, va, out, 4); }
size_t readCStr(const Module& m, uintptr_t va, char* buf, size_t cap) {
    for (size_t i = 0; i + 1 < cap; ++i) {
        uint8_t c; if (!readMem(m, va + i, &c, 1)) return 0;
        buf[i] = (char)c; if (c == 0) return i;
    }
    buf[cap - 1] = 0; return 0;
}
bool isValidApiName(const char* s, size_t len) {
    if (len < 3 || len > 96) return false;
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_')) return false;
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// ARM64 instruction decoders
inline bool decodeBL(uintptr_t pc, uint32_t insn, uintptr_t* target) {
    if ((insn >> 26) != 0x25) return false;
    int32_t imm = (int32_t)(insn & 0x03FFFFFF);
    if (imm & 0x02000000) imm |= (int32_t)0xFC000000;
    *target = pc + ((int64_t)imm << 2);
    return true;
}
inline bool decodeADRP(uintptr_t pc, uint32_t insn, unsigned* rd, uint64_t* val) {
    if ((insn & 0x9F000000) != 0x90000000) return false;
    *rd = insn & 0x1F;
    uint32_t imm = ((insn >> 3) & 0x1FFFFC) | ((insn >> 29) & 3);
    if (insn & 0x00800000) imm |= 0xFFF00000;
    *val = (pc & ~(uint64_t)0xFFF) + ((int64_t)(int32_t)imm * 0x1000);
    return true;
}
inline bool decodeADD(uint32_t insn, unsigned* rd, unsigned* rn, uint64_t* imm) {
    if ((insn >> 23) != 0x122) return false;
    unsigned shift = (insn & 0x00400000) ? 12 : 0;
    *rd = insn & 0x1F; *rn = (insn >> 5) & 0x1F;
    *imm = (uint64_t)((insn >> 10) & 0xFFF) << shift;
    return true;
}
inline bool decodeADR(uintptr_t pc, uint32_t insn, unsigned* rd, uint64_t* val) {
    if ((insn & 0x9F000000) != 0x10000000) return false;
    uint32_t imm = ((insn >> 3) & 0x1FFFFC) | ((insn >> 29) & 3);
    if (insn & 0x00800000) imm |= 0xFFF00000;
    *rd = insn & 0x1F; *val = pc + (int64_t)(int32_t)imm;
    return true;
}
inline bool isCfInsn(uint32_t insn) {
    if ((insn & 0x7C000000) == 0x14000000) return true;
    if ((insn & 0xFF000010) == 0x54000000) return true;
    if (((insn & 0x7E000000) | 0x02000000) == 0x36000000) return true;
    if ((insn & 0xFE1FFC1F) == 0xD61F0000) return true;
    if ((insn & 0xFFFFFC1F) == 0xD65F0000) return true;
    return false;
}

bool readModule(const char* soName, Module& m) {
    FILE* fp = fopen("/proc/self/maps", "re"); if (!fp) return false;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        const char* path = strchr(line, '/');
        if (!path || !strstr(path, soName)) continue;
        uintptr_t s = 0, e = 0, off = 0; char perms[8] = {};
        if (sscanf(line, "%zx-%zx %4s %zx", &s, &e, perms, &off) != 4) continue;
        Region r; r.start = s; r.end = e; r.off = off;
        r.read = perms[0] == 'r'; r.exec = perms[2] == 'x';
        m.regions.push_back(r);
    }
    fclose(fp);
    if (m.regions.empty()) return false;

    auto isElfHdr = [&](uintptr_t start) {
        uint8_t magic[4];
        return readMem(m, start, magic, 4) &&
               magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F';
    };
    uintptr_t minExec = 0;
    for (const auto& r : m.regions) if (r.exec && (!minExec || r.start < minExec)) minExec = r.start;
    m.base = 0;
    if (minExec) {
        uintptr_t bestBelow = 0;
        for (const auto& r : m.regions)
            if (r.off == 0 && r.read && r.start <= minExec && r.start > bestBelow && isElfHdr(r.start))
                bestBelow = r.start;
        m.base = bestBelow;
    }
    if (!m.base)
        for (const auto& r : m.regions) if (r.off == 0 && r.read && isElfHdr(r.start)) { m.base = r.start; break; }
    if (!m.base) m.base = m.regions.front().start;
    return true;
}

void parseElfText(Module& m) {
    Elf64_Ehdr eh;
    if (!readMem(m, m.base, &eh, sizeof(eh))) return;
    if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0 || eh.e_ident[EI_CLASS] != ELFCLASS64) return;
    uintptr_t minVaddr = UINTPTR_MAX;
    for (int i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        if (!readMem(m, m.base + eh.e_phoff + (size_t)i * eh.e_phentsize, &ph, sizeof(ph))) continue;
        if (ph.p_type == PT_LOAD && ph.p_vaddr < minVaddr) minVaddr = ph.p_vaddr;
    }
    if (minVaddr == UINTPTR_MAX) minVaddr = 0;
    m.loadBias = m.base - minVaddr;
    uintptr_t txLo = UINTPTR_MAX, txHi = 0;
    for (int i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        if (!readMem(m, m.base + eh.e_phoff + (size_t)i * eh.e_phentsize, &ph, sizeof(ph))) continue;
        if (ph.p_type == PT_LOAD && (ph.p_flags & PF_X)) {
            uintptr_t lo = ph.p_vaddr + m.loadBias, hi = lo + ph.p_memsz;
            if (lo < txLo) txLo = lo; if (hi > txHi) txHi = hi;
        }
    }
    if (txHi > txLo) { m.textLo = txLo; m.textHi = txHi; m.hasText = true; }
}

std::vector<uintptr_t> buildThunkCandidates(const Module& m) {
    std::map<uintptr_t, int> hist;
    for (const auto& r : m.regions) {
        if (!r.read) continue;
        uintptr_t lo = r.start, hi = r.end;
        if (m.hasText) { if (hi <= m.textLo || lo >= m.textHi) continue; if (lo < m.textLo) lo = m.textLo; if (hi > m.textHi) hi = m.textHi; }
        lo = (lo + 3) & ~(uintptr_t)3;
        for (uintptr_t pc = lo; pc + 4 <= hi; pc += 4) {
            uintptr_t tgt;
            if (decodeBL(pc, *(const uint32_t*)pc, &tgt)) hist[tgt]++;
        }
    }
    std::vector<std::pair<long, uintptr_t>> cands;
    for (const auto& kv : hist) {
        if (!m.inText(kv.first) || kv.second < 0x3c) continue;
        long diff = (long)kv.second - 0xf8; if (diff < 0) diff = -diff;
        cands.emplace_back((long)kv.second - 100 * diff, kv.first);
    }
    std::sort(cands.begin(), cands.end(), [](const std::pair<long,uintptr_t>& a, const std::pair<long,uintptr_t>& b){ return a.first > b.first; });
    std::vector<uintptr_t> out;
    for (size_t i = 0; i < cands.size() && i < 16; ++i) out.push_back(cands[i].second);
    return out;
}

size_t registrationPass2(const Module& m, uintptr_t thunkVA, std::map<std::string, uintptr_t>& out) {
    const uintptr_t MAXBACK = 0x100;
    char nameBuf[128];
    for (const auto& r : m.regions) {
        if (!r.read) continue;
        uintptr_t lo = r.start, hi = r.end;
        if (m.hasText) { if (hi <= m.textLo || lo >= m.textHi) continue; if (lo < m.textLo) lo = m.textLo; if (hi > m.textHi) hi = m.textHi; }
        lo = (lo + 3) & ~(uintptr_t)3;
        for (uintptr_t pc = lo; pc + 4 <= hi; pc += 4) {
            uintptr_t tgt;
            if (!decodeBL(pc, *(const uint32_t*)pc, &tgt) || tgt != thunkVA) continue;
            uintptr_t floor = (pc > lo + MAXBACK) ? (pc - MAXBACK) : lo;
            uintptr_t blockStart = pc;
            for (uintptr_t p = pc; p > floor; ) {
                p -= 4; uint32_t pi;
                if (!read32(m, p, &pi)) { blockStart = p + 4; break; }
                if (isCfInsn(pi)) { blockStart = p + 4; break; }
                blockStart = p;
            }
            uint64_t regs[32] = {}; bool val[32] = {};
            for (uintptr_t ip = blockStart; ip < pc; ip += 4) {
                uint32_t ci; if (!read32(m, ip, &ci)) continue;
                unsigned rd, rn; uint64_t imm;
                if (decodeADRP(ip, ci, &rd, &imm)) { regs[rd] = imm; val[rd] = true; }
                else if (decodeADD(ci, &rd, &rn, &imm)) { if (rd == rn && val[rd]) regs[rd] += imm; }
                else if (decodeADR(ip, ci, &rd, &imm)) { regs[rd] = imm; val[rd] = true; }
                else if ((ci & 0xFFE0FFE0) == 0xAA0003E0) { unsigned xd = ci & 0x1F, xm = (ci >> 16) & 0x1F; regs[xd] = regs[xm]; val[xd] = val[xm]; }
            }
            const char* foundName = nullptr; uintptr_t fnVA = 0;
            char nb0[128];
            if (val[0]) { size_t l = readCStr(m, (uintptr_t)regs[0], nb0, sizeof(nb0)); if (l && isValidApiName(nb0, l)) foundName = nb0; }
            if (val[1] && (regs[1] & 3) == 0 && regs[1] != thunkVA && m.inText((uintptr_t)regs[1])) fnVA = (uintptr_t)regs[1];
            if (!foundName || !fnVA) {
                for (int rr = 0; rr < 32; ++rr) {
                    if (!val[rr]) continue; uint64_t v = regs[rr];
                    if (!foundName) { size_t l = readCStr(m, (uintptr_t)v, nameBuf, sizeof(nameBuf)); if (l && isValidApiName(nameBuf, l)) { foundName = nameBuf; continue; } }
                    if (!fnVA && (v & 3) == 0 && v != thunkVA && m.inText((uintptr_t)v)) fnVA = (uintptr_t)v;
                }
            }
            if (fnVA) { uint64_t slot = 0; if (readMem(m, fnVA, &slot, 8) && slot >= m.base && (slot & 3) == 0 && m.inText((uintptr_t)slot)) fnVA = (uintptr_t)slot; }
            if (foundName && fnVA) out.emplace(std::string(foundName), fnVA);
        }
    }
    return out.size();
}

int applyExports(const std::map<std::string, uintptr_t>& api) {
    using namespace il2cppExports;
    int n = 0;
    auto get = [&](const char* nm) -> uintptr_t { auto it = api.find(nm); return it == api.end() ? 0 : it->second; };
#define BIND(field, nm) do { uintptr_t a = get(nm); if (a) { field = reinterpret_cast<decltype(field)>(a); ++n; } } while(0)
    BIND(il2cpp_domain_get,                "il2cpp_domain_get");
    BIND(il2cpp_thread_attach,             "il2cpp_thread_attach");
    BIND(il2cpp_is_vm_thread,              "il2cpp_is_vm_thread");
    BIND(il2cpp_domain_get_assemblies,     "il2cpp_domain_get_assemblies");
    BIND(il2cpp_assembly_get_image,        "il2cpp_assembly_get_image");
    BIND(il2cpp_image_get_name,            "il2cpp_image_get_name");
    BIND(il2cpp_class_from_name,           "il2cpp_class_from_name");
    BIND(il2cpp_class_get_method_from_name,"il2cpp_class_get_method_from_name");
    BIND(il2cpp_class_get_field_from_name, "il2cpp_class_get_field_from_name");
    BIND(il2cpp_field_get_offset,          "il2cpp_field_get_offset");
    BIND(il2cpp_string_new,                "il2cpp_string_new");
    BIND(il2cpp_runtime_invoke,            "il2cpp_runtime_invoke");
#undef BIND
    return n;
}

} // anonymous namespace

// ============================================================
//  Public: ScanIl2cppApi
// ============================================================
bool ScanIl2cppApi() {
    Module m;
    if (!readModule(IL2CPP_SO, m)) { LOGE("[il2cpp] libil2cpp.so not in maps"); return false; }
    parseElfText(m);

    std::map<std::string, uintptr_t> api;
    bool needScan = !api.count("il2cpp_class_from_name") || !api.count("il2cpp_domain_get");
    if (needScan) {
        std::vector<uintptr_t> cands = buildThunkCandidates(m);
        std::map<std::string, uintptr_t> best; uintptr_t bestThunk = 0;
        for (uintptr_t thunk : cands) {
            std::map<std::string, uintptr_t> tmp;
            registrationPass2(m, thunk, tmp);
            if (tmp.size() > best.size()) { best.swap(tmp); bestThunk = thunk; }
            if (best.count("il2cpp_class_from_name") && best.count("il2cpp_domain_get") && best.size() > 100) break;
        }
        LOGI("[il2cpp] scanner thunk=0x%llx pairs=%zu", (unsigned long long)bestThunk, best.size());
        for (auto& kv : best) api[kv.first] = kv.second;
    }

    int bound = applyExports(api);
    bool ok = (il2cppExports::il2cpp_class_from_name != nullptr) && (il2cppExports::il2cpp_domain_get != nullptr);
    LOGI("[il2cpp] ScanIl2cppApi: bound=%d core_ok=%d", bound, (int)ok);
    return ok;
}

// ============================================================
//  Public: Init_Il2cpp_Symbol
// ============================================================
void Init_Il2cpp_Symbol() {
    using namespace il2cppExports;

    // This game STRIPS il2cpp_* from .dynsym → dlsym fails → use scanner.
    if (!il2cpp_class_from_name || !il2cpp_domain_get) {
        for (int i = 0; i < 60 && (!il2cpp_class_from_name || !il2cpp_domain_get); i++) {
            if (ScanIl2cppApi()) break;
            sleep(1);
        }
    }
    LOGI("[il2cpp] API: domain_get=%p class_from_name=%p", (void*)il2cpp_domain_get, (void*)il2cpp_class_from_name);

    // Runtime-ready gate: block until il2cpp runtime is up.
    if (il2cpp_is_vm_thread) {
        for (int i = 0; i < 900; i++) {
            if (il2cpp_is_vm_thread(nullptr) & 1) break;
            if ((i % 10) == 0) LOGI("[il2cpp] waiting for runtime...");
            sleep(1);
        }
        LOGI("[il2cpp] runtime ready");
    }

    IL2Cpp::EnsureApiHealthy(40);
}

// ============================================================
//  IL2Cpp namespace — by-name resolver
// ============================================================
namespace IL2Cpp {

std::map<std::string, uintptr_t> LOOP;

// Safety: only hook addresses inside libil2cpp.so
static bool Il2cppModuleRange(uintptr_t& lo, uintptr_t& hi) {
    static uintptr_t s_lo = 0, s_hi = 0; static bool s_done = false, s_tried = false;
    if (!s_tried) {
        FILE* fp = fopen("/proc/self/maps", "re");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                if (!strstr(line, "libil2cpp.so")) continue;
                uintptr_t a = 0, b = 0;
                if (sscanf(line, "%zx-%zx", &a, &b) != 2) continue;
                if (!s_lo || a < s_lo) s_lo = a;
                if (b > s_hi) s_hi = b;
            }
            fclose(fp);
        }
        s_done = (s_lo && s_hi); s_tried = true;
    }
    lo = s_lo; hi = s_hi; return s_done;
}

static bool IsArm64HookAddr(uintptr_t p) {
    uintptr_t lo = 0, hi = 0;
    if (!Il2cppModuleRange(lo, hi)) return true;
    return p >= lo && p < hi;
}

void* Il2CppGetImageByName(const char* dll) {
    using namespace il2cppExports;
    if (!il2cpp_domain_get) return nullptr;
    void* dom = il2cpp_domain_get(); if (!dom) return nullptr;
    static bool s_attached = false;
    if (!s_attached && il2cpp_thread_attach) { il2cpp_thread_attach(dom); s_attached = true; }
    size_t n = 0; void** assemblies = il2cpp_domain_get_assemblies(dom, &n);
    for (size_t i = 0; i < n; i++) {
        void* img = il2cpp_assembly_get_image(assemblies[i]);
        const char* name = il2cpp_image_get_name(img);
        if (name && strcmp(name, dll) == 0) return img;
    }
    return nullptr;
}

bool Il2cppReady() {
    using namespace il2cppExports;
    if (!il2cpp_domain_get || !il2cpp_class_from_name) return false;
    void* dom = il2cpp_domain_get(); if (!dom) return false;
    return Il2CppGetImageByName("Project_d.dll") != nullptr;
}

static bool ApiResolvesKnownClass() {
    using namespace il2cppExports;
    if (!il2cpp_domain_get || !il2cpp_class_from_name || !il2cpp_class_get_method_from_name ||
        !il2cpp_domain_get_assemblies || !il2cpp_assembly_get_image || !il2cpp_image_get_name) return false;
    void* dom = il2cpp_domain_get(); if (!dom) return false;
    if (il2cpp_thread_attach) il2cpp_thread_attach(dom);
    struct P { const char* dll; const char* ns; const char* cls; const char* m; int argc; };
    static const P probes[] = {
        { "mscorlib.dll",  "System",        "Object",         ".ctor",       0 },
        { DLL_MAIN, NS_ACTOR,  "ActorLinker",    "get_position",    0 },
        { DLL_MAIN, NS_SYSTEM, "CBattleEquipSystem", "get_HostEquipComp", 0 },
    };
    for (const auto& p : probes) {
        void* img = Il2CppGetImageByName(p.dll); if (!img) continue;
        void* cls = il2cpp_class_from_name(img, p.ns, p.cls); if (!cls) continue;
        void* mth = il2cpp_class_get_method_from_name(cls, p.m, p.argc);
        if (mth && *(void**)mth) return true;
    }
    return false;
}

bool EnsureApiHealthy(int maxAttempts) {
    using namespace il2cppExports;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        if (ApiResolvesKnownClass()) {
            if (attempt) LOGI("[il2cpp] API verified after %d re-scan(s)", attempt);
            return true;
        }
        LOGW("[il2cpp] API does not resolve known class -> re-scan (attempt %d)", attempt);
        il2cpp_domain_get = nullptr; il2cpp_thread_attach = nullptr; il2cpp_is_vm_thread = nullptr;
        il2cpp_domain_get_assemblies = nullptr; il2cpp_assembly_get_image = nullptr;
        il2cpp_image_get_name = nullptr; il2cpp_class_from_name = nullptr;
        il2cpp_class_get_method_from_name = nullptr; il2cpp_class_get_field_from_name = nullptr;
        il2cpp_field_get_offset = nullptr; il2cpp_string_new = nullptr;
        LOOP.clear();
        ScanIl2cppApi();
        if (attempt + 1 < maxAttempts) sleep(1);
    }
    LOGW("[il2cpp] API verify exhausted; proceeding best-effort");
    return ApiResolvesKnownClass();
}

uintptr_t Il2CppGetMethodOffset(const char* dll, const char* ns, const char* klass,
                                 const char* method, int argc, int token) {
    using namespace il2cppExports;
    std::string key = std::string(dll) + ":" + ns + ":" + klass + ":" + method + ":" +
                      std::to_string(argc) + ":" + std::to_string(token);
    auto it = LOOP.find(key);
    if (it != LOOP.end()) return it->second;

    uintptr_t out = 0;
    void* img = Il2CppGetImageByName(dll);
    void* cls = img ? il2cpp_class_from_name(img, ns, klass) : nullptr;
    if (cls) {
        void* m = il2cpp_class_get_method_from_name(cls, method, argc);
        if (m) out = (uintptr_t)*(void**)m;
        if (out && !IsArm64HookAddr(out)) { LOGW("[il2cpp] %s::%s methodPtr not in arm64 range", klass, method); return 0; }
        if (!m) LOGE("[il2cpp] method not found %s::%s (%d)", klass, method, argc);
    } else {
        LOGE("[il2cpp] class not found %s::%s::%s", dll, ns, klass);
    }
    if (out) LOOP[key] = out;
    return out;
}

uintptr_t Il2CppGetFieldOffset(const char* dll, const char* ns, const char* klass, const char* field) {
    using namespace il2cppExports;
    std::string key = std::string("F:") + dll + ":" + ns + ":" + klass + ":" + field;
    auto it = LOOP.find(key);
    if (it != LOOP.end()) return it->second;

    uintptr_t out = 0;
    void* img = Il2CppGetImageByName(dll);
    void* cls = img ? il2cpp_class_from_name(img, ns, klass) : nullptr;
    if (cls) {
        void* f = il2cpp_class_get_field_from_name(cls, field);
        if (f) out = il2cpp_field_get_offset(f);
        else LOGE("[il2cpp] field not found %s::%s", klass, field);
    }
    if (out) LOOP[key] = out;
    return out;
}

} // namespace IL2Cpp
