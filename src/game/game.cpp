#include "game.h"
#include "game/il2cpp.h"

using namespace il2cpp;

namespace game {

namespace {

struct FieldRef {
    Il2CppFieldInfo* info = nullptr;
    int32_t          fallback = 0;
    int32_t offset() const {
        return info ? (int32_t)api.field_get_offset(info) : fallback;
    }
};

struct Refs {

    Il2CppImage* imgGame    = nullptr;
    Il2CppImage* imgCore    = nullptr;


    Il2CppClass* clsMetaPlayer = nullptr;
    Il2CppClass* clsModeMgr    = nullptr;
    Il2CppClass* clsPlayerName = nullptr;
    Il2CppClass* clsPlayerHp   = nullptr;
    Il2CppClass* clsGameObject = nullptr;
    Il2CppClass* clsCamera     = nullptr;
    Il2CppClass* clsCapsule    = nullptr;


    MethodInfo* mLocalInstance  = nullptr;
    MethodInfo* mCamMain        = nullptr;
    MethodInfo* mCamAll         = nullptr;
    MethodInfo* mCamW2S         = nullptr;
    MethodInfo* mGetTransform   = nullptr;
    MethodInfo* mGetPosition    = nullptr;
    MethodInfo* mGetHeight      = nullptr;
    MethodInfo* mGetComponent   = nullptr;


    FieldRef fModeMgr;
    FieldRef fCapsule;
    FieldRef fIsAgent;
    FieldRef fIsCopDisg;
    FieldRef fPmm;
    FieldRef fPlayerName;
    FieldRef fHpCur;
    FieldRef fHpMax;


    Il2CppFieldInfo* sfInstances  = nullptr;
    Il2CppFieldInfo* sfPnInstances = nullptr;

    Il2CppReflectionType* hpType = nullptr;
};

Refs   g;
bool   g_ready      = false;
int64_t g_failUntil = 0;
int64_t g_freq      = 1;


void ensure_thread_attached() {
    if (api.thread_current && api.thread_current()) return;
    Il2CppDomain* dom = api.domain_get();
    if (dom && api.thread_attach) api.thread_attach(dom);
}

template <typename T>
T* rd(void* obj, const FieldRef& f) {
    if (!obj) return nullptr;
    return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(obj) + f.offset());
}

float rdf(void* obj, const FieldRef& f, float def) {
    if (!obj) return def;
    return *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(obj) + f.offset());
}

bool rdfb(void* obj, const FieldRef& f) {
    if (!obj) return false;
    return *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(obj) + f.offset());
}


Il2CppObject* safe_invoke(MethodInfo* m, void* inst, void** args) {
    if (!m) return nullptr;
    Il2CppObject* exc = nullptr;
    Il2CppObject* ret = api.runtime_invoke(m, inst, args, &exc);
    return exc ? nullptr : ret;
}

bool read_string(void* strObj, char* dst, size_t cap) {
    if (!strObj) return false;
    auto* s = reinterpret_cast<uint8_t*>(strObj);
    int32_t len = *reinterpret_cast<int32_t*>(s + 0x10);
    if (len <= 0) return false;
    if (len > (int32_t)cap - 1) len = (int32_t)cap - 1;
    auto* chars = reinterpret_cast<const wchar_t*>(s + 0x14);
    int n = WideCharToMultiByte(CP_UTF8, 0, chars, len, dst, (int)cap - 1, nullptr, nullptr);
    if (n <= 0) return false;
    dst[n] = 0;
    return true;
}

bool list_elem(void* list, int32_t idx, void** out) {
    if (!list) return false;
    auto* l = reinterpret_cast<ListHeader*>(list);
    if (idx >= l->size) return false;
    auto* arr = l->items;
    if (!arr || idx >= (int32_t)arr->max_length) return false;
    void** data = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(arr) + 0x20);
    *out = data[idx];
    return *out != nullptr;
}

Vec3 invoke_vec3(MethodInfo* m, void* inst, void* v3in) {
    Vec3 out{ 0, 0, 0 };
    if (!m) return out;
    void* args[1] = { v3in };
    Il2CppObject* ret = safe_invoke(m, inst, v3in ? args : nullptr);
    if (ret) out = *reinterpret_cast<Vec3*>(reinterpret_cast<uint8_t*>(ret) + 0x10);
    return out;
}

Il2CppImage* find_image(const char* name) {
    size_t n = 0;
    auto** assemblies = api.domain_get_assemblies(api.domain_get(), &n);
    if (!assemblies) return nullptr;
    for (size_t i = 0; i < n; ++i) {
        Il2CppImage* img = api.assembly_get_image(assemblies[i]);
        if (img && api.image_get_name(img)) {
            const char* in = api.image_get_name(img);
            if (!_stricmp(in, name)) return img;
        }
    }
    return nullptr;
}

