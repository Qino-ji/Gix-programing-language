#include "import.h"
#include "register.h"
#include "ir.h"
#include "footprint.h"
#include "helper.h"

bool register_expr(Exprs* expr, Register* reg, CheckerErrList* errors);
void check_expr(Exprs* expr, Register* reg, CheckerErrList* errors);
void resolve_operations(Exprs* expr, Register* reg, CheckerErrList* errors);
Type infer_expr_type(Exprs* expr, Register* reg);
bool register_class(Stmts* stmt, Register* reg, CheckerErrList* errors);
StringView string(const char* s);
void check_expr(Exprs* expr, Register* reg, CheckerErrList* errors);
void check_if_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors, Exprs* parent_cond);
void check_while_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors, Exprs* parent_cond);
void check_for_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_match_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_vars_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_lets_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_consts_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_locals_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_assign_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_return_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors, SourceRange fn_return_type);
void check_extern_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_function_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_struct_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_enum_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_class_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
void check_trait_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors);
StringView string_range(SourceRange range);
void check_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors, SourceRange fn_return_type, Exprs* parent_cond, FuncBodyList* bodies);
void register_free(Register* reg);
char* mangle_name(StringView base, GenericBinding* bindings, size_t count);
Type range_to_type(SourceRange r, Register* reg);
void maybe_resolve_generic(SourceRange name, SourceRange* generic_args, size_t generic_count, Register* reg, CheckerErrList* errors);
SourceRange mangle_method_name(SourceRange class_name, SourceRange method_name);


StringView string_range(SourceRange range) {
    return (StringView){    
        .ptr = range.start,
        .len = (size_t)(range.end - range.start),
    };
}

StringView sv(const char* s) {
    return (StringView){ .ptr = s, .len = s ? strlen(s) : 0 };
}

static inline StringView type_tag_to_view(Type t) {
    switch (t.tag) {
        case Type_Int:    return sv(t.data.int_t.bits == 64 ? "i64" : "i32");
        case Type_Float:  return sv(t.data.float_t.bits == 64 ? "f64" : "f32");
        case Type_Bool:   return sv("bool");
        case Type_Char:   return sv("char");
        case Type_Str:    return sv("str");
        case Type_Void:   return sv("void");
        case Type_Custom: return string_range(t.data.custom.name);
        default:          return sv("unknown");
    }
}

static bool expr_exists(Exprs expr) {
    return expr.data.literals.range.start != NULL;
}


static char* null_term_view_alloc(StringView name) {
    char* out = malloc(name.len + 1);
    memcpy(out, name.ptr, name.len);
    out[name.len] = '\0';
    return out;
}

Register register_new(Register* parent, IDCounter* counter, GenericRegistry* mono) {
    return (Register){
        .table = register_table_init(),
        .pending = pending_table_init(),
        .parent = parent,
        .counter = counter,
        .mono = mono,
    };
}

uint32_t register_fresh_id(Register* reg) {
    return reg->counter->next_id++;
}

