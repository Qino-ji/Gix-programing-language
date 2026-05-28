#ifndef VIX_CODEGEN_H
#define VIX_CODEGEN_H

#include "import.h"
#include "ir.h"

#define CG_MAX 8192

#ifdef __cplusplus
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>


typedef ARR(LLVMValueRef) RefArr;
typedef ARR(LLVMTypeRef)  TypeRef;


typedef struct {
    LLVMContextRef ctx;
    LLVMModuleRef  mod;
    LLVMBuilderRef builder;

    RefArr vars;
    RefArr funcs;
    RefArr methods;

    TypeRef func_types;
    TypeRef method_types;
    TypeRef types;

    LLVMValueRef cur_func;
    LLVMTypeRef  cur_ret_type;
    LLVMBasicBlockRef entry_bb;
    IR_Module *ir_mod;

    uint32_t next_eid;
} CG;

#else
typedef struct CG CG;
typedef struct LLVMOpaqueValue* LLVMValueRef;
typedef struct LLVMOpaqueType* LLVMTypeRef;
typedef struct LLVMOpaqueBasicBlock* LLVMBasicBlockRef;
typedef int LLVMAtomicOrdering;
#endif

#ifdef __cplusplus
extern "C" {
#endif

CG *cg_create(const char *module_name);

int cg_verify(CG *cg);
int cg_emit_obj(CG *cg, const char *project_name, const char *source_file);
int cg_link_exe(const char *project_name, const char *source_file);
int cg_field_index(CG *cg, LLVMTypeRef sty, SourceRange field_name);

LLVMValueRef cg_expr(CG *cg, IR_Expr *e);
LLVMValueRef cg_alloca(CG *cg, LLVMTypeRef ty, const char *name);
LLVMValueRef cg_expr_literal(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_literal_bool(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_literal_char(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_literal_int(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_literal_float(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_literal_str(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_varref(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_binop(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_call(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_method_call(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_make_struct(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_make_class(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_make_enum(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_field(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_index(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_cast(CG *cg, IR_Expr *e);
LLVMValueRef cg_expr_deref(CG *cg, IR_Expr *e);
LLVMValueRef cg_assign_compute_rhs(CG *cg, IR_Expr *tgt, LLVMValueRef rhs, LexerTokenTag op);
LLVMValueRef cg_var_get(CG *cg, EntityID eid);
LLVMValueRef cg_func_get(CG *cg, EntityID eid);
LLVMValueRef cg_method_get(CG *cg, EntityID eid);
LLVMTypeRef cg_type_int(CG *cg, Type t);
LLVMTypeRef cg_type_float(CG *cg, Type t);
LLVMTypeRef cg_type_ptr(CG *cg, Type t);
LLVMTypeRef cg_type_custom(CG *cg, Type t);
LLVMTypeRef  cg_ftype_get(CG *cg, EntityID eid);
LLVMTypeRef cg_type(CG *cg, Type t);
LLVMTypeRef cg_mtype_get(CG *cg, EntityID eid);

static void cg_module_declare_vtable(CG* cg, IR_Module* mod);
static void cg_build_func(CG *cg, IR_FuncDef *def, uint32_t eid, bool is_method);

void cg_stmt_vardecl(CG *cg, IR_Stmt *s);
void cg_stmt_local_decl(CG *cg, IR_Stmt *s);
void cg_stmt_ssa_temp(CG *cg, IR_Stmt *s);
void cg_stmt_assign(CG *cg, IR_Stmt *s);
void cg_stmt_return(CG *cg, IR_Stmt *s);
void cg_stmt_if(CG *cg, IR_Stmt *s);
void cg_stmt_while(CG *cg, IR_Stmt *s);
void cg_stmt_for(CG *cg, IR_Stmt *s);
void cg_stmt_match(CG *cg, IR_Stmt *s);
void cg_stmt_atomic_op(CG *cg, IR_Stmt *s);
void cg_build_body(CG *cg, IR_Stmt *body, size_t count);
void cg_var_set(CG *cg, EntityID eid, LLVMValueRef v);
void cg_assign_to_varref(CG *cg, IR_Expr *tgt, LLVMValueRef rhs);
void cg_assign_to_deref(CG *cg, IR_Expr *tgt, LLVMValueRef rhs);
void cg_assign_to_field(CG *cg, IR_Expr *tgt, LLVMValueRef rhs);
void cg_assign_to_index(CG *cg, IR_Expr *tgt, LLVMValueRef rhs);
void cg_stmt_atomic_load(CG *cg, IR_Stmt *s, LLVMValueRef target_ptr, LLVMAtomicOrdering ord);
void cg_stmt_atomic_store(CG *cg, IR_Stmt *s, LLVMValueRef target_ptr, LLVMAtomicOrdering ord);
void cg_stmt_atomic_cmpxchg(CG *cg, IR_Stmt *s, LLVMValueRef target_ptr, LLVMAtomicOrdering ord, LLVMAtomicOrdering ord2);
void cg_declare_func(CG *cg, IR_FuncDef *def, uint32_t eid);
void cg_register_struct(CG *cg, SourceRange name, uint32_t eid, IR_FieldDef *fields, size_t nfields);
void cg_module_register_types(CG *cg, IR_Module *mod);
void cg_module_declare_funcs(CG *cg, IR_Module *mod);
void cg_module_declare_extern(CG *cg, IR_Def *d);
void cg_module_build_funcs(CG *cg, IR_Module *mod);
void cg_stmt(CG *cg, IR_Stmt *s);
void cg_destroy(CG *cg);
void cg_module(CG *cg, IR_Module *mod);
void cg_dump(CG *cg);
LLVMAtomicOrdering cg_ordering(OrderingTag o);



#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
inline char *sr_cstr_inline(SourceRange r, char *buf, size_t sz) {
    size_t len = (size_t)(r.end - r.start);
    if (len >= sz) len = sz - 1;
    memcpy(buf, r.start, len);
    buf[len] = '\0';
    return buf;
}

#ifdef __cplusplus
inline char *sr_cstr(SourceRange r, char *buf, size_t sz) {
    return sr_cstr_inline(r, buf, sz);
}
#endif

#endif

#endif // VIX_CODEGEN_H 