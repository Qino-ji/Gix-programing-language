#include "codegen.h"
#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static LLVMValueRef cg_expr(CG *cg, IR_Expr *e) {
    if (!e) return LLVMConstNull(LLVMInt32TypeInContext(cg->ctx));
    switch (e->tag) {
    case IR_Expr_Literal:    return cg_expr_literal(cg, e);
    case IR_Expr_VarRef:     return cg_expr_varref(cg, e);
    case IR_Expr_BinOp:      return cg_expr_binop(cg, e);
    case IR_Expr_Call:       return cg_expr_call(cg, e);
    case IR_Expr_MethodCall: return cg_expr_method_call(cg, e);
    case IR_Expr_MakeStruct: return cg_expr_make_struct(cg, e);
    case IR_Expr_MakeClass:  return cg_expr_make_class(cg, e);
    case IR_Expr_MakeEnum:   return cg_expr_make_enum(cg, e);
    case IR_Expr_Field:      return cg_expr_field(cg, e);
    case IR_Expr_Index:      return cg_expr_index(cg, e);
    case IR_Expr_Cast:       return cg_expr_cast(cg, e);
    case IR_Expr_Deref:      return cg_expr_deref(cg, e);
    default:                 return LLVMConstNull(cg_type(cg, e->ty));
    }
}

static void cg_stmt(CG *cg, IR_Stmt *s) {
    if (!s) return;
    LLVMBasicBlockRef cur = LLVMGetInsertBlock(cg->builder);
    if (cur && LLVMGetBasicBlockTerminator(cur)) return;
    switch (s->tag) {
    case IR_Stmt_VarDecl:
    case IR_Stmt_LetDecl:
    case IR_Stmt_ConstDecl: cg_stmt_vardecl(cg, s);     break;
    case IR_Stmt_LocalDecl: cg_stmt_local_decl(cg, s);  break;
    case IR_Stmt_SsaTemp:   cg_stmt_ssa_temp(cg, s);    break;
    case IR_Stmt_Assign:    cg_stmt_assign(cg, s);      break;
    case IR_Stmt_Return:    cg_stmt_return(cg, s);      break;
    case IR_Stmt_Expr:      cg_expr(cg, s->data.expr.expr); break;
    case IR_Stmt_If:        cg_stmt_if(cg, s);          break;
    case IR_Stmt_While:     cg_stmt_while(cg, s);       break;
    case IR_Stmt_For:       cg_stmt_for(cg, s);         break;
    case IR_Stmt_Match:     cg_stmt_match(cg, s);       break;
    case IR_Stmt_AtomicOp:  cg_stmt_atomic_op(cg, s);   break;
    default: break;
    }
}