void register_free(Register* reg) {
    if (!reg->table) return;

    khint_t it;
    kh_foreach(reg->table, it) {
        free(kh_val(reg->table, it).name);
    }

    register_table_destroy(reg->table);
    pending_table_destroy(reg->pending);
    reg->table = NULL;
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


void insert_param(Register* reg, Param* p, Type t) {
    StringView key = (StringView){
        .ptr = p->name.start,
        .len = (size_t)(p->name.end - p->name.start),
    };
    register_insert(reg, key, (RegisterEntry){
        .tag = Reg_Var,
        .name = NULL,
        .type = t,
        .decl_range = p->name,
        .decl_name_range = p->name,
        .data.var = { .type = t, .mode = p->mode, .is_mut = false }
    });
}

void populate_register(Stmts* body, size_t body_count, Register* reg, CheckerErrList* errors) {
    for (size_t i = 0; i < body_count; i++) {
        Stmts* stmt = &body[i];

        switch (stmt->tag) {
            case Stmt_Vars: {
                StringView key = string_range(stmt->data.vars.name);
                khint_t it = register_table_get(reg->table, key);
                RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;
                Type t = (stmt->data.vars.c_type.start != stmt->data.vars.c_type.end || stmt->data.vars.type_tree) ? resolve_type_tree(stmt->data.vars.c_type, stmt->data.vars.type_tree, reg) : (Type){ .tag = Type_Void };

                if (existing) {
                    checker_err_push(errors, (CheckerErr){
                        .tag = Err_Tag_RDL,
                        .data.rdl = { .range = stmt->data.vars.range, .var_name = sv(existing->name) }
                    });
                    break;
                }

                if (expr_exists(stmt->data.vars.value) && (stmt->data.vars.c_type.start != stmt->data.vars.c_type.end || stmt->data.vars.type_tree)) {
                    Type value_type = infer_expr_type(&stmt->data.vars.value, reg);
                    if (t.tag != value_type.tag) {
                        checker_err_push(errors, (CheckerErr){
                            .tag = Err_Tag_VMV,
                            .data.vmv = {
                                .range = stmt->data.vars.range,
                                .var_name = key,
                                .expected_type = type_tag_to_view(t),
                                .actual_type = type_tag_to_view(value_type),
                            }
                        });
                        break;
                    }
                }

                register_insert(reg, key, (RegisterEntry){
                    .tag = Reg_Var,
                    .name = NULL,
                    .type = t,
                    .decl_range = stmt->data.vars.range,
                    .decl_name_range = stmt->data.vars.name,
                    .data.var = { .type = t, .mode = stmt->data.vars.mode, .is_mut = true }
                });
                break;
            }

            case Stmt_Functions: {
                StringView key = string_range(stmt->data.functions.name);
                Type ret = resolve_type_tree(stmt->data.functions.return_type, stmt->data.functions.return_type_tree, reg);

                register_insert(reg, key, (RegisterEntry){
                    .tag = Reg_Function,
                    .name = NULL,
                    .type = ret,
                    .data.function = {
                        .return_type = ret,
                        .params = stmt->data.functions.params,
                        .params_count = stmt->data.functions.params_count,
                        .is_pub = stmt->data.functions.is_pub,
                    }
                });
                break;
            }

            case Stmt_Lets: {
                StringView key = string_range(stmt->data.lets.name);
                khint_t it = register_table_get(reg->table, key);
                RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;
            
                if (existing) {
                    checker_err_push(errors, (CheckerErr){
                        .tag = Err_Tag_RDL,
                        .data.rdl = { .range = stmt->data.lets.range, .var_name = sv(existing->name) }
                    });
                    break;
                }

                Type t = (stmt->data.lets.c_type.start != stmt->data.lets.c_type.end || stmt->data.lets.type_tree) ?
                    resolve_type_tree(stmt->data.lets.c_type, stmt->data.lets.type_tree, reg) :
                    infer_expr_type(&stmt->data.lets.value, reg);

                if (expr_exists(stmt->data.lets.value) && (stmt->data.lets.c_type.start != stmt->data.lets.c_type.end || stmt->data.lets.type_tree)) {
                    Type value_type = infer_expr_type(&stmt->data.lets.value, reg);
                    if (t.tag != value_type.tag) {
                        checker_err_push(errors, (CheckerErr){
                            .tag = Err_Tag_VMV,
                            .data.vmv = {
                                .range = stmt->data.lets.range,
                                .var_name = key,
                                .expected_type = type_tag_to_view(t),
                                .actual_type = type_tag_to_view(value_type),
                            }
                        });
                        break;
                    }
                }

                register_insert(reg, key, (RegisterEntry){
                    .tag = Reg_Let,
                    .name = NULL,
                    .type = t,
                    .decl_range = stmt->data.lets.range,
                    .decl_name_range = stmt->data.lets.name,
                    .data.let = { .type = t, .mode = stmt->data.lets.mode }
                });
                break;
            }

            case Stmt_Consts: {
                StringView key = string_range(stmt->data.consts.name);

                if (!expr_exists(stmt->data.consts.value)) {
                    checker_err_push(errors, (CheckerErr){
                        .tag = Err_Tag_CVN,
                        .data.cvn = { .range = stmt->data.consts.range, .var_name = key }
                    });
                    break;
                }

                khint_t it = register_table_get(reg->table, key);
                RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;
                if (existing) {
                    checker_err_push(errors, (CheckerErr){
                        .tag = Err_Tag_RDL,
                        .data.rdl = { .range = stmt->data.consts.range, .var_name = sv(existing->name) }
                    });
                    break;
                }

                Type t = (stmt->data.consts.c_type.start != stmt->data.consts.c_type.end || stmt->data.consts.type_tree) ?
                    resolve_type_tree(stmt->data.consts.c_type, stmt->data.consts.type_tree, reg) :
                    infer_expr_type(&stmt->data.consts.value, reg);

                register_insert(reg, key, (RegisterEntry){
                    .tag = Reg_Const,
                    .name = NULL,
                    .type = t,
                    .decl_range = stmt->data.consts.range,
                    .decl_name_range = stmt->data.consts.name,
                    .data.const_ = { .type = t, .is_pub = stmt->data.consts.is_pub }
                });
                break;
            }

            case Stmt_Locals: {
                StringView key = string_range(stmt->data.locals.name);
                khint_t it = register_table_get(reg->table, key);
                RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;
                if (existing) {
                    checker_err_push(errors, (CheckerErr){
                        .tag = Err_Tag_RDL,
                        .data.rdl = { .range = stmt->data.locals.range, .var_name = sv(existing->name) }
                    });
                    break;
                }

                Type t = resolve_type_tree(stmt->data.locals.c_type, stmt->data.locals.type_tree, reg);
                if (t.tag == Type_Void) {
                    checker_err_push(errors, (CheckerErr){
                        .tag = Err_Tag_TNF,
                        .data.tnf = { .range = stmt->data.locals.range, .type_name = key }
                    });
                    break;
                }

                register_insert(reg, key, (RegisterEntry){
                    .tag = Reg_Local,
                    .name = NULL,
                    .type = t,
                    .decl_range = stmt->data.locals.range,
                    .decl_name_range = stmt->data.locals.name,
                    .data.local = { .type = t, .is_pub = stmt->data.locals.is_pub }
                });
                break;
            }

            default: break;
        }
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

EntityID register_call(Register* reg, StringView name, RegisterEntryTag kind) {
    int absent = 0;

    RegisterEntry* entry = register_get(reg, name); if (entry) return entry->eid;
    khint_t pit = pending_table_get(reg->pending, name); if (pit != kh_end(reg->pending)) return kh_val(reg->pending, pit);
    EntityID eid = (EntityID){ .id = reg->counter->next_id++, .kind = kind };
    khint_t it = pending_table_put(reg->pending, name, &absent);

    kh_val(reg->pending, it) = eid;
    return eid;
}


bool register_class(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_range(stmt->data.classes.name);
    khint_t it = register_table_get(reg->table, key);
    RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;
    size_t clen = stmt->data.classes.name.end - stmt->data.classes.name.start;
    FunctionMethod* method = stmt->data.classes.methods;
    FunctionMethod* end_method = method + stmt->data.classes.methods_count;

    method = stmt->data.classes.methods;
    while (method < end_method) {
        SourceRange mangled = mangle_method_name(stmt->data.classes.name, method->name); method->name = mangled;
        StringView mkey = (StringView){ mangled.start, (size_t)(mangled.end - mangled.start) };
        Type ret = resolve_type_tree(method->return_type, method->return_type_tree, reg);

        register_insert(reg, mkey, (RegisterEntry){
            .tag  = Reg_Function,
            .name = NULL,
            .type = ret,
            .data.function = {
                .return_type  = ret,
                .params       = method->params,
                .params_count = method->params_count,
                .is_pub       = method->is_pub,
            }
        });
        fprintf(stderr, "[REG] method registered: '%.*s'\n",
            (int)(mangled.end - mangled.start), mangled.start);
        method++;
    }

    if (existing) {
        bool allowed = existing->tag == Reg_Struct || existing->tag == Reg_Enum;
        if (!allowed) {
            checker_err_push(errors, (CheckerErr){
                .tag = Err_Tag_RDL,
                .data.rdl = {
                    .range = stmt->data.classes.range,
                    .var_name = sv(existing->name),
                }
            });
            return true;
        }

        stmt->data.classes.attached_tag = existing->tag == Reg_Struct ? ClassAttach_Struct : ClassAttach_Enum;
        stmt->data.classes.attached_fields = existing->data.strct.fields;
        stmt->data.classes.attached_fields_count = existing->data.strct.fields_count;
    }



    register_insert(reg, key, (RegisterEntry){
    .tag  = Reg_Class,
    .name = NULL,
    .type = (Type){ .tag = Type_Custom, .data.custom.name = stmt->data.classes.name },
    .data._class = {
        .methods = stmt->data.classes.methods,
        .methods_count = stmt->data.classes.methods_count,
        .attached_tag = stmt->data.classes.attached_tag,
        .attached_fields = stmt->data.classes.attached_fields,
        .attached_fields_count = stmt->data.classes.attached_fields_count,
        .generic_params = stmt->data.classes.generic_params,
        .generic_params_count  = stmt->data.classes.generic_params_count,
    }
});
 

    return true;
}

bool register_var(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_range(stmt->data.vars.name);
    khint_t it = register_table_get(reg->table, key);
    RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_RDL,
            .data.rdl = { 
                .range = stmt->data.vars.range, 
                .var_name = sv(existing->name) 
            }
        });
        return false;
    }

    Type t;
    if (stmt->data.vars.c_type.start != stmt->data.vars.c_type.end || stmt->data.vars.type_tree) {
        t = get_type(stmt->data.vars.c_type, stmt->data.vars.type_tree, reg);
    } else {
        t = infer_expr_type(&stmt->data.vars.value, reg);
    }

    if (expr_exists(stmt->data.vars.value) && (stmt->data.vars.c_type.start != stmt->data.vars.c_type.end || stmt->data.vars.type_tree)) {
        Type vt = infer_expr_type(&stmt->data.vars.value, reg);
        if (t.tag != vt.tag) {
            checker_err_push(errors, (CheckerErr){
                .tag = Err_Tag_VMV,
                .data.vmv = { 
                    .range = stmt->data.vars.range, 
                    .var_name = key, 
                    .expected_type = type_tag_to_view(t), 
                    .actual_type = type_tag_to_view(vt) 
                }
            });
            return false;
        }
    }

      register_insert(reg, key, (RegisterEntry){
          .tag = Reg_Var,
          .name = NULL,
          .type = t,
          .decl_range = stmt->data.vars.range,
          .decl_name_range = stmt->data.vars.name,
          .data.var = { .type = t, .mode = stmt->data.vars.mode, .is_mut = true }
      });
      return true;
  }

