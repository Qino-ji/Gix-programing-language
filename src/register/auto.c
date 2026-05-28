#include "import.h"
#include "ast.h"
#include "register.h"
#include "auto.h"




Type resolve_type(SourceRange r, Register* reg) {
    if (!r.start || r.start == r.end) {
        CG_UNREACHABLE_MSG("Register attempted to resolve a null/empty type range.");
    }
    size_t len = r.end - r.start;

    if (!r.start) return (Type){ .tag = Type_Void };

    if (len == 3  && memcmp(r.start, "int",     3)  == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits = 32 };
    if (len == 4  && memcmp(r.start, "int8",    4)  == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits = 8  };
    if (len == 5  && memcmp(r.start, "int16",   5)  == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits = 16 };
    if (len == 5  && memcmp(r.start, "int32",   5)  == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits = 32 };
    if (len == 5  && memcmp(r.start, "int64",   5)  == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits = 64 };
    if (len == 5  && memcmp(r.start, "float",   5)  == 0) return (Type){ .tag = Type_Float, .data.float_t.bits = 32 };
    if (len == 7  && memcmp(r.start, "float32", 7)  == 0) return (Type){ .tag = Type_Float, .data.float_t.bits = 32 };
    if (len == 7  && memcmp(r.start, "float64", 7)  == 0) return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };
    if (len == 4  && memcmp(r.start, "char",    4)  == 0) return (Type){ .tag = Type_Char };
    if (len == 3  && memcmp(r.start, "str",     3)  == 0) return (Type){ .tag = Type_Str  };
    if (len == 4  && memcmp(r.start, "bool",    4)  == 0) return (Type){ .tag = Type_Bool };
    if (len == 4  && memcmp(r.start, "void",    4)  == 0) return (Type){ .tag = Type_Void };

    RegisterEntry* entry = register_get(reg, string_range(r));

    if (entry) {
        switch (entry->tag) {
            case Reg_Class:  return (Type){ .tag = Type_Custom, .data.custom.name = r };
            case Reg_Struct: return (Type){ .tag = Type_Custom, .data.custom.name = r };
            case Reg_Enum:   return (Type){ .tag = Type_Custom, .data.custom.name = r };
            default: break;
        }
    }

    // Unknown names should remain symbolic/custom (and be diagnosed later),
    // not silently become `void` which causes cascaded type crashes.
    return (Type){ .tag = Type_Custom, .data.custom.name = r };
}


Type resolve_type_tree(SourceRange r, Type* tree, Register* reg) {
    if (!tree && (!r.start || r.start == r.end)) { return (Type){ .tag = Type_Void }; }
    
    if (!tree) return resolve_type(r, reg);

    switch (tree->tag) {
        case Type_Ptr: {
            Type inner = resolve_type_tree((SourceRange){0}, tree->data.ptr.inner, reg);
            Type* inner_heap = malloc(sizeof(Type));
            *inner_heap = inner;
            return (Type){ .tag = Type_Ptr, .data.ptr.inner = inner_heap };
        }
        case Type_RawPtr: {
            Type inner = resolve_type_tree((SourceRange){0}, tree->data.raw_ptr.inner, reg);
            Type* inner_heap = malloc(sizeof(Type));
            *inner_heap = inner;
            return (Type){ .tag = Type_RawPtr, .data.raw_ptr.inner = inner_heap };
        }
        case Type_Array: {
            Type inner = resolve_type_tree((SourceRange){0}, tree->data.array_t.inner, reg);
            Type* inner_heap = malloc(sizeof(Type));
            *inner_heap = inner;
            return (Type){ .tag = Type_Array, .data.array_t.inner = inner_heap, .data.array_t.len = tree->data.array_t.len };
        }
        case Type_Custom:
            return resolve_type(tree->data.custom.name, reg);
        default:
            return *tree;
    }
}


