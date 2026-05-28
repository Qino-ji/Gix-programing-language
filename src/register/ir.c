#include "import.h"
#include "ast.h"
#include "register.h"
#include "ir.h"
#include "helper.h"
    
static IR_Expr lower_expr(Exprs *e, Register *reg);
static IR_Expr *lower_expr_alloc(Exprs *e, Register *reg);
static void lower_stmt(Stmts *s, Register *reg, IR_StmtArr *out, SourceRange fn_ret);
bool range_eq(SourceRange r, const char* str);
bool ranges_equal(SourceRange a, SourceRange b);
Register register_new(Register* parent, IDCounter* counter, GenericRegistry* mono);
void register_free(Register* reg);
void register_insert(Register* reg, StringView name, RegisterEntry entry);
RegisterEntry* register_get(Register* reg, StringView name);
uint32_t register_fresh_id(Register* reg);
void insert_param(Register* reg, Param* p, Type t);
Type resolve_type_tree(SourceRange r, Type* tree, Register* reg);
Type ir_type(const Type* t, Register* reg);
static void lower_stmt(Stmts *s, Register *reg, IR_StmtArr *out, SourceRange fn_ret);
static void lower_body(Stmts *body, size_t n, Register *reg, IR_StmtArr *out, SourceRange fn_ret);
static void hoist_decls(Stmts *body, size_t n, Register *reg, IR_StmtArr *out);
StringView sv(const char* s);
SourceRange mangle_method_name(SourceRange class_name, SourceRange method_name);


static SourceRange resolve_mangled_name(SourceRange base_range, SourceRange* generic_args, size_t generic_count, Register* reg) {
    if (generic_count == 0 || !reg->mono) return base_range;
    ARR(GenericBinding) bindings = {0};
    char* mangled = mangle_name(sv_of(base_range), bindings.data, bindings.len);
    StringView msv = { mangled, strlen(mangled) };
    SourceRange result = base_range;
    RegisterEntry* ment = register_get(reg, msv);

    for (size_t i = 0; i < generic_count; i++) {
        if (!generic_args[i].start) {
            CG_UNREACHABLE_MSG("Null generic argument range in name mangling.");
        }

        ARR_PUSH(bindings, ((GenericBinding){
            .param = {0},
            .bound = range_to_type(generic_args[i], reg),
        }));
    }

    if (ment) {
        result = (SourceRange){
            .start = ment->name,
            .end = ment->name + strlen(ment->name),
        };
    }

    free(mangled);
    ARR_FREE(bindings);
    return result;
}



static EntityID eid_of(Register *reg, SourceRange name) { RegisterEntry *e = register_get(reg, sv_of(name)); return e ? e->eid : (EntityID){0}; }
RegisterEntry *reg_get(Register *reg, SourceRange name) { return register_get(reg, sv_of(name)); }

void ir_module_free(IR_Module *mod) {
    free(mod->defs);
    mod->defs       = NULL;
    mod->defs_count = 0;
    mod->defs_cap   = 0;
}

static SourceRange expr_range(Exprs *e) {
    if (!e) return (SourceRange){0};
    switch (e->tag) {
        case Expr_Literals:     return e->data.literals.range;
        case Expr_Identifiers:  return e->data.identifiers.range;
        case Expr_Vars:         return e->data.vars.range;
        case Expr_BinaryOps:    return e->data.binary_ops.range;
        case Expr_Function:     return e->data.function_call.range;
        case Expr_MethodCalls:  return e->data.method_calls.range;
        case Expr_Struct_Calls: return e->data.struct_calls.range;
        case Expr_Class_Calls:  return e->data.class_calls.range;
        case Expr_Enum_Calls:   return e->data.enum_calls.range;
        default:                return (SourceRange){0};
    }
}

static IR_Expr emit_ssa_temp(IR_StmtArr *out, IR_Expr val, Register *reg) {
    uint32_t id = register_fresh_id(reg);
    SourceRange origin = val.origin;

    ARR_PUSH(*out, ((IR_Stmt){
        .tag = IR_Stmt_SsaTemp,
        .origin = origin,
        .data.ssa_temp = {
            .eid = id,
            .ty = val.ty,
            .val = ir_expr_alloc(val),
        },
    }));
    return (IR_Expr){
        .tag = IR_Expr_VarRef,
        .ty = val.ty,
        .origin = origin,
        .data.var_ref = { .name = origin, .eid = id },
    };
}