bool register_let(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_range(stmt->data.lets.name);
    khint_t it = register_table_get(reg->table, key);
    RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_RDL,
            .data.rdl = { 
                .range = stmt->data.lets.range, 
                .var_name = sv(existing->name) 
            }
        });
        return false;
    }

    Type t;
    if (stmt->data.lets.c_type.start != stmt->data.lets.c_type.end || stmt->data.lets.type_tree) {
        t = resolve_type_tree(stmt->data.lets.c_type, stmt->data.lets.type_tree, reg);
    } else {
        t = infer_expr_type(&stmt->data.lets.value, reg);
    }

    if (expr_exists(stmt->data.lets.value) && (stmt->data.lets.c_type.start != stmt->data.lets.c_type.end || stmt->data.lets.type_tree)) {
        Type vt = infer_expr_type(&stmt->data.lets.value, reg);
        if (t.tag != vt.tag) {
            checker_err_push(errors, (CheckerErr){
                .tag = Err_Tag_VMV,
                .data.vmv = { 
                    .range = stmt->data.lets.range, 
                    .var_name = key, 
                    .expected_type = type_tag_to_view(t), 
                    .actual_type = type_tag_to_view(vt) 
                }
            });
            return false;
        }
    }

      register_insert(reg, key, (RegisterEntry){
          .tag = Reg_Let,
          .name = NULL,
          .type = t,
          .decl_range = stmt->data.lets.range,
          .decl_name_range = stmt->data.lets.name,
          .data.let = { .type = t, .mode = stmt->data.lets.mode }
      });
      return true;
  }