Type infer_literal_type(SourceRange range) {
    size_t len = range.end - range.start;
    if (len == 0) return (Type){ .tag = Type_Void };
    if ((len == 4 && memcmp(range.start, "true",  4) == 0) ||
        (len == 5 && memcmp(range.start, "false", 5) == 0))
        return (Type){ .tag = Type_Bool };

    if (len == 4 && memcmp(range.start, "null", 4) == 0) return (Type){ .tag = Type_Ptr, .data.ptr.inner = NULL };
    if (range.start[0] == '"')  return (Type){ .tag = Type_Str  };
    if (range.start[0] == '\'') return (Type){ .tag = Type_Char };

    bool has_dot = false;
    for (size_t i = 0; i < len; i++) {
        if (range.start[i] == '.') { has_dot = true; break; }
    }

    if (has_dot) return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };
    return (Type){ .tag = Type_Int, .data.int_t.bits = 32 };
}

static uint64_t builtin_size_of(Type* ty, Register* reg) {
    if (!ty) return 0;
    switch (ty->tag) {
        case Type_Int:   return ty->data.int_t.bits / 8;
        case Type_Float: return ty->data.float_t.bits / 8;
        case Type_Bool:  return 1;
        case Type_Char:  return 4;
        case Type_Str:   return 8;
        case Type_Ptr:
        case Type_RawPtr:
        case Type_FnPtr: return 8;
        case Type_Custom: {
            RegisterEntry* ent = register_get(reg, sv_of(ty->data.custom.name));
            if (!ent) return 0;
            uint64_t total = 0;
            if (ent->tag == Reg_Struct) {
                for (size_t i = 0; i < ent->data.strct.fields_count; i++) {
                    Type ft = resolve_type_tree(ent->data.strct.fields[i].c_type, ent->data.strct.fields[i].type_tree, reg);
                    total += builtin_size_of(&ft, reg);
                }
            } else if (ent->tag == Reg_Class) {
                for (size_t i = 0; i < ent->data._class.fields_count; i++) {
                    Type ft = resolve_type_tree(ent->data._class.fields[i].c_type, ent->data._class.fields[i].type_tree, reg);
                
                    total += builtin_size_of(&ft, reg);
                }
            }
            return total;
        }
        default: return 0;
    }
}

static uint64_t builtin_align_of(Type* ty, Register* reg) {
    if (!ty) return 1;
    switch (ty->tag) {
        case Type_Int:   return ty->data.int_t.bits / 8;
        case Type_Float: return ty->data.float_t.bits / 8;
        case Type_Bool:  return 1;
        case Type_Char:  return 4;
        case Type_Str:
        case Type_Ptr:
        case Type_RawPtr:
        case Type_FnPtr: return 8;
        case Type_Custom: {
            RegisterEntry* ent = register_get(reg, sv_of(ty->data.custom.name));
            uint64_t max_align = 1;

            if (!ent) return 1;

            if (ent->tag == Reg_Struct) {
                for (size_t i = 0; i < ent->data.strct.fields_count; i++) {
                    Type ft = resolve_type_tree(ent->data.strct.fields[i].c_type, ent->data.strct.fields[i].type_tree, reg);
                    uint64_t a = builtin_align_of(&ft, reg);
                    if (a > max_align) max_align = a;
                }
            }
            return max_align;
        }
        default: return 1;
    }
}

Type infer_expr_type(Exprs* expr, Register* reg) {
    if (!expr) return (Type){ .tag = Type_Void };

    switch (expr->tag) {
        case Expr_Literals: return infer_literal_type(expr->data.literals.range);
        case Expr_Identifiers:
        case Expr_Vars: {
            SourceRange r = (expr->tag == Expr_Identifiers) ? expr->data.identifiers.name : expr->data.vars.name;
            RegisterEntry* entry = register_get(reg, string_range(r));
            if (entry) return entry->type;
            return (Type){ .tag = Type_Void };
        }

        case Expr_BinaryOps: {
            LexerTokenTag op = expr->data.binary_ops.op;
            if (op == DoubleEqualss   
                || op == NotEqualss     
                || op == Lesses          
                || op == Greaters 
                || op == LessEqualss     
                || op == GreaterEqualss  
                || op == Ands            
                || op == Ors)
                return (Type){ .tag = Type_Bool };
            return infer_expr_type(expr->data.binary_ops.left, reg);
        }

        case Expr_Function: {
            RegisterEntry* entry = register_get(reg, string_range(expr->data.function_call.name));
            if (entry && entry->tag == Reg_Function) return entry->data.function.return_type;
            return (Type){ .tag = Type_Void };
        }

        case Expr_Class_Calls:
        case Expr_Struct_Calls: {
            SourceRange r = (expr->tag == Expr_Class_Calls) ? expr->data.class_calls.name : expr->data.struct_calls.name;

            return (Type){ .tag = Type_Custom, .data.custom.name = r };
        }

        default:
            return (Type){ .tag = Type_Void };
    }
}


