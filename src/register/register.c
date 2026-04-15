#include "import.h"
#include "register.h"
#include "type.h"


bool register_expr(Exprs* expr, Register* reg, CheckerErrList* errors);
bool register_body(Stmts* body, size_t count, Register* reg, CheckerErrList* errors);
void resolve_operations(Exprs* expr, Register* reg, CheckerErrList* errors);

static inline StringView sv_from_cstr(const char* s) {
    return (StringView){ .ptr = s, .len = s ? strlen(s) : 0 };
}

static inline StringView type_tag_to_view(Type t) {
    switch (t.tag) {
        case Type_Int:    return sv_from_cstr(t.data.int_t.bits == 64 ? "i64" : "i32");
        case Type_Float:  return sv_from_cstr(t.data.float_t.bits == 64 ? "f64" : "f32");
        case Type_Bool:   return sv_from_cstr("bool");
        case Type_Char:   return sv_from_cstr("char");
        case Type_Str:    return sv_from_cstr("str");
        case Type_Void:   return sv_from_cstr("void");
        case Type_Custom: return string_view_from_range(t.data.custom.name);
        default:          return sv_from_cstr("unknown");
    }
}

StringView string_view_from_range(SourceRange range) {
    return (StringView){
        .ptr = range.start,
        .len = (size_t)(range.end - range.start),
    };
}

static char* null_term_view_alloc(StringView name) {
    char* out = malloc(name.len + 1);
    memcpy(out, name.ptr, name.len);
    out[name.len] = '\0';
    return out;
}

Register register_new(Register* parent, IDCounter* counter) {
    return (Register){ 
        .table   = register_table_init(), 
        .pending = pending_table_init(),
        .parent  = parent, 
        .counter = counter 
    };
}

void register_free(Register* reg) {
    if (!reg->table) return;

    khint_t it;
    kh_foreach(reg->table, it) {
        free(kh_val(reg->table, it).name);
    }

    register_table_destroy(reg->table);
    pending_table_destroy(reg->pending);
    reg->table   = NULL;
    reg->pending = NULL;
}

void register_insert(Register* reg, StringView name, RegisterEntry entry) {
    khint_t pit = pending_table_get(reg->pending, name);
    int absent = 0;

    if (pit != kh_end(reg->pending)) { entry.eid = kh_val(reg->pending, pit); pending_table_del(reg->pending, pit); } else { entry.eid = (EntityID){ .id = reg->counter->next_id++, .kind = entry.tag }; }
    khint_t it = register_table_put(reg->table, name, &absent);

    if (!absent) {
        if (entry.name && entry.name != kh_val(reg->table, it).name) free(entry.name);
        entry.name = kh_val(reg->table, it).name;
        kh_val(reg->table, it) = entry;
        return;
    }

    if (!entry.name) entry.name = null_term_view_alloc(name);
    kh_key(reg->table, it) = (StringView){ .ptr = entry.name, .len = strlen(entry.name) };
    kh_val(reg->table, it) = entry;
}


RegisterEntry* register_get(Register* reg, StringView name) {
    for (Register* r = reg; r != NULL; r = r->parent) {
        khint_t it = register_table_get(r->table, name);
        if (it != kh_end(r->table)) return &kh_val(r->table, it);
    }
    return NULL;
}


Type resolve_type(SourceRange r, Register* reg) {
    size_t len = r.end - r.start;

    if (len == 3 && memcmp(r.start, "i32",  3) == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits   = 32 };
    if (len == 3 && memcmp(r.start, "i64",  3) == 0) return (Type){ .tag = Type_Int,   .data.int_t.bits   = 64 };
    if (len == 3 && memcmp(r.start, "f32",  3) == 0) return (Type){ .tag = Type_Float, .data.float_t.bits = 32 };
    if (len == 3 && memcmp(r.start, "f64",  3) == 0) return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };
    if (len == 4 && memcmp(r.start, "bool", 4) == 0) return (Type){ .tag = Type_Bool };
    if (len == 4 && memcmp(r.start, "char", 4) == 0) return (Type){ .tag = Type_Char };
    if (len == 3 && memcmp(r.start, "str",  3) == 0) return (Type){ .tag = Type_Str  };

    RegisterEntry* entry = register_get(reg, string_view_from_range(r));

    if (entry) {
        switch (entry->tag) {
            case Reg_Class:  return (Type){ .tag = Type_Custom, .data.custom.name = r };
            case Reg_Struct: return (Type){ .tag = Type_Custom, .data.custom.name = r };
            case Reg_Enum:   return (Type){ .tag = Type_Custom, .data.custom.name = r };
            default: break;
        }
    }

    return (Type){ .tag = Type_Void };
}