bool register_const(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_range(stmt->data.consts.name);
    khint_t it = register_table_get(reg->table, key);
    RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;

    if (!expr_exists(stmt->data.consts.value)) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_CVN,
            .data.cvn = { 
                .range = stmt->data.consts.range, 
                .var_name = key 
            }
        });
        return false;
    }

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_RDL,
            .data.rdl = { 
                .range = stmt->data.consts.range, 
                .var_name = sv(existing->name) 
            }
        });
        return false;
    }

    Type t = (stmt->data.consts.c_type.start != stmt->data.consts.c_type.end || stmt->data.consts.type_tree) ?
        resolve_type_tree(stmt->data.consts.c_type, stmt->data.consts.type_tree, reg) :
        infer_expr_type(&stmt->data.consts.value, reg);

        register_insert(reg, key, (RegisterEntry){
          .tag = Reg_Const,
          .name = NULL,
          .type = t,
          .decl_range = stmt->data.consts.range,
          .decl_name_range = stmt->data.consts.name,
          .data.const_ = { .type = t, .is_pub = stmt->data.consts.is_pub }
      });
      return true;
  }

bool register_local(Stmts* stmt, Register* reg, CheckerErrList* errors) {
    StringView key = string_range(stmt->data.locals.name);
    khint_t it = register_table_get(reg->table, key);
    RegisterEntry* existing = (it != kh_end(reg->table)) ? &kh_val(reg->table, it) : NULL;

    if (existing) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_RDL,
            .data.rdl = {
                .range = stmt->data.locals.range,
                .var_name = sv(existing->name),
            }
        });
        return false;
    }

    Type t = resolve_type_tree(stmt->data.locals.c_type, stmt->data.locals.type_tree, reg);
    if (t.tag == Type_Void) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_TNF,
            .data.tnf = {
                .range = stmt->data.locals.range,
                .type_name = key,
            }
        });
        return false;
    }

      register_insert(reg, key, (RegisterEntry){
          .tag = Reg_Local,
          .name = NULL,
          .type = t,
          .decl_range = stmt->data.locals.range,
          .decl_name_range = stmt->data.locals.name,
          .data.local = { .type = t, .is_pub = stmt->data.locals.is_pub }
      });
      return true;
  }