static bool type_is_void(Type t) { return t.tag == Type_Void; }
static void report_unknown_type(Register* reg, SourceRange range, StringView name, CheckerErrList* errors) {
    if (!errors) return;
    checker_err_push(errors, (CheckerErr){
        .tag = Err_Tag_UKT,
        .data.ukt = { .range = range, .var_name = name }
    });
}

static bool type_eq(Type a, Type b) {
    if (a.tag != b.tag) return false;
    if (a.tag == Type_Int)   return a.data.int_t.bits == b.data.int_t.bits;
    if (a.tag == Type_Float) return a.data.float_t.bits == b.data.float_t.bits;
    return true;
}

static Type unify(Type a, Type b) {
    if (type_is_void(a)) return b;
    if (type_is_void(b)) return a;
    if (a.tag == Type_Float || b.tag == Type_Float) return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };
    if (type_eq(a, b)) return a;
    if (a.tag == Type_Int && b.tag == Type_Int) return (Type){ .tag = Type_Int, .data.int_t.bits = a.data.int_t.bits > b.data.int_t.bits ? a.data.int_t.bits : b.data.int_t.bits };
    return a;
}

static bool is_comparison_op(LexerTokenTag op) {
    switch (op) {
        case DoubleEqualss:
        case NotEqualss:
        case Lesses:
        case Greaters:
        case LessEqualss:
        case GreaterEqualss:
        case Ands:
        case Ors: return true;
        default: return false;
    }
}

static bool is_compound_assign(LexerTokenTag op) {
    switch (op) {
        case PlusEqualss:
        case MinusEqualss:
        case StarEqualss:
        case SlashEqualss:
        case PercentEqualss:
        case AmpersandEqualss:
        case PipeEqualss:
        case CaretEqualss:
        case LeftShiftEqualss:
        case RightShiftEqualss: return true;
        default: return false;
    }
}

static bool class_has_operation(Register* reg, SourceRange class_name, LexerTokenTag op) {
    RegisterEntry* ce = register_get(reg, (StringView){ class_name.start, (size_t)(class_name.end - class_name.start) });
    if (!ce || ce->tag != Reg_Class) return false;
    for (size_t i = 0; i < ce->data._class.methods_count; i++) { if (ce->data._class.methods[i].operation.op == op) return true; }
    return false;
}

static Type infer_literal(SourceRange r) {
    size_t len = r.end - r.start;

    if (!len) return (Type){ .tag = Type_Void };
    if ((len == 4 && memcmp(r.start, "true",  4) == 0) || (len == 5 && memcmp(r.start, "false", 5) == 0)) return (Type){ .tag = Type_Bool };
    if (len == 4 && memcmp(r.start, "null", 4) == 0) return (Type){ .tag = Type_Ptr, .data.ptr.inner = NULL };
    if (r.start[0] == '"') return (Type){ .tag = Type_Str };
    if (r.start[0] == '\'') return (Type){ .tag = Type_Char };
    for (size_t i = 0; i < len; i++) if (r.start[i] == '.') return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };

    return (Type){ .tag = Type_Int, .data.int_t.bits = 32 };
}