static IR_Expr lower_literal(SourceRange r) {
    if (range_eq(r, "true"))  return ir_literal_bool(true,  r);
    if (range_eq(r, "false")) return ir_literal_bool(false, r);
    if (range_eq(r, "null")) {
        return (IR_Expr){
            .tag = IR_Expr_Literal,
            .ty  = (Type){ .tag = Type_Ptr, .data.ptr.inner = NULL },
        };
    }

    if (r.start[0] == '"')  return ir_literal_str(r);
    if (r.start[0] == '\'') {
        size_t len = r.end - r.start;
        char ch = (len >= 2) ? r.start[1] : '\0';
        return ir_literal_char(ch, r);
    }

    size_t len = r.end - r.start;
    for (size_t i = 0; i < len; i++) { if (r.start[i] == '.') return ir_literal_float(strtod(r.start, NULL), r); }

   
    return ir_literal_int(strtoll(r.start, NULL, 0), r);
}

  static IR_Expr lower_expr(Exprs *e, Register *reg) {
      if (!e) return (IR_Expr){ .tag = IR_Expr_Literal, .ty = { .tag = Type_Void } };

      switch (e->tag) {

    case Expr_Literals: return lower_literal(e->data.literals.range);

    case Expr_Identifiers: {
        SourceRange name = e->data.identifiers.name;
        RegisterEntry *ent = reg_get(reg, name);
        Type ty = ent ? ent->type : (Type){ .tag = Type_Void };
        return (IR_Expr){
            .tag = IR_Expr_VarRef,
            .ty = ty,
            .origin = name,
            .data.var_ref = { .name = name, .eid = eid_of(reg, name) },
        };
    }

    case Expr_Vars: {
        SourceRange name = e->data.vars.name;
        RegisterEntry *ent = reg_get(reg, name);
        Type ty = ent ? ent->type : (Type){ .tag = Type_Void };
        return (IR_Expr){
            .tag = IR_Expr_VarRef,
            .ty = ty,
            .origin = name,
            .data.var_ref = { .name = name, .eid = eid_of(reg, name) },
        };
    }

      case Expr_BinaryOps: {
          IR_Expr lhs_raw = lower_expr(e->data.binary_ops.left,  reg);
          IR_Expr rhs_raw = lower_expr(e->data.binary_ops.right, reg);

        LexerTokenTag op = e->data.binary_ops.op;
        Type ty = lhs_raw.ty;
        switch (op) {
            case DoubleEqualss: 
            case NotEqualss: 
            case Lesses:
            case Greaters: 
            case LessEqualss: 
            case GreaterEqualss:
            case Ands: 
            case Ors: ty = (Type){ .tag = Type_Bool }; break;
            default: break;
        }
        return (IR_Expr){
            .tag = IR_Expr_BinOp,
            .ty = ty,
                .origin = e->data.binary_ops.range,
                .data.bin = {
                    .op = op,
                    .lhs = ir_expr_alloc(lhs_raw),
                    .rhs = ir_expr_alloc(rhs_raw),
                },
          };
      }

      case Expr_Function: {
          SourceRange fname = e->data.function_call.name;
          SourceRange resolved_name = fname;
          StringView base = sv_of(fname);
          ARR(GenericBinding) bindings = {0};
    
        if (e->data.function_call.generic_params_count > 0 && reg->mono) {
            for (size_t i = 0; i < e->data.function_call.generic_params_count; i++) {
                ARR_PUSH(bindings, ((GenericBinding){
                    .param = {0},
                    .bound = range_to_type(e->data.function_call.generic_params[i], reg),
                }));
            }

            char* mangled = mangle_name(base, bindings.data, bindings.len);
            StringView msv = { mangled, strlen(mangled) };
            RegisterEntry* ment = register_get(reg, msv);

            if (ment) {
                resolved_name = (SourceRange){
                    .start = ment->name,
                    .end   = ment->name + strlen(ment->name),
                };
            }
            free(mangled);
            ARR_FREE(bindings);
        }

        RegisterEntry* ent = reg_get(reg, resolved_name);
        Type ret = (ent && ent->tag == Reg_Function) ? ent->data.function.return_type : (Type){ .tag = Type_Void };

        IR_ExprPtrArr args_arr = {0};
        for (size_t i = 0; i < e->data.function_call.param_count; i++) {
            Exprs* arg = &e->data.function_call.param[i].value;
            ARR_PUSH(args_arr, ir_expr_alloc(lower_expr(arg, reg)));
        }
        return (IR_Expr){
            .tag = IR_Expr_Call,
            .ty  = ret,
            .origin = e->data.function_call.range,
            .data.call = {
                .name = resolved_name,
                .eid = eid_of(reg, resolved_name),
                .args = args_arr.data,
                .args_count = args_arr.len,
            },
        };
    }

    case Expr_MethodCalls: {
        IR_Expr obj = lower_expr(e->data.method_calls.object, reg);
        SourceRange mname = e->data.method_calls.method;
        SourceRange mangled = mangle_method_name(obj.ty.data.custom.name, mname);
        RegisterEntry* me = reg_get(reg, mangled);
        Type ret = me ? me->data.function.return_type : (Type){ .tag = Type_Void };
        EntityID method_eid = me ? me->eid : (EntityID){0};

        free((char*)mangled.start);

        IR_ExprPtrArr args_arr = {0};
        for (size_t i = 0; i < e->data.method_calls.args_count; i++)
            ARR_PUSH(args_arr, ir_expr_alloc(lower_expr(&e->data.method_calls.args[i], reg)));

        return (IR_Expr){
            .tag = IR_Expr_MethodCall,
            .ty = ret,
            .origin = e->data.method_calls.range,
            .data.method_call = {
                .object = ir_expr_alloc(obj),
                .method = mname,
                .method_eid = method_eid,
                .args = args_arr.data,
                .args_count = args_arr.len,
            },
        };
    }

    case Expr_Struct_Calls: { 
        SourceRange sname = resolve_mangled_name(e->data.struct_calls.name, e->data.struct_calls.generic_params, e->data.struct_calls.generic_params_count, reg);
        ARR(IR_FieldInit) fields_arr = {0};
    
        for (size_t i = 0; i < e->data.struct_calls.param_count; i++) {
            SourceRange fn = e->data.struct_calls.param[i].name;
            Exprs val_expr = {
                .tag = Expr_Identifiers,
                .data.identifiers = { .name = fn },
            };
            ARR_PUSH(fields_arr, ((IR_FieldInit){
                .name = fn,
                .val = ir_expr_alloc(lower_expr(&val_expr, reg)),
            }));
        }
        return (IR_Expr){
            .tag = IR_Expr_MakeStruct,
            .ty = (Type){ .tag = Type_Custom, .data.custom.name = sname },
            .origin = e->data.struct_calls.range,
            .data.make_struct = {
                .name = sname,
                .eid = eid_of(reg, sname),
                .fields = fields_arr.data,
                .fields_count = fields_arr.len,
            },
        };
    }

    case Expr_Class_Calls: { 
        SourceRange cname = resolve_mangled_name(e->data.class_calls.name, e->data.class_calls.generic_params, e->data.class_calls.generic_params_count, reg);
        IR_ExprPtrArr args_arr = {0};

        for (size_t i = 0; i < e->data.class_calls.param_count; i++) {
            Exprs *arg = &e->data.class_calls.param[i].value;
            ARR_PUSH(args_arr, ir_expr_alloc(lower_expr(arg, reg)));
        }

        return (IR_Expr){
            .tag = IR_Expr_MakeClass,
            .ty = (Type){ .tag = Type_Custom, .data.custom.name = cname },
            .origin = e->data.class_calls.range,
            .data.make_class = {
                .name = cname,
                .eid = eid_of(reg, cname),
                .args = args_arr.data,
                .args_count = args_arr.len, 
            },
        };
    }

    case Expr_Enum_Calls: {
        SourceRange ename = resolve_mangled_name(e->data.enum_calls.name, e->data.enum_calls.generic_params, e->data.enum_calls.generic_params_count, reg);
        SourceRange variant = e->data.enum_calls.field;
        IR_ExprPtrArr args_arr = {0};

       for (size_t i = 0; i < e->data.enum_calls.param_count; i++) {
            Exprs *arg = &e->data.enum_calls.param[i].value;
            ARR_PUSH(args_arr, ir_expr_alloc(lower_expr(arg, reg)));
        }

        return (IR_Expr){
            .tag = IR_Expr_MakeEnum,
            .ty = (Type){ .tag = Type_Custom, .data.custom.name = ename },
            .origin = e->data.enum_calls.range,
            .data.make_enum = {
                .type_name = ename,
                .variant = variant,
                .eid = eid_of(reg, variant),
                .args = args_arr.data,
                .args_count = args_arr.len,
            },
        };
    }

    case Expr_Cast: {
        IR_Expr inner = lower_expr(e->data.cast.expr, reg);
        if (!e->data.cast.ty) {
            CG_UNREACHABLE_MSG("cast expression has NULL target type");
            return inner;
        }
        Type target_ty = *e->data.cast.ty;
        return (IR_Expr){
            .tag = IR_Expr_Cast,
            .ty  = target_ty,
            .origin = e->data.cast.range,
            .data.cast = { .expr = ir_expr_alloc(inner) },
        };
    }

    default:
        return (IR_Expr){ .tag = IR_Expr_Literal, .ty = { .tag = Type_Void } };
    }
}

