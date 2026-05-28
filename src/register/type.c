#include "ir.h"
#include "import.h"
#include "register.h"

RegisterEntry* register_get(Register* reg, StringView name);

static StringView sv_from_range(SourceRange r) {
    return (StringView){ .ptr = r.start, .len = (size_t)(r.end - r.start) };
}

static Type* type_heap_copy(Type t) {
    Type* p = malloc(sizeof(Type));
    *p = t;
    return p;
}

Type ir_type(const Type* t, Register* reg) {
    if (!t) {
        printf("[DEBUG] Received NULL type, defaulting to Void\n");
        return (Type){ .tag = Type_Void };
    }

    // Safety check for the garbage value you saw
    if (t->tag < 0 || t->tag > 100) {
        printf("[CRITICAL] Corrupted Type at %p | Tag: %d\n", (void*)t, t->tag);
        // This is usually where the crash happens.
    }

    // Log the incoming tag
    printf("[DEBUG] Resolving Type Tag: %d\n", t->tag);

    switch (t->tag) {
        case Type_Custom: {
            StringView name_sv = sv_from_range(t->data.custom.name);
            printf("[DEBUG] Resolving Custom: %.*s\n", (int)name_sv.len, name_sv.ptr);

            // empty custom = unresolved type, return void instead of crashing
            if (name_sv.len == 0) {
                fprintf(stderr, "[WARN] Empty custom type name, defaulting to Void\n");
                return (Type){ .tag = Type_Void };
            }

            RegisterEntry* e = register_get(reg, name_sv);
            if (!e) {
                fprintf(stderr, "[ERROR] Symbol '%.*s' not found in register!\n",
                    (int)name_sv.len, name_sv.ptr);
                CG_UNREACHABLE_MSG("type could not be resolved");
            }
            return e->type;
        }
        case Type_Ptr: {
            printf("[DEBUG] Descending into Pointer\n");
            Type inner = ir_type(t->data.ptr.inner, reg);
            return (Type){ .tag = Type_Ptr, .data.ptr.inner = type_heap_copy(inner) };
        }

        case Type_RawPtr: {
            printf("[DEBUG] Descending into Raw Pointer\n");
            Type inner = ir_type(t->data.raw_ptr.inner, reg);
            return (Type){ .tag = Type_RawPtr, .data.raw_ptr.inner = type_heap_copy(inner) };
        }

        case Type_Array: {
            printf("[DEBUG] Descending into Array (len: %zu)\n", t->data.array_t.len);
            Type inner = ir_type(t->data.array_t.inner, reg);
            return (Type){
                .tag = Type_Array,
                .data.array_t = {
                    .inner = type_heap_copy(inner),
                    .len   = t->data.array_t.len,
                },
            };
        }

        case Type_FnPtr: {
            printf("[DEBUG] Resolving Function Pointer (%zu params)\n", t->data.fn_ptr.params_count);
            Type ret = ir_type(t->data.fn_ptr.ret, reg);

            size_t pc = t->data.fn_ptr.params_count;
            Type* params = pc ? malloc(pc * sizeof(Type)) : NULL;
            for (size_t i = 0; i < pc; i++) {
                params[i] = ir_type(&t->data.fn_ptr.params[i], reg);
            }

            return (Type){
                .tag = Type_FnPtr,
                .data.fn_ptr = {
                    .ret          = type_heap_copy(ret),
                    .params       = params,
                    .params_count = pc,
                },
            };
        }

        case Type_Tuple: {
            printf("[DEBUG] Resolving Tuple (%zu elems)\n", t->data.tuple.elems_count);
            size_t ec = t->data.tuple.elems_count;
            Type* elems = ec ? malloc(ec * sizeof(Type)) : NULL;
            for (size_t i = 0; i < ec; i++)
                elems[i] = ir_type(&t->data.tuple.elems[i], reg);
            return (Type){
                .tag = Type_Tuple,
                .data.tuple = { .elems = elems, .elems_count = ec },
            };
        }

        // Primitives: These are "leaf nodes" in your type tree
        case Type_Int:   printf("[DEBUG] Leaf: Int\n");   return *t;
        case Type_Float: printf("[DEBUG] Leaf: Float\n"); return *t;
        case Type_Bool:  printf("[DEBUG] Leaf: Bool\n");  return *t;
        case Type_Char:  printf("[DEBUG] Leaf: Char\n");  return *t;
        case Type_Str:   printf("[DEBUG] Leaf: Str\n");   return *t;
        case Type_Void:  printf("[DEBUG] Leaf: Void\n");  return *t;

        default:
            fprintf(stderr, "[ERROR] Unknown Type Tag encountered: %d\n", t->tag);
            return *t;
    }
}