Type infer_literal_type(SourceRange range) {
    size_t len = range.end - range.start;
    if (len == 0) return (Type){ .tag = Type_Void };
    if ((len == 4 && memcmp(range.start, "true",  4) == 0) ||
        (len == 5 && memcmp(range.start, "false", 5) == 0))
        return (Type){ .tag = Type_Bool };
    if (range.start[0] == '"')  return (Type){ .tag = Type_Str  };
    if (range.start[0] == '\'') return (Type){ .tag = Type_Char };

    bool has_dot = false;
    for (size_t i = 0; i < len; i++) {
        if (range.start[i] == '.') { has_dot = true; break; }
    }

    if (has_dot) return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };
    return (Type){ .tag = Type_Int, .data.int_t.bits = 32 };
}

Type infer_expr_type(Exprs* expr, Register* reg) {
    if (!expr) return (Type){ .tag = Type_Void };

    switch (expr->tag) {
        case Expr_Literals: return infer_literal_type(expr->data.literals.range);

        case Expr_Identifiers:
        case Expr_Vars: {
            SourceRange r = (expr->tag == Expr_Identifiers) ? expr->data.identifiers.name : expr->data.vars.name;
            RegisterEntry* entry = register_get(reg, string_view_from_range(r));
            if (entry) return entry->type;
            return (Type){ .tag = Type_Void };
        }

        case Expr_BinaryOps: {
            LexerTokenTag op = expr->data.binary_ops.op;
            if (op == DoubleEqualss   || op == NotEqualss     ||
                op == Lesses          || op == Greaters        ||
                op == LessEqualss     || op == GreaterEqualss  ||
                op == Ands            || op == Ors)
                return (Type){ .tag = Type_Bool };
            return infer_expr_type(expr->data.binary_ops.left, reg);
        }

        case Expr_Function: {
            RegisterEntry* entry = register_get(reg, string_view_from_range(expr->data.function_call.name));
            if (entry && entry->tag == Reg_Function) return entry->data.function.return_type;
            return (Type){ .tag = Type_Void };
        }

        case Expr_MethodCalls: {
            return (Type){ .tag = Type_Void };
        }

        case Expr_Class_Calls:
        case Expr_Struct_Calls: {
            SourceRange r = (expr->tag == Expr_Class_Calls)
                ? expr->data.class_calls.name
                : expr->data.struct_calls.name;
            return (Type){ .tag = Type_Custom, .data.custom.name = r };
        }

        default:
            return (Type){ .tag = Type_Void };
    }
}

EntityID register_call(Register* reg, StringView name, RegisterEntryTag kind) {
    int absent = 0;

    RegisterEntry* entry = register_get(reg, name);
    if (entry) return entry->eid;

    khint_t pit = pending_table_get(reg->pending, name);
    if (pit != kh_end(reg->pending)) return kh_val(reg->pending, pit);

    EntityID eid = (EntityID){ .id = reg->counter->next_id++, .kind = kind };
    khint_t it = pending_table_put(reg->pending, name, &absent);

    kh_val(reg->pending, it) = eid;
    return eid;
}