static IR_Expr *lower_expr_alloc(Exprs *e, Register *reg) {
    return ir_expr_alloc(lower_expr(e, reg));
}

static IR_Expr lower_assign_target(Exprs *e, Register *reg) {
    if (e && e->tag == Expr_Unary && e->data.unary.op == Stars) {
        IR_Expr ptr = lower_expr(e->data.unary.operand, reg);
        Type inner = ptr.ty.tag == Type_Ptr ? *ptr.ty.data.ptr.inner : (Type){ .tag = Type_Void };

        return (IR_Expr){
            .tag = IR_Expr_Deref,
            .ty = inner,
            .origin = expr_range(e),
            .data.deref = { .ptr = ir_expr_alloc(ptr) },
        };
    }
    return lower_expr(e, reg);
}

static void hoist_decls(Stmts *body, size_t n, Register *reg, IR_StmtArr *out) {
    for (size_t i = 0; i < n; i++) {
        Stmts *s = &body[i];
        switch (s->tag) {
            case Stmt_Vars: {
            SourceRange name = s->data.vars.name;
            int name_len = (int)(name.end - name.start);
            printf("[DEBUG:hoist] Hoisting variable: %.*s\n", name_len, name.start);

            RegisterEntry *ent = reg_get(reg, name);
            Type ty = (Type){ .tag = Type_Void };

            if (ent) {
                if (ent->type.tag < 0 || ent->type.tag > 100) {
                    ty = (Type){ .tag = Type_Void };
                } else {
                    ty = ent->type;
                }
            }

            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_VarDecl,
                .origin = s->data.vars.range,
                .data.var_decl = {
                    .name = name,
                    .eid = ent ? ent->eid.id : 0,
                    .ty = ty,
                    .init = NULL,
                    .mode = s->data.vars.mode,
                },
            }));
            break;
        }
        
        case Stmt_Lets: {
            SourceRange name = s->data.lets.name;
            RegisterEntry *ent = reg_get(reg, name);
            Type ty = ent ? ent->type : (Type){ .tag = Type_Void };
            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_LetDecl,
                .origin = s->data.lets.range,
                .data.let_decl = {
                    .name = name,
                    .eid = ent ? ent->eid.id : 0,
                    .ty = ty,
                    .init = NULL,
                    .mode = s->data.lets.mode,
                },
            }));
            break;
        }

        case Stmt_Consts: {
            SourceRange name = s->data.consts.name;
            RegisterEntry *ent = reg_get(reg, name);
            Type ty = ent ? ent->type : (Type){ .tag = Type_Void };
            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_ConstDecl,
                .origin = s->data.consts.range,
                .data.const_decl = {
                    .name = name,
                    .eid = ent ? ent->eid.id : 0,
                    .ty = ty,
                    .init = NULL,
                },
            }));
            break;
        }

        case Stmt_Locals: {
            SourceRange name = s->data.locals.name;
            RegisterEntry *ent = reg_get(reg, name);
            Type ty = ent ? ent->type : (Type){ .tag = Type_Void };
            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_LocalDecl,
                .origin = s->data.locals.range,
                .data.local_decl = {
                    .name = name,
                    .eid = ent ? ent->eid.id : 0,
                    .ty = ty,
                },
            }));
            break;
        }

        case Stmt_Ifs: hoist_decls(s->data.ifs.body, s->data.ifs.body_count, reg, out); hoist_decls(s->data.ifs.else_body, s->data.ifs.else_body_count, reg, out); break;
        case Stmt_Whiles: hoist_decls(s->data.whiles.body, s->data.whiles.body_count, reg, out); break;
        case Stmt_Fors: hoist_decls(s->data.fors.body, s->data.fors.body_count, reg, out); break;
        case Stmt_Matchs:
            for (size_t c = 0; c < s->data.matchs.cases_count; c++) hoist_decls(s->data.matchs.cases[c].body, s->data.matchs.cases[c].body_count, reg, out);
            if (s->data.matchs.default_body_count > 0) hoist_decls(s->data.matchs.default_body, s->data.matchs.default_body_count, reg, out);
            break;
        case Stmt_AtomicOp: break;
        default: break;
        }
    }
}