template <typename T>
void bind_field(FieldRef& f, Il2CppClass* cls, const char* name, int32_t fallback) {
    f.info = api.class_get_field_from_name(cls, name);
    f.fallback = fallback;
    (void)sizeof(T);
}

}

bool ready() { return g_ready; }

void shutdown() {
    g_ready = false;
    g = Refs{};
}

bool try_init() {
    if (g_ready) return true;
    if (!api.domain_get) return false;
    Il2CppDomain* dom = api.domain_get();
    if (!dom) return false;
    ensure_thread_attached();

    g.imgGame = find_image("Assembly-CSharp.dll");
    g.imgCore = find_image("UnityEngine.CoreModule.dll");
    if (!g.imgGame || !g.imgCore) return false;

    g.clsMetaPlayer = api.class_from_name(g.imgGame, "Metater", "MetaPlayer");
    g.clsModeMgr    = api.class_from_name(g.imgGame, "", "PlayerModeManager");
    g.clsPlayerName = api.class_from_name(g.imgGame, "", "PlayerName");
    g.clsPlayerHp   = api.class_from_name(g.imgGame, "", "PlayerHealth");
    if (!g.clsMetaPlayer || !g.clsModeMgr) return false;

    g.clsGameObject = api.class_from_name(g.imgCore, "UnityEngine", "GameObject");
    g.clsCamera     = api.class_from_name(g.imgCore, "UnityEngine", "Camera");
    g.clsCapsule    = api.class_from_name(g.imgCore, "UnityEngine", "CapsuleCollider");
    if (!g.clsGameObject || !g.clsCamera) return false;

    g.mLocalInstance = api.class_get_method_from_name(g.clsMetaPlayer, "get_LocalPlayerInstance", 0);
    g.mCamMain       = api.class_get_method_from_name(g.clsCamera, "get_main", 0);
    g.mCamAll        = api.class_get_method_from_name(g.clsCamera, "get_allCameras", 0);
    g.mCamW2S        = api.class_get_method_from_name(g.clsCamera, "WorldToScreenPoint", 1);
    g.mGetTransform  = api.class_get_method_from_name(g.clsMetaPlayer, "get_transform", 0);
    if (Il2CppClass* clsTransform = api.class_from_name(g.imgCore, "UnityEngine", "Transform"))
        g.mGetPosition = api.class_get_method_from_name(clsTransform, "get_position", 0);
    g.mGetHeight     = g.clsCapsule ? api.class_get_method_from_name(g.clsCapsule, "get_height", 0) : nullptr;
    g.mGetComponent  = api.class_get_method_from_name(g.clsGameObject, "GetComponent", 1);

    bind_field<void*>(g.fModeMgr,   g.clsMetaPlayer, "modeManager",  0x150);
    bind_field<void*>(g.fCapsule,   g.clsMetaPlayer, "capsuleCollider", 0x98);
    bind_field<bool> (g.fIsAgent,   g.clsModeMgr,    "isAgent",      0x128);
    bind_field<bool> (g.fIsCopDisg, g.clsModeMgr,    "isCopDisguised", 0x129);
    if (g.clsPlayerName) {
        bind_field<void*>(g.fPmm,        g.clsPlayerName, "pmm",        0x138);
        bind_field<void*>(g.fPlayerName, g.clsPlayerName, "playerName", 0x68);
        g.sfPnInstances = api.class_get_field_from_name(g.clsPlayerName, "Instances");
    }
    if (g.clsPlayerHp) {
        bind_field<float>(g.fHpCur, g.clsPlayerHp, "currentHealth", 0x6C);
        bind_field<float>(g.fHpMax, g.clsPlayerHp, "maxHealth",     0x68);
        if (const Il2CppType* t = api.class_get_type(g.clsPlayerHp))
            g.hpType = api.type_get_object(t);
    }

    g.sfInstances = api.class_get_field_from_name(g.clsMetaPlayer, "Instances");
    if (!g.sfInstances) return false;

    api.runtime_class_init(g.clsMetaPlayer);
    api.runtime_class_init(g.clsModeMgr);
    if (g.clsPlayerName) api.runtime_class_init(g.clsPlayerName);

    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_freq = f.QuadPart;
    g_ready = true;
    return true;
}

