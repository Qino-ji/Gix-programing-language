#ifndef REGISTER_H
#define REGISTER_H

#include "third-party/khashl.h"
#include "ast.h"
#include "import.h"
#include "error.h"

typedef struct Register Register;

typedef enum {
    Reg_Var, 
    Reg_Let, 
    Reg_Const, 
    Reg_Local,
    Reg_Function, 
    Reg_Class, 
    Reg_Struct, 
    Reg_Enum,
    Reg_Trait, 
    Reg_Extern,
    Reg_ExprFunctionCall, 
    Reg_ExprClassCall,
    Reg_ExprStructCall, 
    Reg_ExprEnumCall,
    Reg_ExprMethodCall, 
    Reg_ExprBinaryOp,
    Reg_ExprIdentifier, 
    Reg_ExprLiteral, 
    Reg_ExprVar,
} RegisterEntryTag;

static inline khint_t string_hash(StringView view) { return kh_hash_bytes((int)view.len, (const unsigned char*)view.ptr); }
static inline int string_eq(StringView a, StringView b) { return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0; }

typedef struct {
    SourceRange param;
    Type bound;
} GenericBinding;

typedef struct {
    char* mangled_name;
    GenericBinding*  bindings;
    size_t bindings_count;
    RegisterEntryTag def_kind;
    StringView def_name;
} GenericInstance;

KHASHL_MAP_INIT(KH_LOCAL, GenericInstanceTable, mono_table, StringView, GenericInstance, string_hash, string_eq)

typedef struct {
    GenericInstanceTable* table;
} GenericRegistry;

typedef struct { uint32_t id; uint16_t file_id; RegisterEntryTag kind; } EntityID;
typedef struct { uint32_t next_id; } IDCounter;

typedef struct {
    RegisterEntryTag tag;
    char* name;
    Type type;
    EntityID eid;
    SourceRange decl_range;
    SourceRange decl_name_range;
    union {
        struct { Type type; VarMode mode; bool is_mut; } var;
        struct { Type type; VarMode mode; } let;
        struct { Type type; bool is_pub; } const_;
        struct { Type type; bool is_pub; } local;
        struct { Param* params; size_t params_count; Type return_type; bool is_pub; bool is_unsafe; Register* child_reg; SourceRange* generic_params; size_t generic_params_count; GenericParam* generic_param_nodes; } function;
        struct { StructParam* fields; size_t fields_count; bool is_pub; SourceRange* generic_params; size_t generic_params_count; GenericParam* generic_param_nodes; } strct;
        struct { EnumVariant* variants; size_t variants_count; bool is_pub; SourceRange* generic_params; size_t generic_params_count; GenericParam* generic_param_nodes; } enm;
        struct { StructParam* fields; size_t fields_count; char** traits; size_t traits_count; char* parent; bool is_pub; FunctionMethod* methods; size_t methods_count; ClassAttachTag attached_tag; StructParam* attached_fields; size_t attached_fields_count; SourceRange* generic_params; size_t generic_params_count; GenericParam* generic_param_nodes; } _class;
        struct { TraitMethod* methods; size_t methods_count; bool is_pub; } trait;
        struct { SourceRange name; Param* params; size_t params_count; Type return_type; SourceRange* generic_args; size_t generic_args_count; } expr_function_call;
        struct { SourceRange name; SourceRange function; Param* params; size_t params_count; Type return_type; SourceRange* generic_args; size_t generic_args_count; } expr_class_call;
        struct { SourceRange name; SourceRange function; Param* params; size_t params_count; Type return_type; SourceRange* generic_args; size_t generic_args_count; } expr_struct_call;
        struct { SourceRange name; SourceRange field; Type resolved_type; SourceRange* generic_args; size_t generic_args_count; } expr_enum_call;
        struct { Type obj_type; SourceRange method; Param* params; size_t params_count; Type return_type; } expr_method_call;
        struct { Type left_type; LexerTokenTag op; Type right_type; Type resolved_type; } expr_binary_op;
        struct { SourceRange name; Type resolved_type; } expr_identifier;
        struct { Type resolved_type; } expr_literal;
        struct { SourceRange abi; SourceRange ffi; ExternFunction* funcs; size_t funcs_count; bool is_pub; } extern_;
        struct { SourceRange name; Type resolved_type; } expr_var;
    } data;
} RegisterEntry;

KHASHL_MAP_INIT(KH_LOCAL, RegisterTable, register_table, StringView, RegisterEntry, string_hash, string_eq)
KHASHL_MAP_INIT(KH_LOCAL, PendingTable, pending_table, StringView, EntityID, string_hash, string_eq)

typedef struct Register {
    RegisterTable* table;
    struct Register* parent;
    PendingTable* pending;
    IDCounter* counter;
    GenericRegistry* mono;
} Register;

void register_free(Register* reg);

typedef struct { char* func_name; uint32_t func_chunk; Register* body_reg; } FuncBody;

typedef struct {
    FuncBody* data; size_t len; size_t cap; uint32_t func_counter;
} FuncBodyList;

static inline void func_body_list_push(FuncBodyList* fl, FuncBody fb) { ARR_PUSH(*fl, fb); }

static inline void func_body_list_free(FuncBodyList* fl) {
    for (size_t i = 0; i < fl->len; i++) {
        free(fl->data[i].func_name);
        register_free(fl->data[i].body_reg);
        free(fl->data[i].body_reg);
    }
    ARR_FREE(*fl);
}

typedef struct { EntityID* ids; size_t count; RegisterEntryTag* kinds; } ID;
typedef struct { StringView type_name; Type type; } GenericArg;

void resolve_generic_call(StringView def_name, SourceRange* call_generic_args, size_t call_generic_count, Register* reg, GenericRegistry* mono, CheckerErrList* errors, SourceRange error_range);
#endif