static void lower_body(Stmts *body, size_t n, Register *reg, IR_StmtArr *out, SourceRange fn_ret) {
    for (size_t i = 0; i < n; i++) lower_stmt(&body[i], reg, out, fn_ret);
}

static IR_Stmt lower_match(Stmts *s, Register *reg, SourceRange fn_ret) {
    IR_Expr *mexpr = lower_expr_alloc(&s->data.matchs.expr, reg);

    ARR(IR_MatchArm) arms_arr = {0};
    for (size_t i = 0; i < s->data.matchs.cases_count; i++) {
        MatchArm *src = &s->data.matchs.cases[i];
        IR_StmtArr arm_body = {0};

        lower_body(src->body, src->body_count, reg, &arm_body, fn_ret);
        ARR_PUSH(arms_arr, ((IR_MatchArm){
            .pattern = src->pattern,
            .body = arm_body.data,
            .body_count = arm_body.len,
        }));
    }

    IR_StmtArr def_arr = {0};

    if (s->data.matchs.default_body_count > 0) lower_body(s->data.matchs.default_body, s->data.matchs.default_body_count, reg, &def_arr, fn_ret);

    return (IR_Stmt){
        .tag = IR_Stmt_Match,
        .origin = s->data.matchs.range,
        .data.match = {
            .expr = mexpr,
            .arms = arms_arr.data,
            .arms_count = arms_arr.len,
            .default_body = def_arr.data,
            .default_body_count = def_arr.len,
        },
    };
}

static IR_Stmt lower_if(Stmts *s, Register *reg, SourceRange fn_ret) {
    IR_Expr *cond = lower_expr_alloc(&s->data.ifs.cond, reg);

    IR_StmtArr then_arr = {0};
    IR_StmtArr else_arr = {0};

    lower_body(s->data.ifs.body, s->data.ifs.body_count, reg, &then_arr, fn_ret);
    lower_body(s->data.ifs.else_body, s->data.ifs.else_body_count, reg, &else_arr, fn_ret);

    return (IR_Stmt){
        .tag = IR_Stmt_If,
        .origin = s->data.ifs.range,
        .data.if_ = {
            .cond = cond,
            .guard_pattern = s->data.ifs.guard_pattern,
            .body = then_arr.data,
            .body_count = then_arr.len,
            .else_body = else_arr.data,
            .else_body_count = else_arr.len,
        },
    };
}

static bool expr_exists(Exprs expr) { return expr.data.literals.range.start != NULL; }

static IR_Stmt lower_while(Stmts *s, Register *reg, SourceRange fn_ret) {
    IR_Expr *cond = lower_expr_alloc(&s->data.whiles.cond, reg);

    IR_StmtArr body_arr = {0};
    lower_body(s->data.whiles.body, s->data.whiles.body_count, reg, &body_arr, fn_ret);

    return (IR_Stmt){
        .tag = IR_Stmt_While,
        .origin = s->data.whiles.range,
        .data.while_ = {
            .cond = cond,
            .body = body_arr.data,
            .body_count = body_arr.len,
        },
    };
}

static IR_Stmt lower_for(Stmts *s, Register *reg, SourceRange fn_ret) {
    SourceRange var = s->data.fors._var;
    RegisterEntry *var_ent = reg_get(reg, var);
    Type var_ty = var_ent ? var_ent->type : (Type){ .tag = Type_Void };
    IR_Expr *iter = lower_expr_alloc(&s->data.fors.iter, reg);
    IR_StmtArr body_arr = {0};

    lower_body(s->data.fors.body, s->data.fors.body_count, reg, &body_arr, fn_ret);

    return (IR_Stmt){
        .tag = IR_Stmt_For,
        .origin = s->data.fors.range,
        .data.for_ = {
            .var = var,
            .var_eid = var_ent ? var_ent->eid.id : 0,
            .var_ty = var_ty,
            .iter = iter,
            .body = body_arr.data,
            .body_count = body_arr.len,
        },
    };
}