namespace {


void* get_transform(void* component) {
    if (!component || !g.mGetTransform) return nullptr;
    return safe_invoke(g.mGetTransform, component, nullptr);
}

Vec3 get_position(void* transform) {
    if (!transform || !g.mGetPosition) return Vec3{ 0,0,0 };
    return invoke_vec3(g.mGetPosition, transform, nullptr);
}

void* get_component(void* gameObject, Il2CppReflectionType* type) {
    if (!gameObject || !g.mGetComponent || !type) return nullptr;
    void* args[1] = { type };
    return safe_invoke(g.mGetComponent, gameObject, args);
}

void* get_game_object(void* component) {

    static MethodInfo* m = nullptr;
    if (!m && g.imgCore) {
        Il2CppClass* comp = api.class_from_name(g.imgCore, "UnityEngine", "Component");
        if (comp) m = api.class_get_method_from_name(comp, "get_gameObject", 0);
    }
    if (!m) return nullptr;
    return safe_invoke(m, component, nullptr);
}

struct SnapshotCtx {
    std::vector<PlayerData>* out;
    float sw, sh;
};

void collect(SnapshotCtx& ctx) {
    void* local = nullptr;
    if (g.mLocalInstance) {
        Il2CppObject* r = safe_invoke(g.mLocalInstance, nullptr, nullptr);
        local = r;
    }


    void* cam = nullptr;
    if (g.mCamMain) {
        Il2CppObject* c = safe_invoke(g.mCamMain, nullptr, nullptr);
        if (c) cam = c;
    }
    if (!cam && g.mCamAll) {
        Il2CppObject* arrObj = safe_invoke(g.mCamAll, nullptr, nullptr);
        if (arrObj) {
            auto* arr = reinterpret_cast<ArrayHeader*>(arrObj);
            void** data = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(arrObj) + 0x20);
            for (uintptr_t i = 0; i < arr->max_length; ++i)
                if (data[i]) { cam = data[i]; break; }
        }
    }
    if (!cam) return;

    void* instList = nullptr;
    if (g.sfInstances)
        api.field_static_get_value(g.sfInstances, &instList);

    void* pnList = nullptr;
    if (g.sfPnInstances)
        api.field_static_get_value(g.sfPnInstances, &pnList);

    int32_t count = instList ? reinterpret_cast<ListHeader*>(instList)->size : 0;
    if (count <= 0 || count > 64) return;

    for (int32_t i = 0; i < count; ++i) {
        void* mp = nullptr;
        if (!list_elem(instList, i, &mp)) continue;

        PlayerData p;
        p.isLocal = (mp == local);
        if (p.isLocal) continue;

        void* pmm = rd<void*>(mp, g.fModeMgr);
        if (!pmm) continue;

        p.isAgent   = rdfb(pmm, g.fIsAgent);
        p.disguised = rdfb(pmm, g.fIsCopDisg);


        if (g.clsPlayerName && pnList) {
            int32_t pn = reinterpret_cast<ListHeader*>(pnList)->size;
            for (int32_t j = 0; j < pn; ++j) {
                void* pnObj = nullptr;
                if (!list_elem(pnList, j, &pnObj)) continue;
                if (rd<void*>(pnObj, g.fPmm) != pmm) continue;
                void* str = rd<void*>(pnObj, g.fPlayerName);
                read_string(str, p.name, sizeof(p.name));
                break;
            }
        }


        void* tr = get_transform(pmm);
        if (!tr) tr = get_transform(mp);
        Vec3 feet3 = get_position(tr);
        if (feet3.x == 0.0f && feet3.y == 0.0f && feet3.z == 0.0f) continue;

        float height = 1.85f;
        void* cap = rd<void*>(mp, g.fCapsule);
        if (cap && g.mGetHeight) {
            Il2CppObject* h = safe_invoke(g.mGetHeight, cap, nullptr);
            if (h) {
                float hv = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(h) + 0x10);
                if (hv > 0.5f && hv < 4.0f) height = hv + 0.15f;
            }
        }
        Vec3 head3{ feet3.x, feet3.y + height, feet3.z };


        Vec3 sf = invoke_vec3(g.mCamW2S, cam, &feet3);
        Vec3 shh = invoke_vec3(g.mCamW2S, cam, &head3);

        p.onScreen = sf.z > 0.1f && shh.z > 0.1f;
        p.feet[0] = sf.x;  p.feet[1] = ctx.sh - sf.y;
        p.head[0] = shh.x; p.head[1] = ctx.sh - shh.y;
        p.dist = sf.z > 0.0f ? sf.z : 0.0f;


        if (g.hpType) {
            void* go = get_game_object(mp);
            void* hpComp = get_component(go, g.hpType);
            if (hpComp) {
                p.hpMax = rdf(hpComp, g.fHpMax, 100.0f);
                p.hp    = rdf(hpComp, g.fHpCur, 0.0f);
                p.alive = p.hp > 0.0f;
            }
        }

        p.valid = true;
        ctx.out->push_back(p);
    }
}

}

void build_snapshot(std::vector<PlayerData>& out, float screenW, float screenH) {
    if (!g_ready) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (now.QuadPart < g_failUntil) return;

    ensure_thread_attached();

    out.clear();

    __try {
        SnapshotCtx ctx{ &out, screenW, screenH };
        collect(ctx);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_failUntil = now.QuadPart + g_freq;
    }
}

}
