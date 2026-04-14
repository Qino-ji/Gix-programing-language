#include "ir.h"
#include "import.h"
#include "register.h"


bool range_eq(SourceRange r, const char* str); // You'll Find this function, in the parser.
RegisterEntry* register_get(Register* reg, StringView name);

// define the str type.
IR_Type str_type(void) {
    static IR_Type char_type = { .tag = IR_Type_Char };
    static IR_Type fields[2] = {
        { .tag = IR_Type_Ptr, .data.ptr.inner = &char_type },  // pointer to static — OK
        { .tag = IR_Type_I64 },
    };
    return (IR_Type){ .tag = IR_Type_Struct, .data.named.name_id = 0 };
}


IR_Type type(const Type* t, Register* reg) {
    if (!t) return (IR_Type){ .tag = IR_Type_Void };

    switch (t->tag) {
        case Type_Int:
            switch (t->data.int_t.bits) {
                case 8:  return (IR_Type){ .tag = IR_Type_I8  };
                case 16: return (IR_Type){ .tag = IR_Type_I16 };
                case 32: return (IR_Type){ .tag = IR_Type_I32 };
                case 64: return (IR_Type){ .tag = IR_Type_I64 };
                default: return (IR_Type){ .tag = IR_Type_I32 };
            }

        case Type_Float:
            switch (t->data.float_t.bits) {
                case 32: return (IR_Type){ .tag = IR_Type_F32 };
                case 64: return (IR_Type){ .tag = IR_Type_F64 };
                default: return (IR_Type){ .tag = IR_Type_F64 };
            }

        case Type_Char: return (IR_Type){ .tag = IR_Type_Char };
        case Type_Str:  return str_type();
        case Type_Bool: return (IR_Type){ .tag = IR_Type_Bool };
        case Type_Void: return (IR_Type){ .tag = IR_Type_Void };

        case Type_Ptr: {
            IR_Type* inner = malloc(sizeof(IR_Type));
            *inner = type(t->data.ptr.inner, reg);
            return (IR_Type){ .tag = IR_Type_Ptr, .data.ptr.inner = inner };
        }

        case Type_RawPtr: {
            IR_Type* inner = malloc(sizeof(IR_Type));
            *inner = type(t->data.raw_ptr.inner, reg);

            return (IR_Type){ .tag = IR_Type_Ptr, .data.ptr.inner = inner };
        }

        case Type_Array: {
            IR_Type* inner = malloc(sizeof(IR_Type));
            *inner = type(t->data.array_t.inner, reg);
    
            return (IR_Type){ .tag = IR_Type_Array, .data.array.inner = inner, .data.array.len = t->data.array_t.len };
        }

        case Type_FnPtr: { return (IR_Type){ .tag = IR_Type_Ptr, .data.ptr.inner = &(IR_Type){ .tag = IR_Type_Void } }; }

        case Type_Custom: {
            StringView key = {
                .ptr = t->data.custom.name.start,
                .len = (size_t)(t->data.custom.name.end - t->data.custom.name.start)
            };

            RegisterEntry* entry = register_get(reg, key);
            if (entry) return (IR_Type){ .tag = IR_Type_Struct, .data.named.name_id = entry->eid.id };
            return (IR_Type){ .tag = IR_Type_Void };
        }
    }

    return (IR_Type){ .tag = IR_Type_Void };
}