static Type infer_expr(Exprs* expr, Register* reg) {
    if (!expr) return (Type){ .tag = Type_Void };

    switch (expr->tag) {
        case Expr_Literals: return infer_literal(expr->data.literals.range);
        case Expr_Identifiers: {
            RegisterEntry* e = register_get(reg, (StringView){
                expr->data.identifiers.name.start,
                (size_t)(expr->data.identifiers.name.end - expr->data.identifiers.name.start)
            });
            return e ? e->type : (Type){ .tag = Type_Void };
        }

        case Expr_Vars: {
            RegisterEntry* e = register_get(reg, (StringView){
                expr->data.vars.name.start,
                (size_t)(expr->data.vars.name.end - expr->data.vars.name.start)
            });
            return e ? e->type : (Type){ .tag = Type_Void };
        }

        case Expr_BinaryOps: {
            LexerTokenTag op = expr->data.binary_ops.op;
            if (is_comparison_op(op)) return (Type){ .tag = Type_Bool };
            Type lhs = infer_expr(expr->data.binary_ops.left,  reg);
            Type rhs = infer_expr(expr->data.binary_ops.right, reg);
            return unify(lhs, rhs);
        }

        case Expr_Function: {
            RegisterEntry* e = register_get(reg, (StringView){
                expr->data.function_call.name.start,
                (size_t)(expr->data.function_call.name.end - expr->data.function_call.name.start)
            });
            if (e && e->tag == Reg_Function) return e->data.function.return_type;
            return (Type){ .tag = Type_Void };
        }

        case Expr_MethodCalls: {
            Type obj_ty = infer_expr(expr->data.method_calls.object, reg);
            if (obj_ty.tag != Type_Custom) return (Type){ .tag = Type_Void };

            SourceRange cname = obj_ty.data.custom.name;
            SourceRange mname = expr->data.method_calls.method;
            size_t clen = cname.end - cname.start;
            size_t mlen = mname.end - mname.start;
            char* mangled = malloc(clen + 1 + mlen + 1);
            memcpy(mangled, cname.start, clen);
            mangled[clen] = '_';
            memcpy(mangled + clen + 1, mname.start, mlen);
            mangled[clen + 1 + mlen] = '\0';

            RegisterEntry* me = register_get(reg, (StringView){ mangled, clen + 1 + mlen });
            Type ret = (me && me->tag == Reg_Function) ? me->data.function.return_type : (Type){ .tag = Type_Void };
            free(mangled);
            return ret;
        }

        case Expr_Struct_Calls: {
            SourceRange sname = expr->data.struct_calls.name;
            RegisterEntry* se = register_get(reg, (StringView){ sname.start, (size_t)(sname.end - sname.start) });
            SourceRange field = expr->data.struct_calls.function;

            if (!se || se->tag != Reg_Struct) return (Type){ .tag = Type_Custom, .data.custom.name = sname };
            if (!field.start) return (Type){ .tag = Type_Custom, .data.custom.name = sname };

            for (size_t i = 0; i < se->data.strct.fields_count; i++) {
                StructParam* f = &se->data.strct.fields[i];
                size_t fn_len = f->name.end - f->name.start;
                size_t fl_len = field.end - field.start;
                if (fn_len == fl_len && memcmp(f->name.start, field.start, fn_len) == 0) {
                    if (f->type_tree) return *f->type_tree;
                    return (Type){ .tag = Type_Custom, .data.custom.name = f->c_type };
                }
            }
            return (Type){ .tag = Type_Void };
        }

        case Expr_Enum_Calls: return (Type){ .tag = Type_Custom, .data.custom.name = expr->data.enum_calls.name };
        case Expr_Class_Calls: return (Type){ .tag = Type_Custom, .data.custom.name = expr->data.class_calls.name };
        case Expr_Cast: return expr->data.cast.ty ? *expr->data.cast.ty : (Type){ .tag = Type_Void };
        case Expr_Unary: if (expr->data.unary.op == Nots) return (Type){ .tag = Type_Bool }; return infer_expr(expr->data.unary.operand, reg);
        case Expr_Builtins: return (Type){ .tag = Type_Int, .data.int_t.bits = 64 };
        default:return (Type){ .tag = Type_Void };
    } 
}