bool register_expr(Exprs* expr, Register* reg, CheckerErrList* errors) {
    if (!expr) return true;

    switch (expr->tag) {
        case Expr_Literals:
        case Expr_Vars:
        case Expr_Identifiers:
            return true;

        case Expr_BinaryOps:
            register_expr(expr->data.binary_ops.left,  reg, errors);
            register_expr(expr->data.binary_ops.right, reg, errors);
            return true;

        case Expr_MethodCalls: {
            register_expr(expr->data.method_calls.object, reg, errors);
            for (size_t i = 0; i < expr->data.method_calls.args_count; i++) {
                register_expr(&expr->data.method_calls.args[i], reg, errors);
            }
            return true;
        }

        case Expr_Function: {
            StringView name = string_range(expr->data.function_call.name);
            register_call(reg, name, Reg_Function);
            for (size_t i = 0; i < expr->data.function_call.param_count; i++) {
                StringView arg = string_range(expr->data.function_call.param[i].name);
                register_call(reg, arg, Reg_Var);
            }
            return true;
        }

        case Expr_Class_Calls: {
            StringView name = string_range(expr->data.class_calls.name);
            register_call(reg, name, Reg_Class);
            for (size_t i = 0; i < expr->data.class_calls.param_count; i++) {
                StringView arg = string_range(expr->data.class_calls.param[i].name);
                register_call(reg, arg, Reg_Var);
            }
            return true;
        }

        case Expr_Struct_Calls: {
            StringView name = string_range(expr->data.struct_calls.name);
            register_call(reg, name, Reg_Struct);
            for (size_t i = 0; i < expr->data.struct_calls.param_count; i++) {
                StringView arg = string_range(expr->data.struct_calls.param[i].name);
                register_call(reg, arg, Reg_Var);
            }
            return true;
        }

        case Expr_Enum_Calls: {
            StringView name = string_range(expr->data.enum_calls.name);
            register_call(reg, name, Reg_Enum);
            for (size_t i = 0; i < expr->data.enum_calls.param_count; i++) {
                StringView arg = string_range(expr->data.enum_calls.param[i].name);
                register_call(reg, arg, Reg_Var);
            }
            return true;
        }

        default: return true;
    }
}

static void check_body(Stmts* body, size_t count, Register* reg, CheckerErrList* errors, SourceRange fn_return_type, Exprs* parent_cond, FuncBodyList* bodies) {
    populate_register(body, count, reg, errors);

    for (size_t i = 0; i < count; i++) check_stmt(&body[i], reg, errors, fn_return_type, parent_cond, bodies);
}
 