bool register_class(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_view_from_range(stmt->data.classes.name);
    RegisterEntry* existing = register_get(reg, key);

    if (existing) {
        bool allowed = existing->tag == Reg_Struct || existing->tag == Reg_Enum;
        if (!allowed) {
            checker_err_push(errors, (CheckerErr){
                .tag      = Err_Tag_RDL,
                .data.rdl = {
                    .range               = stmt->data.classes.range,
                    .var_name            = sv_from_cstr(existing->name),
                }
            });
            return true;
        }

        stmt->data.classes.attached_tag = existing->tag == Reg_Struct ? ClassAttach_Struct : ClassAttach_Enum;
        stmt->data.classes.attached_fields       = existing->data.strct.fields;
        stmt->data.classes.attached_fields_count = existing->data.strct.fields_count;
    }

    register_insert(reg, key, (RegisterEntry){
        .tag  = Reg_Class,
        .name = NULL,
        .type = (Type){ .tag = Type_Custom, .data.custom.name = stmt->data.classes.name },
        .data.class = {
            .methods               = stmt->data.classes.methods,
            .methods_count         = stmt->data.classes.methods_count,
            .has_attached          = existing != NULL,
            .attached_tag          = stmt->data.classes.attached_tag,
            .attached_fields       = stmt->data.classes.attached_fields,
            .attached_fields_count = stmt->data.classes.attached_fields_count,
        }
    });

    return true;
}

bool register_var(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_view_from_range(stmt->data.vars.name);
    RegisterEntry* existing = register_get(reg, key);

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag      = Err_Tag_RDL,
            .data.rdl = {
                .range               = stmt->data.vars.range,
                .var_name            = sv_from_cstr(existing->name),
            }
        });
        return false;
    }

    Type t = (stmt->data.vars.c_type.start != stmt->data.vars.c_type.end)
        ? resolve_type(stmt->data.vars.c_type, reg)
        : infer_expr_type(&stmt->data.vars.value, reg);

    if (stmt->data.vars.has_value && stmt->data.vars.c_type.start != stmt->data.vars.c_type.end) {
        Type vt = infer_expr_type(&stmt->data.vars.value, reg);
        if (t.tag != vt.tag) {
            checker_err_push(errors, (CheckerErr){
                .tag      = Err_Tag_VMV,
                .data.vmv = {
                    .range         = stmt->data.vars.range,
                    .var_name      = key,
                    .expected_type = type_tag_to_view(t),
                    .actual_type   = type_tag_to_view(vt),
                }
            });
            return false;
        }
    }

    register_insert(reg, key, (RegisterEntry){
        .tag      = Reg_Var,
        .name     = NULL,
        .type     = t,
        .data.var = { .type = t, .mode = stmt->data.vars.mode, .is_mut = true }
    });
    return true;
}

bool register_let(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_view_from_range(stmt->data.lets.name);
    RegisterEntry* existing = register_get(reg, key);

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag      = Err_Tag_RDL,
            .data.rdl = {
                .range               = stmt->data.lets.range,
                .var_name            = sv_from_cstr(existing->name),
            }
        });
        return false;
    }

    Type t = (stmt->data.lets.c_type.start != stmt->data.lets.c_type.end)
        ? resolve_type(stmt->data.lets.c_type, reg)
        : infer_expr_type(&stmt->data.lets.value, reg);

    if (stmt->data.lets.has_value && stmt->data.lets.c_type.start != stmt->data.lets.c_type.end) {
        Type vt = infer_expr_type(&stmt->data.lets.value, reg);
        if (t.tag != vt.tag) {
            checker_err_push(errors, (CheckerErr){
                .tag      = Err_Tag_VMV,
                .data.vmv = {
                    .range         = stmt->data.lets.range,
                    .var_name      = key,
                    .expected_type = type_tag_to_view(t),
                    .actual_type   = type_tag_to_view(vt),
                }
            });
            return false;
        }
    }

    register_insert(reg, key, (RegisterEntry){
        .tag      = Reg_Let,
        .name     = NULL,
        .type     = t,
        .data.let = { .type = t, .mode = stmt->data.lets.mode }
    });
    return true;
}

bool register_const(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_view_from_range(stmt->data.consts.name);
    RegisterEntry* existing = register_get(reg, key);

    if (!stmt->data.consts.has_value) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_CVN,
            .data.cvn = {
                .range = stmt->data.consts.range,
                .var_name = key,
            }
        });
        return false;
    }

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_RDL,
            .data.rdl = {
                .range               = stmt->data.consts.range,
                .var_name            = sv_from_cstr(existing->name),
            }
        });
        return false;
    }

    Type t = (stmt->data.consts.c_type.start != stmt->data.consts.c_type.end)
        ? resolve_type(stmt->data.consts.c_type, reg)
        : infer_expr_type(&stmt->data.consts.value, reg);

    register_insert(reg, key, (RegisterEntry){
        .tag         = Reg_Const,
        .name        = NULL,
        .type        = t,
        .data.const_ = { .type = t, .is_pub = stmt->data.consts.is_pub }
    });
    return true;
}