static void lower_stmt(Stmts *s, Register *reg, IR_StmtArr *out, SourceRange fn_ret) {
    if (!s) return;

    switch (s->tag) {
    case Stmt_Vars: {
        if (expr_exists(s->data.vars.value)) {
            SourceRange name = s->data.vars.name;
            RegisterEntry *ent = reg_get(reg, name);
            IR_Expr target = (IR_Expr){
                .tag = IR_Expr_VarRef,
                .ty = ent ? ent->type : (Type){ .tag = Type_Void },
                .origin = name,
                .data.var_ref = { .name = name, .eid = ent ? ent->eid.id : 0 },
            };
            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_Assign,
                .origin = s->data.vars.range,
                .data.assign = {
                    .target = ir_expr_alloc(target),
                    .op = Equalss,
                    .value = lower_expr_alloc(&s->data.vars.value, reg),
                },
            }));
        }
        break;
    }

    case Stmt_Lets: {
        if (expr_exists(s->data.lets.value)) {
            SourceRange name = s->data.lets.name;
            RegisterEntry *ent = reg_get(reg, name);

            IR_Expr target = (IR_Expr){
                .tag = IR_Expr_VarRef,
                .ty = ent ? ent->type : (Type){ .tag = Type_Void },
                .origin = name,
                .data.var_ref = { .name = name, .eid = ent ? ent->eid.id : 0 },
            };
            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_Assign,
                .origin = s->data.lets.range,
                .data.assign = {
                    .target = ir_expr_alloc(target),
                    .op = Equalss,
                    .value = lower_expr_alloc(&s->data.lets.value, reg),
                },
            }));
        }
        break;
    }

    case Stmt_Consts: {
        if (expr_exists(s->data.consts.value)) {
            SourceRange name = s->data.consts.name;
            RegisterEntry *ent = reg_get(reg, name);
            IR_Expr target = (IR_Expr){
                .tag = IR_Expr_VarRef,
                .ty = ent ? ent->type : (Type){ .tag = Type_Void },
                .origin = name,
                .data.var_ref = { .name = name, .eid = ent ? ent->eid.id : 0 },
            };
            ARR_PUSH(*out, ((IR_Stmt){
                .tag = IR_Stmt_Assign,
                .origin = s->data.consts.range,
                .data.assign = {
                    .target = ir_expr_alloc(target),
                    .op = Equalss,
                    .value = lower_expr_alloc(&s->data.consts.value, reg),
                },
            }));
        }
        break;
    }

    case Stmt_Locals:
        break;

    case Stmt_Assigns: {
        IR_Expr target = lower_assign_target(&s->data.assigns.target, reg);
        IR_Expr value  = lower_expr(&s->data.assigns.value, reg);

        ARR_PUSH(*out, ((IR_Stmt){
            .tag = IR_Stmt_Assign,
            .origin = s->data.assigns.range,
            .data.assign = {
                .target = ir_expr_alloc(target),
                .op = s->data.assigns.op,
                .value = ir_expr_alloc(value),
            },
        }));
        break;
    }

    case Stmt_Returns: {
        IR_Expr tmp = lower_expr(&s->data.returns.expr, reg);
        IR_Expr *val = tmp.ty.tag != Type_Void ? ir_expr_alloc(tmp) : NULL;
        ARR_PUSH(*out, ((IR_Stmt){
            .tag = IR_Stmt_Return,
            .origin = s->data.returns.range,
            .data.ret = { .val = val },
        }));
        break;
    }

    case Stmt_Ifs:    ARR_PUSH(*out, lower_if(s, reg, fn_ret));    break;
    case Stmt_Whiles: ARR_PUSH(*out, lower_while(s, reg, fn_ret)); break;
    case Stmt_Fors:   ARR_PUSH(*out, lower_for(s, reg, fn_ret));   break;
    case Stmt_Matchs: ARR_PUSH(*out, lower_match(s, reg, fn_ret)); break;

    case Stmt_ExprStmt: {
        IR_Expr ex = lower_expr(&s->data.expr_stmt.expr, reg);
        ARR_PUSH(*out, ((IR_Stmt){
            .tag = IR_Stmt_Expr,
            .origin = expr_range(&s->data.expr_stmt.expr),
            .data.expr = { .expr = ir_expr_alloc(ex) },
        }));
        break;
    }

    case Stmt_AtomicOp: {
        RegisterEntry *ent = reg_get(reg, s->data.atomic_op.target);
        size_t ac = s->data.atomic_op.args_count;
        IR_Expr *args[3] = { NULL, NULL, NULL };

        for (size_t i = 0; i < ac; i++) args[i] = lower_expr_alloc(&s->data.atomic_op.args[i], reg);

        ARR_PUSH(*out, ((IR_Stmt){
            .tag = IR_Stmt_AtomicOp,
            .origin = s->data.atomic_op.range,
            .data.atomic_op = {
                .target = s->data.atomic_op.target,
                .target_eid = ent ? ent->eid.id : 0,
                .op = s->data.atomic_op.op,
                .args = { args[0], args[1], args[2] },
                .args_count = ac,
                .ordering = s->data.atomic_op.ordering,
                .ordering2  = s->data.atomic_op.ordering2,
            },
        }));
        break;
    }

    case Stmt_Functions:
    case Stmt_Structs:
    case Stmt_Enums:
    case Stmt_Classes:
    case Stmt_Traits:
    default:
        break;
    }
}


