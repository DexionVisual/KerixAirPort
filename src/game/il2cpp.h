#pragma once


#include <windows.h>
#include <cstdint>

struct Il2CppDomain;
struct Il2CppThread;
struct Il2CppAssembly;
struct Il2CppImage;
struct Il2CppClass;
struct Il2CppFieldInfo;
struct MethodInfo;
struct Il2CppType;
struct Il2CppObject;
struct Il2CppString;
struct Il2CppReflectionType;

namespace il2cpp {

using InvokeFn = Il2CppObject* (*)(MethodInfo*, void*, void**, Il2CppObject**);

struct Api {
    Il2CppDomain* (*domain_get)();
    Il2CppThread* (*thread_current)();
    Il2CppThread* (*thread_attach)(Il2CppDomain*);
    int           (*thread_detach)(Il2CppThread*);

    Il2CppAssembly** (*domain_get_assemblies)(Il2CppDomain*, size_t*);
    const char*      (*image_get_name)(Il2CppImage*);
    Il2CppImage*     (*assembly_get_image)(Il2CppAssembly*);

    Il2CppClass*  (*class_from_name)(Il2CppImage*, const char*, const char*);
    MethodInfo*   (*class_get_method_from_name)(Il2CppClass*, const char*, int);
    Il2CppFieldInfo* (*class_get_field_from_name)(Il2CppClass*, const char*);
    size_t        (*field_get_offset)(Il2CppFieldInfo*);
    const char*   (*class_get_name)(Il2CppClass*);
    const char*   (*class_get_namespace)(Il2CppClass*);
    const Il2CppType* (*class_get_type)(Il2CppClass*);
    Il2CppReflectionType* (*type_get_object)(const Il2CppType*);
    void          (*runtime_class_init)(Il2CppClass*);
    void*         (*field_static_get_value)(Il2CppFieldInfo*, void*);
    void          (*field_get_value)(Il2CppObject*, Il2CppFieldInfo*, void*);

    InvokeFn      runtime_invoke;
    Il2CppObject* (*object_new)(Il2CppClass*);

    Il2CppString* (*string_new)(const char*);
    const char*   (*string_chars)(Il2CppString*);
};

inline Api api{};
inline HMODULE module = nullptr;


struct ObjectHeader {
    void* klass;
    void* monitor;
};
struct ArrayHeader : ObjectHeader {
    void* bounds;
    uintptr_t max_length;

};
struct ListHeader : ObjectHeader {
    ArrayHeader* items;
    int32_t      size;
};

inline bool init() {
    for (int i = 0; i < 300; ++i) {
        module = GetModuleHandleA("GameAssembly.dll");
        if (module) break;
        Sleep(200);
    }
    if (!module) return false;

#define R(n) api.n = reinterpret_cast<decltype(api.n)>(GetProcAddress(module, "il2cpp_" #n))
    R(domain_get); R(thread_current); R(thread_attach); R(thread_detach);
    R(domain_get_assemblies); R(image_get_name); R(assembly_get_image);
    R(class_from_name); R(class_get_method_from_name); R(class_get_field_from_name);
    R(field_get_offset); R(class_get_name); R(class_get_namespace); R(class_get_type);
    R(type_get_object); R(runtime_class_init); R(field_static_get_value); R(field_get_value);
    R(runtime_invoke); R(object_new); R(string_new); R(string_chars);
#undef R
    return api.domain_get && api.thread_attach && api.runtime_invoke && api.class_from_name;
}

}