void check_stmt(Stmts* stmt, Register* reg, CheckerErrList* errors, SourceRange fn_return_type, Exprs* parent_cond, FuncBodyList* bodies) {
    if (!stmt) return;

    switch (stmt->tag) {
        case Stmt_Vars:    check_vars_stmt(stmt, reg, errors);   break;
        case Stmt_Lets:    check_lets_stmt(stmt, reg, errors);   break;
        case Stmt_Consts:  check_consts_stmt(stmt, reg, errors); break;
        case Stmt_Locals:  check_locals_stmt(stmt, reg, errors); break;
        case Stmt_Assigns: check_assign_stmt(stmt, reg, errors); break;
        case Stmt_Returns: check_return_stmt(stmt, reg, errors, fn_return_type); break;
        case Stmt_ExprStmt: check_expr(&stmt->data.expr_stmt.expr, reg, errors); break;
        case Stmt_Ifs: {
            check_if_stmt(stmt, reg, errors, parent_cond);

            Register child = register_new(reg, reg->counter, reg->mono);
            check_body(stmt->data.ifs.body, stmt->data.ifs.body_count, &child, errors, fn_return_type, &stmt->data.ifs.cond, bodies);
            register_free(&child);

            if (stmt->data.ifs.else_body_count > 0) {
                Register else_child = register_new(reg, reg->counter, reg->mono);
                check_body(stmt->data.ifs.else_body, stmt->data.ifs.else_body_count, &else_child, errors, fn_return_type, parent_cond, bodies);
                register_free(&else_child);
            }
            break;
        }
        case Stmt_Modules: check_body(stmt->data.modules.body, stmt->data.modules.body_count, reg, errors, fn_return_type, parent_cond, bodies); break;

        case Stmt_Whiles: {
            check_while_stmt(stmt, reg, errors, parent_cond);
            Register child = register_new(reg, reg->counter, reg->mono);
            check_body(stmt->data.whiles.body, stmt->data.whiles.body_count, &child, errors, fn_return_type, &stmt->data.whiles.cond, bodies);
            register_free(&child);
            break;
        }

        case Stmt_Fors: {
            check_for_stmt(stmt, reg, errors);
            Register child = register_new(reg, reg->counter, reg->mono);
            StringView var_sv = string_range(stmt->data.fors._var);
            Type iter_type = infer_expr_type(&stmt->data.fors.iter, reg);
            register_insert(&child, var_sv, (RegisterEntry){
                .tag = Reg_Var,
                .name = NULL,
                .type = iter_type,
                .data.var = { .type = iter_type, .is_mut = false }
            });
            check_body(stmt->data.fors.body, stmt->data.fors.body_count, &child, errors, fn_return_type, NULL, bodies);
            register_free(&child);
            break;
        }

        case Stmt_Matchs: check_match_stmt(stmt, reg, errors); break;
        case Stmt_Externs: {
            ExternFunction* funcs = stmt->data.extern_.funcs;
            size_t funcs_count    = stmt->data.extern_.funcs_count;
            SourceRange abi       = stmt->data.extern_.abi;
            SourceRange ffi       = stmt->data.extern_.ffi;

            
            register_insert(reg, string_range(abi), (RegisterEntry){
                .tag = Reg_Extern,
                .name = NULL,
                .data.extern_ = {
                    .abi = abi,
                    .ffi = ffi,
                    .funcs = funcs,
                    .funcs_count = funcs_count,
                    .is_pub = true,
                }
            });

            for (size_t i = 0; i < funcs_count; i++) {
                ExternFunction* fn = &funcs[i];
                Type ret = resolve_type_tree(fn->return_type, fn->return_type_tree, reg);
                register_insert(reg, string_range(fn->name), (RegisterEntry){
                    .tag = Reg_Function,
                    .name = NULL,
                    .type = ret,
                    .data.function = {
                        .return_type = ret,
                        .params = fn->params,
                        .params_count = fn->params_count,
                        .is_pub = true,
                    }
                });
            }

            check_extern_stmt(stmt, reg, errors);
            break;
        }
                
        case Stmt_Functions: {
            check_function_stmt(stmt, reg, errors);

            FuncBody fb = {0};
            fb.func_chunk = ++bodies->func_counter;

            if (stmt->data.functions.name.start) {
                size_t name_len = stmt->data.functions.name.end - stmt->data.functions.name.start;
                fb.func_name = strndup(stmt->data.functions.name.start, name_len);
            }

            fb.body_reg = malloc(sizeof(Register));
            *fb.body_reg = register_new(reg, reg->counter, reg->mono);

            for (size_t i = 0; i < stmt->data.functions.params_count; i++) {
                Param* p = &stmt->data.functions.params[i];
                Type t = resolve_type_tree(p->c_type, p->type_tree, reg);
                register_insert(fb.body_reg, string_range(p->name), (RegisterEntry){
                    .tag = Reg_Var,
                    .name = NULL,
                    .type = t,
                    .data.var = { .type = t, .is_mut = false }
                });
            }

            check_body(stmt->data.functions.body, stmt->data.functions.body_count, fb.body_reg, errors, stmt->data.functions.return_type, NULL, bodies);

            ARR_PUSH(*bodies, fb);
            size_t idx = bodies->len - 1;
            RegisterEntry* fn_entry = register_get(reg, string_range(stmt->data.functions.name));
            if (fn_entry) fn_entry->data.function.child_reg = bodies->data[idx].body_reg;

            break;
        }


        case Stmt_Structs: check_struct_stmt(stmt, reg, errors); break;
        case Stmt_Enums:   check_enum_stmt(stmt, reg, errors);   break;
        case Stmt_Classes: check_class_stmt(stmt, reg, errors);  break;
        case Stmt_Traits:  check_trait_stmt(stmt, reg, errors);  break;

        default: break;
    }
}
FuncBodyList register_body(Stmts* body, size_t count, Register* reg, CheckerErrList* errors) {
    for (size_t i = 0; i < count; i++) {
        Stmts* s = &body[i];
        switch (s->tag) {
            case Stmt_Structs: {
                StringView key = string_range(s->data.structs.name);
                register_insert(reg, key, (RegisterEntry){
                    .tag  = Reg_Struct,
                    .name = NULL,
                    .type = (Type){ .tag = Type_Custom, .data.custom.name = s->data.structs.name },
                    .data.strct = {
                        .fields = s->data.structs.fields,
                        .fields_count = s->data.structs.fields_count,
                        .is_pub = s->data.structs.is_pub,
                        .generic_params = s->data.structs.generic_params,
                        .generic_params_count = s->data.structs.generic_params_count,
                    }
                });
                break;
            }
            case Stmt_Enums: {
                StringView key = string_range(s->data.enums.name);
                register_insert(reg, key, (RegisterEntry){
                    .tag  = Reg_Enum,
                    .name = NULL,
                    .type = (Type){ .tag = Type_Custom, .data.custom.name = s->data.enums.name },
                    .data.enm = {
                        .variants = s->data.enums.variants,
                        .variants_count = s->data.enums.variants_count,
                        .is_pub = s->data.enums.is_pub,
                        .generic_params = s->data.enums.generic_params,
                        .generic_params_count = s->data.enums.generic_params_count,
                    }
                });
                break;
            }
            case Stmt_Traits: {
                StringView key = string_range(s->data.traits.name);
                register_insert(reg, key, (RegisterEntry){
                    .tag = Reg_Trait,
                    .name = NULL,
                    .type = (Type){ .tag = Type_Custom, .data.custom.name = s->data.traits.name },
                    .data.trait = {
                        .methods = s->data.traits.methods,
                        .methods_count = s->data.traits.methods_count,
                    }
                });
                break;
            }
            case Stmt_Modules:
                register_body(s->data.modules.body, s->data.modules.body_count, reg, errors);
                break;
            default: break;
        }
    }

    for (size_t i = 0; i < count; i++) {
        Stmts* s = &body[i];
        switch (s->tag) {
            case Stmt_Functions: {
                StringView key = string_range(s->data.functions.name);
                Type ret = resolve_type_tree(s->data.functions.return_type, s->data.functions.return_type_tree, reg);
                register_insert(reg, key, (RegisterEntry){
                    .tag  = Reg_Function,
                    .name = NULL,
                    .type = ret,
                    .data.function = {
                        .return_type = ret,
                        .params = s->data.functions.params,
                        .params_count = s->data.functions.params_count,
                        .is_pub = s->data.functions.is_pub,
                        .generic_params = s->data.functions.generic_params,
                        .generic_params_count = s->data.functions.generic_params_count,
                    }
                });
                break;
            }
            case Stmt_Externs: {
                ExternFunction* funcs = s->data.extern_.funcs;
                size_t funcs_count = s->data.extern_.funcs_count;
                SourceRange abi = s->data.extern_.abi;
                SourceRange ffi = s->data.extern_.ffi;
                register_insert(reg, string_range(abi), (RegisterEntry){
                    .tag = Reg_Extern,
                    .name = NULL,
                    .data.extern_ = { .abi = abi, .ffi = ffi, .funcs = funcs, .funcs_count = funcs_count, .is_pub = true }
                });
                for (size_t j = 0; j < funcs_count; j++) {
                    ExternFunction* fn = &funcs[j];
                    Type ret = resolve_type_tree(fn->return_type, fn->return_type_tree, reg);
                    register_insert(reg, string_range(fn->name), (RegisterEntry){
                        .tag = Reg_Function,
                        .name = NULL,
                        .type = ret,
                        .data.function = { .return_type = ret, .params = fn->params, .params_count = fn->params_count, .is_pub = true }
                    });
                }
                break;
            }
            case Stmt_Classes: register_class(s, reg, errors); break;
            default: break;
        }
    }

    SourceRange empty = {0};
    FuncBodyList bodies = {0};
    check_body(body, count, reg, errors, empty, NULL, &bodies);
    return bodies;
}