static IR_FuncDef lower_func_def(Stmts *s, Register *reg) {
    SourceRange fname = s->data.functions.name;
    Param *params = s->data.functions.params;
    size_t pc = s->data.functions.params_count;
    Register child = register_new(reg, reg->counter, reg->mono);
    IR_StmtArr body_arr = {0};
    ARR(IR_Param) params_arr = {0};

    for (size_t i = 0; i < pc; i++) {
        Type tmp_t = resolve_type_tree(params[i].c_type, params[i].type_tree, reg);
        Type t = tmp_t;

        ARR_PUSH(params_arr, ((IR_Param){
            .name = params[i].name,
            .ty = t,
            .mode = VarMode_Value,
            .eid = 0,
        }));
        insert_param(&child, &params[i], t);
    }

    Type tmp_ret = resolve_type_tree(s->data.functions.return_type, s->data.functions.return_type_tree, reg);
    Type ret_ty = tmp_ret;
    RegisterEntry *fn_entry = register_get(reg, sv_of(fname));
    CheckerErrList dummy_errors = {0};

    populate_register(s->data.functions.body, s->data.functions.body_count, &child, &dummy_errors);
    hoist_decls(s->data.functions.body, s->data.functions.body_count, &child, &body_arr);
    lower_body(s->data.functions.body, s->data.functions.body_count, &child, &body_arr, s->data.functions.return_type);
    register_free(&child);

    return (IR_FuncDef){
        .name = fname,
        .eid = fn_entry ? fn_entry->eid : (EntityID){0},
        .return_type = ret_ty,
        .params = params_arr.data,
        .params_count = params_arr.len,
        .body = body_arr.data,
        .body_count = body_arr.len,
        .is_pub = s->data.functions.is_pub,
        .is_unsafe = false,
        .operation_op = 0,
        .cc = CC_Default,
    };
}


static IR_FuncDef lower_method_def(FunctionMethod *m, SourceRange class_name, Register *reg) {
    size_t pc = m->params_count;
    Register child = register_new(reg, reg->counter, reg->mono);

    register_insert(&child, sv("self"), (RegisterEntry){
        .tag  = Reg_Var,
        .type = (Type){ .tag = Type_Custom, .data.custom.name = class_name },
    });

    RegisterEntry *dbg_self = register_get(&child, sv("self"));


    Type tmp_ret = resolve_type_tree(m->return_type, m->return_type_tree, reg);
    Type ret_ty = tmp_ret;
    IR_StmtArr body_arr = {0};
    ARR(IR_Param) params_arr = {0};

    for (size_t i = 0; i < pc; i++) {
        Type tmp_t = resolve_type_tree(m->params[i].c_type, m->params[i].type_tree, reg);
        Type t = tmp_t;
        ARR_PUSH(params_arr, ((IR_Param){
            .name = m->params[i].name,
            .ty = t,
            .mode = VarMode_Value,
            .eid  = 0,
        }));
    }

    RegisterEntry *fn_entry = register_get(reg, sv_of(m->name));

    for (size_t i = 0; i < pc; i++) insert_param(&child, &m->params[i], params_arr.data[i].ty);
    for (size_t i = 0; i < pc; i++) params_arr.data[i].eid = eid_of(&child, m->params[i].name);

    CheckerErrList dummy_errors = {0};

    populate_register(m->body, m->body_count, &child, &dummy_errors);
    hoist_decls(m->body, m->body_count, &child, &body_arr);
    lower_body(m->body, m->body_count, &child, &body_arr, m->return_type);
    register_free(&child);

    
    return (IR_FuncDef){
        .name = m->name,
        .eid = fn_entry ? fn_entry->eid : (EntityID){0},
        .return_type = ret_ty,
        .params = params_arr.data,
        .params_count = params_arr.len,
        .body = body_arr.data,
        .body_count = body_arr.len,
        .is_pub = true,
        .is_unsafe = false,
        .operation_op = m->operation.function.start != NULL ? m->operation.op : 0,
        .cc = CC_Default,
    };
}