static Type infer_body_return(Stmts* body, size_t count, Register* reg) {
    Type result = (Type){ .tag = Type_Void };

    for (size_t i = 0; i < count; i++) {
        Stmts* s = &body[i];

        switch (s->tag) {
            case Stmt_Returns: {
                if (s->data.returns.expr.tag == 0 && s->data.returns.expr.data.literals.range.start == NULL) break; 

                Type t = infer_expr(&s->data.returns.expr, reg); result = unify(result, t);
                break;
            }

            case Stmt_Ifs: {
                Type then_t = infer_body_return(s->data.ifs.body, s->data.ifs.body_count, reg);
                Type else_t = infer_body_return(s->data.ifs.else_body, s->data.ifs.else_body_count, reg);
                result = unify(result, unify(then_t, else_t));
                break;
            }

            
            case Stmt_Matchs: {
                for (size_t c = 0; c < s->data.matchs.cases_count; c++) result = unify(result, infer_body_return(s->data.matchs.cases[c].body, s->data.matchs.cases[c].body_count, reg));
                if (s->data.matchs.default_body_count > 0) result = unify(result, infer_body_return(s->data.matchs.default_body, s->data.matchs.default_body_count, reg));

                break;
            }

            case Stmt_Whiles: result = unify(result, infer_body_return(s->data.whiles.body, s->data.whiles.body_count, reg)); break;
            case Stmt_Fors: result = unify(result, infer_body_return(s->data.fors.body, s->data.fors.body_count, reg)); break;
            default: break;
        }
    }

    return result;
}

static void infer_func(Stmts* stmt, Register* reg) {
    if (stmt->tag != Stmt_Functions) return;

    if (stmt->data.functions.return_type_tree && stmt->data.functions.return_type_tree->tag != Type_Void) return;
    if (stmt->data.functions.return_type.start && stmt->data.functions.return_type.start != stmt->data.functions.return_type.end) return;

    Type inferred = infer_body_return(stmt->data.functions.body, stmt->data.functions.body_count, reg);

    if (type_is_void(inferred)) return;

    Type* patched = malloc(sizeof(Type)); *patched = inferred;

    stmt->data.functions.return_type_tree = patched;
    stmt->data.functions.return_type_tag  = inferred.tag;

    if (inferred.tag == Type_Int) stmt->data.functions.return_type_bits = inferred.data.int_t.bits;
    else if (inferred.tag == Type_Float) stmt->data.functions.return_type_bits = inferred.data.float_t.bits;

    StringView fname = { stmt->data.functions.name.start, (size_t)(stmt->data.functions.name.end - stmt->data.functions.name.start) };
    RegisterEntry* entry = register_get(reg, fname);

    if (entry && entry->tag == Reg_Function) {
        entry->data.function.return_type = inferred;
        entry->type = inferred;
    }
}

static void infer_var(Stmts* stmt, Register* reg) {
    SourceRange name;
    Exprs* value;
    Type* type_tree;
    bool has_annotation;

    switch (stmt->tag) {
        case Stmt_Vars:
            name = stmt->data.vars.name;
            value = &stmt->data.vars.value;
            type_tree = stmt->data.vars.type_tree;
            has_annotation = (type_tree && type_tree->tag != Type_Void) || (stmt->data.vars.c_type.start && stmt->data.vars.c_type.start != stmt->data.vars.c_type.end);
            break;
        case Stmt_Lets:
            name = stmt->data.lets.name;
            value = &stmt->data.lets.value;
            type_tree = stmt->data.lets.type_tree;
            has_annotation = (type_tree && type_tree->tag != Type_Void) || (stmt->data.lets.c_type.start && stmt->data.lets.c_type.start != stmt->data.lets.c_type.end);
            break;
        case Stmt_Consts:
            name = stmt->data.consts.name;
            value = &stmt->data.consts.value;
            type_tree = stmt->data.consts.type_tree;
            has_annotation = (type_tree && type_tree->tag != Type_Void) || (stmt->data.consts.c_type.start && stmt->data.consts.c_type.start != stmt->data.consts.c_type.end);
            break;
        default: return;
    }

    if (has_annotation) return;
    if (!value || (value->tag == 0 && !value->data.literals.range.start)) return;

    Type inferred = infer_expr(value, reg);
    Type* patched = malloc(sizeof(Type));

    if (type_is_void(inferred)) return;

    *patched = inferred;

    switch (stmt->tag) {
        case Stmt_Vars:   stmt->data.vars.type_tree   = patched; break;
        case Stmt_Lets:   stmt->data.lets.type_tree   = patched; break;
        case Stmt_Consts: stmt->data.consts.type_tree = patched; break;
        default: break;
    }

    StringView key = { name.start, (size_t)(name.end - name.start) };
    RegisterEntry* entry = register_get(reg, key);
    if (entry) {
        entry->type = inferred;
        if (entry->tag == Reg_Var)   entry->data.var.type   = inferred;
        if (entry->tag == Reg_Let)   entry->data.let.type   = inferred;
        if (entry->tag == Reg_Const) entry->data.const_.type = inferred;
    }
}

