#include "ir.h"
#include "import.h"
#include "register.h"
#include "type.h"

IR_Type type(const Type* t, Register* reg);
StringView string_view_from_range(SourceRange range);
RegisterEntry* register_get(Register* reg, StringView name);
Type infer_literal_type(SourceRange range);

IR_Val ir_lower_expr(IR_Function* fn, Exprs* expr, Register* reg) {
    switch (expr->tag) {

        case Expr_Literals: {
            Type ast_type = infer_literal_type(expr->data.literals.range);
            IR_Type irt = type(&ast_type, reg);
            return ir_const_i32(0);
        }

        case Expr_Vars:
        case Expr_Identifiers: {
            SourceRange r = (expr->tag == Expr_Vars) ? expr->data.vars.name : expr->data.identifiers.name;
            StringView key = string_view_from_range(r);
            RegisterEntry* entry = register_get(reg, key);

            if (!entry) return (IR_Val){ .tag = IR_Val_Undef };

            IR_Type irt = type(&entry->type, reg); 
            return ir_reg(entry->eid.id, irt);
        }

        case Expr_BinaryOps: {
            IR_Val lhs = ir_lower_expr(fn, expr->data.binary_ops.left,  reg);
            IR_Val rhs = ir_lower_expr(fn, expr->data.binary_ops.right, reg);
            uint32_t dest = ir_next_reg(fn);
            IR_Type ty = lhs.reg.ty;

            IR_InstTag op;
            switch (expr->data.binary_ops.op) {
                case Plus:   op = IR_Add; break;
                case Minuss: op = IR_Sub; break;
                case Stars:  op = IR_Mul; break;
                case Slashs: op = IR_Div; break;
                default: op = IR_Nop; break;
            }

            ir_emit(fn, (IR_Inst){
                .tag = op,
                .dest_reg = dest,
                .bin = { .ty = ty, .lhs = lhs, .rhs = rhs }
            });

            return ir_reg(dest, ty);
        }

        case Expr_Function: {
            StringView name = string_view_from_range(expr->data.function_call.name);
            RegisterEntry* entry = register_get(reg, name);
            IR_Type ret_ty = entry ? type(&entry->data.function.return_type, reg) : (IR_Type){ .tag = IR_Type_Void };
            size_t argc = expr->data.function_call.param_count;
            IR_Val* args = malloc(argc * sizeof(IR_Val));

            for (size_t i = 0; i < argc; i++) {
                Param* p = &expr->data.function_call.param[i];
                StringView pname = string_view_from_range(p->name);
                RegisterEntry* pentry = register_get(reg, pname);

                args[i] = pentry ? ir_reg(pentry->eid.id, type(&pentry->type, reg)) : (IR_Val){ .tag = IR_Val_Undef };
            }

            uint32_t dest = ir_next_reg(fn);
            IR_Val fn_val = ir_reg(entry ? entry->eid.id : 0, (IR_Type){ .tag = IR_Type_Fn });

            ir_emit(fn, (IR_Inst){
                .tag      = IR_Call,
                .dest_reg = dest,
                .call     = { .ty = ret_ty, .fn = fn_val, .args = args, .args_count = argc }
            });

            return ir_reg(dest, ret_ty);
        }

        default: return (IR_Val){ .tag = IR_Val_Undef };
    }
}