IR_Module lower_module(const char *name, Stmts *body, size_t n, Register *reg) {
    IR_Module mod = {
        .name = (char *)name,
        .defs = NULL,
        .defs_count = 0,
        .defs_cap = 0,
    };

    for (size_t i = 0; i < n; i++) {
        Stmts *s = &body[i];

        switch (s->tag) {

        case Stmt_Functions: {
            IR_FuncDef def = lower_func_def(s, reg);
            ir_module_push(&mod, (IR_Def){
                .tag = IR_Def_Function,
                .origin = s->data.functions.range,
                .data.function = { .def = def },
            });
            break;
        }

        case Stmt_Structs: {
            SourceRange sname = s->data.structs.name;
            ARR(IR_FieldDef) fields_arr = {0};
            for (size_t f = 0; f < s->data.structs.fields_count; f++) {
                ARR_PUSH(fields_arr, ((IR_FieldDef){
                    .name = s->data.structs.fields[f].name,
                    .ty = range_to_type(s->data.structs.fields[f].c_type, reg),
                }));
            }
            RegisterEntry *ent = reg_get(reg, sname);
            ir_module_push(&mod, (IR_Def){
                .tag = IR_Def_Struct,
                .origin = s->data.structs.range,
                .data.struct_ = {
                    .name = sname,
                    .eid = ent ? ent->eid.id : 0,
                    .fields = fields_arr.data,
                    .fields_count = fields_arr.len,
                    .is_pub = s->data.structs.is_pub,
                },
            });
            break;
        }

        case Stmt_Enums: {
            SourceRange ename = s->data.enums.name;
            ARR(IR_VariantDef) variants_arr = {0};

            for (size_t v = 0; v < s->data.enums.variants_count; v++) {
                EnumVariant *ev = &s->data.enums.variants[v];
                ARR(IR_FieldDef) fds_arr = {0};

                for (size_t f = 0; f < ev->fields_count; f++) {
                    ARR_PUSH(fds_arr, ((IR_FieldDef){
                        .name = ev->fields[f].first,
                        .ty = range_to_type(ev->fields[f].second, reg),
                    }));
                }

                ARR_PUSH(variants_arr, ((IR_VariantDef){
                    .name = ev->name,
                    .fields = fds_arr.data,
                    .fields_count = fds_arr.len,
                }));
            }

            RegisterEntry *ent = reg_get(reg, ename);
            ir_module_push(&mod, (IR_Def){
                .tag = IR_Def_Enum,
                .origin = s->data.enums.range,
                .data.enum_ = {
                    .name = ename,
                    .eid = ent ? ent->eid.id : 0,
                    .variants = variants_arr.data,
                    .variants_count = variants_arr.len,
                    .is_pub = s->data.enums.is_pub,
                },
            });
            break;
        }

        case Stmt_Externs: {
            ExternFunction* funcs = s->data.extern_.funcs;
            size_t funcs_count = s->data.extern_.funcs_count;
            SourceRange abi = s->data.extern_.abi;
            SourceRange ffi = s->data.extern_.ffi;

            for (size_t j = 0; j < funcs_count; j++) {
                  ExternFunction* fn = &funcs[j];
                  ARR(IR_Param) params_arr = {0};
                  Type tmp_ret = resolve_type_tree(fn->return_type, fn->return_type_tree, reg);
                  Type ret_ty  = ir_type(&tmp_ret, reg);

                  for (size_t k = 0; k < fn->params_count; k++) {
                      Type tmp_t = resolve_type_tree(fn->params[k].c_type, fn->params[k].type_tree, reg);
                      Type t = ir_type(&tmp_t, reg);
                      ARR_PUSH(params_arr, ((IR_Param){
                          .name = fn->params[k].name,
                          .ty = t,
                          .mode = VarMode_Value,
                          .eid = 0,
                      }));
                  }

                ir_module_push(&mod, (IR_Def){
                    .tag = IR_Def_Extern,
                    .origin = abi,
                    .data.extern_ = {
                        .abi = abi,
                          .ffi = ffi,
                          .name = fn->name,
                          .eid = eid_of(reg, fn->name),
                          .return_type = ret_ty,
                          .params = params_arr.data,
                          .params_count = params_arr.len,
                          .is_pub = true,
                      }
                  });
            }
            break;
        }

        case Stmt_Classes: {
            SourceRange cname = s->data.classes.name;
            ARR(IR_FieldDef) fields_arr = {0};
            ARR(IR_FuncDef) methods_arr = {0};

            for (size_t f = 0; f < s->data.classes.fields_count; f++) {
                ARR_PUSH(fields_arr, ((IR_FieldDef){
                    .name = s->data.classes.fields[f].name,
                    .ty = range_to_type(s->data.classes.fields[f].c_type, reg),
                }));
            }

            for (size_t m = 0; m < s->data.classes.methods_count; m++) ARR_PUSH(methods_arr, lower_method_def(&s->data.classes.methods[m], cname, reg));

            RegisterEntry *ent = reg_get(reg, cname);
            ir_module_push(&mod, (IR_Def){
                .tag = IR_Def_Class,
                .origin = s->data.classes.range,
                .data.class_ = {
                    .name = cname,
                    .eid = ent ? ent->eid.id : 0,
                    .fields = fields_arr.data,
                    .fields_count = fields_arr.len,
                    .methods = methods_arr.data,
                    .methods_count = methods_arr.len,
                    .is_pub = true,
                },
            });

            for (size_t t = 0; t < s->data.classes.traits_count; t++) {
                SourceRange trait_name = s->data.classes.trait_bounds[t];
                RegisterEntry* te = reg_get(reg, trait_name);
                ARR(IR_FuncDef) vtable_methods = {0};

                if (!te || te->tag != Reg_Trait) continue;
    
                for (size_t m = 0; m < te->data.trait.methods_count; m++) {
                    for (size_t cm = 0; cm < s->data.classes.methods_count; cm++) {
                        if (ranges_equal(s->data.classes.methods[cm].name, te->data.trait.methods[m].name)) {
                            ARR_PUSH(vtable_methods, lower_method_def(&s->data.classes.methods[cm], cname, reg));
                            break;
                        }
                    }
                }

                ir_module_push(&mod, (IR_Def){
                    .tag = IR_Def_VTable,
                    .origin = s->data.classes.range,
                    .data.vtable = {
                        .trait_name = trait_name,
                        .impl_type = cname,
                        .eid = ent ? ent->eid.id : 0,
                        .methods = vtable_methods.data,
                        .methods_count = vtable_methods.len,
                    },
                });
            }
            break;
        }

        case Stmt_Traits: {
            SourceRange tname = s->data.traits.name;
            ARR(IR_FuncDef) methods_arr = {0};
            for (size_t m = 0; m < s->data.traits.methods_count; m++) {
                TraitMethod *tm = &s->data.traits.methods[m];
                ARR(IR_Param) params_arr = {0};

                for (size_t p = 0; p < tm->params_count; p++) {
                    ARR_PUSH(params_arr, ((IR_Param){
                        .name = tm->params[p].name,
                        .ty = range_to_type(tm->params[p].c_type, reg),
                        .mode = VarMode_Value,
                        .eid = 0,
                    }));
                }
                ARR_PUSH(methods_arr, ((IR_FuncDef){
                    .name = tm->name,
                    .return_type = range_to_type(tm->return_type, reg),
                    .params = params_arr.data,
                    .params_count = params_arr.len,
                    .body = NULL,
                    .body_count = 0,
                    .is_pub = true,
                    .cc = CC_Default,
                }));
            }

            RegisterEntry *ent = reg_get(reg, tname);
            ir_module_push(&mod, (IR_Def){
                .tag = IR_Def_Trait,
                .origin = s->data.traits.range,
                .data.trait_ = {
                    .name = tname,
                    .eid = ent ? ent->eid.id : 0,
                    .methods = methods_arr.data,
                    .methods_count = methods_arr.len,
                    .is_pub = s->data.traits.is_pub,
                },
            });
            break;
        }

        case Stmt_Modules: {
            IR_Module sub = lower_module( NULL, s->data.modules.body, s->data.modules.body_count, reg);

            for (size_t j = 0; j < sub.defs_count; j++) ir_module_push(&mod, sub.defs[j]); free(sub.defs);
            break;
        }

        default:
            break;
        }
    }

    if (reg->mono) {
        khint_t it;
        kh_foreach(reg->mono->table, it) {
            GenericInstance* gi = &kh_val(reg->mono->table, it);
            StringView msv = { gi->mangled_name, strlen(gi->mangled_name) };
            RegisterEntry* ent = register_get(reg, msv);
            if (!ent) continue;

            switch (gi->def_kind) {

                case Reg_Struct: {
                    ARR(IR_FieldDef) fields_arr = {0};
                    for (size_t f = 0; f < ent->data.strct.fields_count; f++) {
                        ARR_PUSH(fields_arr, ((IR_FieldDef){
                            .name = ent->data.strct.fields[f].name,
                            .ty   = range_to_type(ent->data.strct.fields[f].c_type, reg),
                        }));
                    }
                    ir_module_push(&mod, (IR_Def){
                        .tag    = IR_Def_Struct,
                        .origin = ent->decl_range,
                        .data.struct_ = {
                            .name         = ent->decl_name_range,
                            .eid          = ent->eid.id,
                            .fields       = fields_arr.data,
                            .fields_count = fields_arr.len,
                            .is_pub       = ent->data.strct.is_pub,
                        },
                    });
                    break;
                }

                case Reg_Enum: {
                    ARR(IR_VariantDef) variants_arr = {0};
                    for (size_t v = 0; v < ent->data.enm.variants_count; v++) {
                        EnumVariant* ev = &ent->data.enm.variants[v];
                        ARR(IR_FieldDef) fds_arr = {0};
                        for (size_t f = 0; f < ev->fields_count; f++) {
                            ARR_PUSH(fds_arr, ((IR_FieldDef){
                                .name = ev->fields[f].first,
                                .ty   = range_to_type(ev->fields[f].second, reg),
                            }));
                        }
                        ARR_PUSH(variants_arr, ((IR_VariantDef){
                            .name         = ev->name,
                            .fields       = fds_arr.data,
                            .fields_count = fds_arr.len,
                        }));
                    }
                    ir_module_push(&mod, (IR_Def){
                        .tag    = IR_Def_Enum,
                        .origin = ent->decl_range,
                        .data.enum_ = {
                            .name           = ent->decl_name_range,
                            .eid            = ent->eid.id,
                            .variants       = variants_arr.data,
                            .variants_count = variants_arr.len,
                            .is_pub         = ent->data.enm.is_pub,
                        },
                    });
                    break;
                }

                case Reg_Class: {
                    ARR(IR_FieldDef) fields_arr = {0};
                    ARR(IR_FuncDef) methods_arr = {0};

                    for (size_t f = 0; f < ent->data._class.fields_count; f++) {
                        ARR_PUSH(fields_arr, ((IR_FieldDef){
                            .name = ent->data._class.fields[f].name,
                            .ty = range_to_type(ent->data._class.fields[f].c_type, reg),
                        }));
                    }
                    for (size_t m = 0; m < ent->data._class.methods_count; m++)
                        ARR_PUSH(methods_arr, lower_method_def(&ent->data._class.methods[m], ent->decl_name_range, reg));
                        ir_module_push(&mod, (IR_Def){
                            .tag = IR_Def_Class,
                            .origin = ent->decl_range,
                            .data.class_ = {
                                .name = ent->decl_name_range,
                                .eid = ent->eid.id,
                                .fields = fields_arr.data,
                                .fields_count  = fields_arr.len,
                                .methods = methods_arr.data,
                                .methods_count = methods_arr.len,
                                .is_pub = ent->data._class.is_pub,
                            },
                        });
                        break;
                }
                default: break;
            }
        }
    }

    return mod;
}