bool register_local(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_view_from_range(stmt->data.locals.name);
    RegisterEntry* existing = register_get(reg, key);

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag      = Err_Tag_RDL,
            .data.rdl = {
                .range               = stmt->data.locals.range,
                .var_name            = sv_from_cstr(existing->name),
            }
        });
        return false;
    }

    Type t = resolve_type(stmt->data.locals.c_type, reg);
    if (t.tag == Type_Void) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_TNF,
            .data.tnf = {
                .range     = stmt->data.locals.range,
                .type_name = key,
            }
        });
        return false;
    }

    register_insert(reg, key, (RegisterEntry){
        .tag        = Reg_Local,
        .name       = NULL,
        .type       = t,
        .data.local = { .type = t, .is_pub = stmt->data.locals.is_pub }
    });
    return true;
}


bool register_expr(Exprs* expr, Register* reg, CheckerErrList* errors) {
    if (!expr) return true;
    switch (expr->tag) {
        case Expr_Function: {
            StringView name = string_view_from_range(expr->data.function_call.name);
            register_call(reg, name, Reg_Function);
            return true;
        }
        case Expr_Class_Calls: {
            StringView name = string_view_from_range(expr->data.class_calls.name);
            register_call(reg, name, Reg_Class);
            return true;
        }

        case Expr_Struct_Calls: {
            StringView name = string_view_from_range(expr->data.struct_calls.name);
            register_call(reg, name, Reg_Struct);
            return true;
        }

        default: return true;
    }
}

bool register_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    if (!stmt) return true;
    switch (stmt->tag) {
        case Stmt_Vars: return register_var(stmt, reg, errors);
        case Stmt_Lets: return register_let(stmt, reg, errors);
        case Stmt_Consts: return register_const(stmt, reg, errors);
        case Stmt_Classes: return register_class(stmt, reg, errors);
        case Stmt_Functions: {
            StringView key = string_view_from_range(stmt->data.functions.name);

            register_insert(reg, key, (RegisterEntry){
                .tag  = Reg_Function,
                .name = NULL,
                .type = resolve_type(stmt->data.functions.return_type, reg),
                .data.function = {
                    .params       = stmt->data.functions.params,
                    .params_count = stmt->data.functions.params_count,
                    .return_type  = resolve_type(stmt->data.functions.return_type, reg),
                    .is_pub       = stmt->data.functions.is_pub,
                    .is_unsafe    = stmt->data.functions.is_unsafe,
                }
            });

            Register child = register_new(reg, reg->counter);

            for (size_t i = 0; i < stmt->data.functions.params_count; i++) {
                Param* p   = &stmt->data.functions.params[i];
                Type t   = resolve_type(p->c_type, reg);
                register_insert(&child, string_view_from_range(p->name), (RegisterEntry){
                    .tag  = Reg_Var,
                    .name = NULL,
                    .type = t,
                    .data.var = { .type = t, .is_mut = false }
                });
            }
            bool ok = register_body(stmt->data.functions.body, stmt->data.functions.body_count, &child, errors);
            register_free(&child);
            return ok;
        }

        case Stmt_ExprStmt: { return register_expr(&stmt->data.expr_stmt.expr, reg, errors); }
        case Stmt_Returns: { return register_expr(&stmt->data.returns.expr, reg, errors); }
        case Stmt_Assigns: {
            Exprs op_expr = {
                .tag = Expr_BinaryOps,
                .data.binary_ops = {
                    .left  = &stmt->data.assigns.target,
                    .right = &stmt->data.assigns.value,
                    .op    = stmt->data.assigns.op,
                }
            };
            resolve_operations(&op_expr, reg, errors);
            return true;
        }
        default: return true;
    }

}


bool register_body(Stmts* body, size_t count, Register* reg, CheckerErrList* errors) {
    bool ok = true;
    for (size_t i = 0; i < count; i++) ok = register_stmt(&body[i], reg, errors) && ok;
    return ok;
}

