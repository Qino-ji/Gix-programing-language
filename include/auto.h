#ifndef TYPEINFER_H
#define TYPEINFER_H

#include "ast.h"
#include "register.h"

void type_infer_pass(Stmts* body, size_t count, Register* reg);
void infer_func(Stmts* stmt, Register* reg);
Type infer_stmt_return_type(Stmts* stmt, Register* reg);
Type infer_expr(Exprs* expr, Register* reg);
Type unify(Type a, Type b);
Type infer_body_return(Stmts* body, size_t count, Register* reg);


#endif