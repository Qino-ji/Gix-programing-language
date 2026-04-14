#ifndef VIX_IR_H
#define VIX_IR_H

#include "import.h"
#include "ast.h"


typedef struct IR_Type IR_Type;
typedef struct IR_Inst IR_Inst;

typedef enum {
    IR_Type_I8,
    IR_Type_I16,
    IR_Type_I32,
    IR_Type_I64,
    IR_Type_F32,
    IR_Type_F64,
    IR_Type_Bool,
    IR_Type_Char,
    IR_Type_Void,
    IR_Type_Ptr,
    IR_Type_Array,
    IR_Type_Struct,
    IR_Type_Enum,
    IR_Type_Fn,
} IR_TypeTag;


typedef enum {
    IR_Add,
    IR_Sub,
    IR_Mul,
    IR_Div,
    IR_Mod,
    IR_And,
    IR_Or,
    IR_Xor,
    IR_Not,
    IR_Shl,
    IR_Shr,
    IR_CmpEq,
    IR_CmpNe,
    IR_CmpLt,
    IR_CmpLe,
    IR_CmpGt,
    IR_CmpGe,
    IR_Alloca,
    IR_Load,
    IR_Store,
    IR_GEP,
    IR_FieldPtr,
    IR_BitCast,
    IR_ZExt,
    IR_SExt,
    IR_Trunc,
    IR_FPToInt,
    IR_IntToFP,
    IR_Call,
    IR_MethodCall,
    IR_Return,
    IR_Branch,
    IR_Jump,
    IR_Phi,
    IR_Move,
    IR_Nop,
} IR_InstTag;

typedef enum {
    IR_Val_Const_Int,
    IR_Val_Const_Float,
    IR_Val_Const_Bool,
    IR_Val_Const_Char,
    IR_Val_Const_Null,
    IR_Val_Reg,
    IR_Val_Global,
    IR_Val_Undef,
} IR_ValTag;

struct IR_Type {
    IR_TypeTag tag;
    union {
        struct { IR_Type* inner; } ptr;
        struct { IR_Type* inner; size_t len; } array;
        struct { uint32_t name_id; } named;
        struct {
            IR_Type* ret;
            IR_Type* params;
            size_t params_count;
        } fn;
    } data;
};

typedef struct {
    IR_ValTag tag;
    union {
        struct { IR_Type ty; int64_t val; } const_int;
        struct { IR_Type ty; double val; } const_float;
        struct { bool val; } const_bool;
        struct { char val; } const_char;
        struct { IR_Type ty; uint32_t id; } reg;
        struct { IR_Type ty; uint32_t id; } global;
    };
} IR_Val;


static inline IR_Val ir_const_i32(int32_t n) {
    IR_Val v = {0};
    v.tag = IR_Val_Const_Int;
    v.const_int.ty.tag = IR_Type_I32;
    v.const_int.val = n;
    return v;
}
static inline IR_Val ir_const_i64(int64_t n) {
    IR_Val v = {0};
    v.tag = IR_Val_Const_Int;
    v.const_int.ty.tag = IR_Type_I64;
    v.const_int.val = n;
    return v;
}
static inline IR_Val ir_reg(uint32_t id, IR_Type ty) {
    IR_Val v = {0};
    v.tag = IR_Val_Reg;
    v.reg.id = id;
    v.reg.ty = ty;
    return v;
}


typedef struct IR_Inst {
    IR_InstTag tag;
    uint32_t dest_reg;
    SourceRange origin;
    union {
        struct { IR_Type ty; IR_Val lhs; IR_Val rhs; } bin;
        struct { IR_Type ty; IR_Val operand; } unary;
        struct { IR_Type ty; } alloca;
        struct { IR_Type ty; IR_Val ptr; } load;
        struct { IR_Val val; IR_Val ptr; } store;
        struct { IR_Type ty; IR_Val ptr; IR_Val* indices; size_t indices_count; } gep;
        struct { IR_Type ty; IR_Val ptr; uint32_t field_index; } field_ptr;
        struct { IR_Type ty; IR_Val fn; IR_Val* args; size_t args_count; } call;
        struct { IR_Type ty; IR_Val object; uint32_t method_id; IR_Val* args; size_t args_count; } method_call;
        struct { IR_Val val; bool has_val; } ret;
        struct { IR_Val cond; uint32_t then_bb; uint32_t else_bb; } branch;
        struct { uint32_t target_bb; } jump;
        struct { IR_Type ty; uint32_t* bbs; IR_Val* vals; size_t count; } phi;
        struct { IR_Type ty; IR_Val src; } move;
    };
} IR_Inst;

typedef struct {
    uint32_t id;
    char* label;
    IR_Inst* insts;
    size_t insts_count;
    size_t insts_cap;
} IR_BasicBlock;


typedef struct {
    uint32_t name_id;
    char* name;
    IR_Type ret_ty;
    IR_Type* param_tys;
    char** param_names;
    size_t params_count;
    IR_BasicBlock* blocks;
    size_t blocks_count;
    size_t blocks_cap;
    uint32_t next_reg;
    bool is_pub;
    bool is_unsafe;
} IR_Function;

typedef struct {
    char* name;
    IR_Function* functions;
    size_t functions_count;
    size_t functions_cap;
    struct {
        uint32_t id;
        char* name;
        IR_Type ty;
        IR_Val init;
        bool is_pub;
    }* globals;
    size_t globals_count;
    size_t globals_cap;
} IR_Module;

static inline uint32_t ir_next_reg(IR_Function* fn) {
    return fn->next_reg++;
}

static inline void ir_emit(IR_Function* fn, IR_Inst inst) {
    IR_BasicBlock* bb = &fn->blocks[fn->blocks_count - 1];
    if (bb->insts_count == bb->insts_cap) {
        bb->insts_cap = bb->insts_cap ? bb->insts_cap * 2 : 8;
        bb->insts = realloc(bb->insts, bb->insts_cap * sizeof(IR_Inst));
    }
    bb->insts[bb->insts_count++] = inst;
}

static inline uint32_t ir_push_block(IR_Function* fn, char* label) {
    if (fn->blocks_count == fn->blocks_cap) {
        fn->blocks_cap = fn->blocks_cap ? fn->blocks_cap * 2 : 4;
        fn->blocks = realloc(fn->blocks, fn->blocks_cap * sizeof(IR_BasicBlock));
    }
    IR_BasicBlock bb = {0};
    bb.id = (uint32_t)fn->blocks_count;
    bb.label = label;
    fn->blocks[fn->blocks_count++] = bb;
    return bb.id;
}

#endif