static void infer_assign(Stmts* stmt, Register* reg) {
    Exprs* target = &stmt->data.assigns.target;
    SourceRange tname = (target->tag == Expr_Identifiers) ? target->data.identifiers.name : target->data.vars.name;
    RegisterEntry* entry = register_get(reg, (StringView){ tname.start, (size_t)(tname.end - tname.start) });

    if (stmt->tag != Stmt_Assigns) return;
    if (!is_compound_assign(stmt->data.assigns.op)) return;
    if (target->tag != Expr_Identifiers && target->tag != Expr_Vars) return;
    if (!entry) return;
    if (entry->type.tag == Type_Custom) { if (class_has_operation(reg, entry->type.data.custom.name, stmt->data.assigns.op)) return; }

    if (type_is_void(entry->type)) {
        Type val_type = infer_expr(&stmt->data.assigns.value, reg);
        if (!type_is_void(val_type)) {
            entry->type = val_type;
            if (entry->tag == Reg_Var) entry->data.var.type = val_type;
        }
    }
}


static void infer_body(Stmts* body, size_t count, Register* reg) {
    for (size_t i = 0; i < count; i++) {
        Stmts* s = &body[i];
        switch (s->tag) {
            case Stmt_Vars:
            case Stmt_Lets:
            case Stmt_Consts: infer_var(s, reg); break;
            case Stmt_Assigns: infer_assign(s, reg); break;
            case Stmt_Functions: infer_func(s, reg); infer_body(s->data.functions.body, s->data.functions.body_count, reg); break;
            case Stmt_Ifs: infer_body(s->data.ifs.body, s->data.ifs.body_count, reg); infer_body(s->data.ifs.else_body, s->data.ifs.else_body_count, reg); break;
            case Stmt_Modules: infer_body(s->data.modules.body, s->data.modules.body_count, reg); break;
            case Stmt_Whiles: infer_body(s->data.whiles.body, s->data.whiles.body_count, reg); break;
            case Stmt_Fors: infer_body(s->data.fors.body, s->data.fors.body_count, reg); break;
             case Stmt_Classes: for (size_t m = 0; m < s->data.classes.methods_count; m++) infer_body(s->data.classes.methods[m].body, s->data.classes.methods[m].body_count, reg); break;
            case Stmt_Matchs:
                for (size_t c = 0; c < s->data.matchs.cases_count; c++) infer_body(s->data.matchs.cases[c].body, s->data.matchs.cases[c].body_count, reg);
                if (s->data.matchs.default_body_count > 0) infer_body(s->data.matchs.default_body, s->data.matchs.default_body_count, reg);
                break;


            default: break;
        }
    }
}


void type_infer_pass(Stmts* body, size_t count, Register* reg) {
    for (size_t i = 0; i < count; i++) if (body[i].tag == Stmt_Functions) infer_func(&body[i], reg);

    infer_body(body, count, reg);
}