void check_expr(Exprs* expr, Register* reg, CheckerErrList* errors) {
    if (!expr) return;

    switch (expr->tag) {
        case Expr_Self: {
            RegisterEntry* self_entry = register_get(reg, sv("self"));
            if (!self_entry) {
                checker_err_push(errors, (CheckerErr){
                    .tag = Err_Tag_SOC,
                    .data.soc = { .range = expr->data.self_access.target }
                });
                return;
            }

            if (!expr->data.self_access.is_call) return;

            SourceRange self_sr = (SourceRange){ .start = "self", .end = "self" + 4 };
            Exprs* obj = malloc(sizeof(Exprs));
            *obj = (Exprs){
                .tag = Expr_Identifiers,
                .data.identifiers = {
                    .name  = self_sr,
                    .range = self_sr,
                },
            };

            size_t ac = expr->data.self_access.args_count;
            Exprs* args = NULL;
            if (ac) {
                args = malloc(sizeof(Exprs) * ac);
                for (size_t i = 0; i < ac; i++) args[i] = expr->data.self_access.args[i].value;
            }

            expr->tag = Expr_MethodCalls;
            expr->data.method_calls.object = obj;
            expr->data.method_calls.method = expr->data.self_access.target;
            expr->data.method_calls.args = args;
            expr->data.method_calls.args_count = ac;
            expr->data.method_calls.range = self_sr;
            return;
        }

        case Expr_Function: {
            StringView name = string_range(expr->data.function_call.name);
            RegisterEntry* entry = register_get(reg, name);
            if (!entry) {
                checker_err_push(errors, (CheckerErr){
                    .tag = Err_Tag_VSF,
                    .data.vsf = { .range = expr->data.function_call.name, .var_name = name }
                });
            }
            return;
        }


        case Expr_MethodCalls: {
            check_expr(expr->data.method_calls.object, reg, errors);
            for (size_t i = 0; i < expr->data.method_calls.args_count; i++)
                check_expr(&expr->data.method_calls.args[i], reg, errors);
            return;
        }

        case Expr_BinaryOps: check_expr(expr->data.binary_ops.left,  reg, errors); check_expr(expr->data.binary_ops.right, reg, errors); return;

        case Expr_Identifiers: {
            StringView name = string_range(expr->data.identifiers.name);
            RegisterEntry* entry = register_get(reg, name);
            if (!entry) {
                checker_err_push(errors, (CheckerErr){
                    .tag = Err_Tag_VSF,
                    .data.vsf = { .range = expr->data.identifiers.name, .var_name = name }
                });
            }
            return;
        }

        case Expr_Vars: {
            StringView name = string_range(expr->data.vars.name);
            RegisterEntry* entry = register_get(reg, name);
            if (!entry) {
                checker_err_push(errors, (CheckerErr){
                    .tag = Err_Tag_VSF,
                    .data.vsf = { .range = expr->data.vars.name, .var_name = name }
                });
            }
            return;
        }
        case Expr_Builtins:
        case Expr_Literals:
        default:
            return;
    }
}
