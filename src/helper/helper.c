#include "import.h"
#include "register.h"

StringView string_range(SourceRange range);
bool range_eq(SourceRange r, const char* str);

bool sv_equal(StringView a, StringView b) {
    return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0;
}

bool str_equal(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

StringView string(const char* s) {
    return (StringView){ .ptr = s, .len = s ? strlen(s) : 0 };
}

void range_to_span(SourceRange r, LineStarts* ls, uint32_t* line_start, uint16_t* col_start, uint32_t* line_end, uint16_t* col_end) {
    size_t ls_idx = get_line_num(ls, (uintptr_t)r.start);
    size_t le_idx = get_line_num(ls, (uintptr_t)r.end);

    *line_start = (ls_idx != (size_t)-1) ? (uint32_t)ls_idx : 0;
    *line_end = (le_idx != (size_t)-1) ? (uint32_t)le_idx : 0;

    *col_start = (ls_idx != (size_t)-1) ? (uint16_t)(r.start - ls->data[ls_idx]) : 0;
    *col_end = (le_idx != (size_t)-1) ? (uint16_t)(r.end - ls->data[le_idx])   : 0;
}


const char* op_tag_to_str(LexerTokenTag tag) {
    switch (tag) {
        case Plus:              return "+";
        case Minuss:            return "-";
        case Stars:             return "*";
        case Slashs:            return "/";
        case Percents:          return "%";
        case Lesses:            return "<";
        case Greaters:          return ">";
        case Equalss:           return "=";
        case Ampersands:        return "&";
        case Pipes:             return "|";
        case Carets:            return "^";
        case Tildes:            return "~";
        case Bangs:             return "!";
        case PlusEqualss:       return "+=";
        case MinusEqualss:      return "-=";
        case StarEqualss:       return "*=";
        case SlashEqualss:      return "/=";
        case PercentEqualss:    return "%=";
        case PipeEqualss:       return "|=";
        case AmpersandEqualss:  return "&=";
        case CaretEqualss:      return "^=";
        case LeftShiftEqualss:  return "<<=";
        case RightShiftEqualss: return ">>=";
        case LeftShifts:        return "<<";
        case RightShifts:       return ">>";
        case LessEqualss:       return "<=";
        case GreaterEqualss:    return ">=";
        case NotEqualss:        return "!=";
        case DoubleEqualss:     return "==";
        case Ands:              return "&&";
        case Ors:               return "||";
        case Nots:              return "!";
        default:                return "?";
    }
}

StringView type_tag_to_view(Type t) {
    switch (t.tag) {
        case Type_Int:    return string(t.data.int_t.bits == 64 ? "i64" : "i32");
        case Type_Float:  return string(t.data.float_t.bits == 64 ? "f64" : "f32");
        case Type_Bool:   return string("bool");
        case Type_Char:   return string("char");
        case Type_Str:    return string("str");
        case Type_Void:   return string("void");
        case Type_Custom: return string_range(t.data.custom.name);
        default:          return string("unknown");
    }
}


bool ranges_equal(SourceRange a, SourceRange b) {
    size_t la = a.end - a.start, lb = b.end - b.start;
    return la == lb && memcmp(a.start, b.start, la) == 0;
}

bool range_eq_sv(SourceRange r, StringView sv) {
    size_t len = r.end - r.start;
    return len == sv.len && memcmp(r.start, sv.ptr, len) == 0;
}

bool conds_equal(Exprs* a, Exprs* b) {
    if (a->tag != b->tag) return false;
    if (a->tag == Expr_Identifiers) return ranges_equal(a->data.identifiers.name, b->data.identifiers.name);
    if (a->tag == Expr_Literals)    return ranges_equal(a->data.literals.range,   b->data.literals.range);
    if (a->tag == Expr_BinaryOps)   return a->data.binary_ops.op == b->data.binary_ops.op && conds_equal(a->data.binary_ops.left,  b->data.binary_ops.left) && conds_equal(a->data.binary_ops.right, b->data.binary_ops.right);
    if (a->tag == Expr_Function)    return ranges_equal(a->data.function_call.name, b->data.function_call.name);
    if (a->tag == Expr_MethodCalls) return ranges_equal(a->data.method_calls.method, b->data.method_calls.method);
    return false;
}

bool is_tautolog(Exprs* cond) {
    if (cond->tag != Expr_BinaryOps) return false;
    LexerTokenTag op = cond->data.binary_ops.op;
    if (op == DoubleEqualss || op == LessEqualss || op == GreaterEqualss) return conds_equal(cond->data.binary_ops.left, cond->data.binary_ops.right);
    if (op == NotEqualss    || op == Lesses      || op == Greaters)       return conds_equal(cond->data.binary_ops.left, cond->data.binary_ops.right);
    return false;
}

bool is_always(Exprs* cond) {
    if (cond->tag != Expr_BinaryOps) return false;
    LexerTokenTag op = cond->data.binary_ops.op;
    return (op == NotEqualss || op == Lesses || op == Greaters) && conds_equal(cond->data.binary_ops.left, cond->data.binary_ops.right);
}


bool is_conditionable(Type t) { return t.tag == Type_Bool; }

bool is_builtin_type(SourceRange name) {
    return range_eq(name, "int")     ||
           range_eq(name, "int8")    ||
           range_eq(name, "int16")   ||
           range_eq(name, "int32")   ||
           range_eq(name, "int64")   ||
           range_eq(name, "float")   ||
           range_eq(name, "float32") ||
           range_eq(name, "float64") ||
           range_eq(name, "char")    ||
           range_eq(name, "str")     ||
           range_eq(name, "bool")    ||
           range_eq(name, "void");
}

bool generic_param_used_in_type(SourceRange param, SourceRange type_name) {
    size_t plen = param.end - param.start;
    size_t tlen = type_name.end - type_name.start;
    return plen == tlen && memcmp(param.start, type_name.start, plen) == 0;
}

bool param_name_eq(Param* a, Param* b) {
    size_t alen = a->name.end - a->name.start;
    size_t blen = b->name.end - b->name.start;
    return alen == blen && memcmp(a->name.start, b->name.start, alen) == 0;
}

static bool body_always_exits(Stmts* body, size_t body_count);

static bool stmt_always_exits_body(Stmts* stmt) {
    if (!stmt) return false;

    switch (stmt->tag) {
        case Stmt_Returns:
        case Stmt_Continues:
            return true;

        case Stmt_Ifs:
            return stmt->data.ifs.else_body_count > 0 &&
                   body_always_exits(stmt->data.ifs.body, stmt->data.ifs.body_count) &&
                   body_always_exits(stmt->data.ifs.else_body, stmt->data.ifs.else_body_count);

        case Stmt_Matchs: {
            if (stmt->data.matchs.default_body_count == 0) return false;

            for (size_t i = 0; i < stmt->data.matchs.cases_count; i++) {
                MatchArm* arm = &stmt->data.matchs.cases[i];
                if (!body_always_exits(arm->body, arm->body_count)) {
                    return false;
                }
            }

            return body_always_exits(stmt->data.matchs.default_body, stmt->data.matchs.default_body_count);
        }

        default:
            return false;
    }
}

static bool body_always_exits(Stmts* body, size_t body_count) {
    if (body_count == 0) return false;
    return stmt_always_exits_body(&body[body_count - 1]);
}

bool body_has_return(Stmts* body, size_t body_count) {
    for (size_t i = 0; i < body_count; i++) {
        Stmts* s = &body[i];
        if (s->tag == Stmt_Returns) return true;

        if (s->tag == Stmt_Ifs) {
            bool in_body = body_has_return(s->data.ifs.body, s->data.ifs.body_count);
            bool in_else_body = body_has_return(s->data.ifs.else_body, s->data.ifs.else_body_count);
            if (in_body && in_else_body) return true;
        }

        if (s->tag == Stmt_Matchs) {
            bool all_return = true;
            for (size_t j = 0; j < s->data.matchs.cases_count; j++) {
                if (!body_has_return(s->data.matchs.cases[j].body, s->data.matchs.cases[j].body_count)) {
                    all_return = false;
                    break;
                }
            }
            if (all_return && s->data.matchs.default_body_count > 0 &&
                body_has_return(s->data.matchs.default_body, s->data.matchs.default_body_count)) {
                return true;
            }
        }
    }
    return false;
}

bool body_has_unreachable(Stmts* body, size_t body_count) {
    for (size_t i = 0; i < body_count; i++) {
        if (stmt_always_exits_body(&body[i])) {
            return i + 1 < body_count;
        }
    }
    return false;
}


bool expr_references_var(Exprs* expr, StringView var) {
    if (!expr) return false;

    switch (expr->tag) {
        case Expr_Identifiers: {
            StringView name = string_range(expr->data.identifiers.name);
            return sv_equal(name, var);
        }

        case Expr_Vars: {
            StringView name = string_range(expr->data.vars.name);
            return sv_equal(name, var);
        }

        case Expr_BinaryOps:
            return expr_references_var(expr->data.binary_ops.left,  var) ||
                   expr_references_var(expr->data.binary_ops.right, var);

        case Expr_Function: {
            for (size_t i = 0; i < expr->data.function_call.param_count; i++)
                ;
            return false;
        }

        case Expr_MethodCalls: {
            if (expr_references_var(expr->data.method_calls.object, var)) return true;
            for (size_t i = 0; i < expr->data.method_calls.args_count; i++) if (expr_references_var(&expr->data.method_calls.args[i], var)) return true;
            return false;
        }

        case Expr_Class_Calls:
        case Expr_Struct_Calls:
        case Expr_Enum_Calls:
        case Expr_Literals:
        default:
            return false;
    }
}

bool stmt_references_var(Stmts* stmt, StringView var) {
    if (!stmt) return false;

    switch (stmt->tag) {
        case Stmt_ExprStmt: return expr_references_var(&stmt->data.expr_stmt.expr, var);
        case Stmt_Returns:  return expr_references_var(&stmt->data.returns.expr,   var);
        case Stmt_Vars:     return expr_references_var(&stmt->data.vars.value,     var);
        case Stmt_Lets:     return expr_references_var(&stmt->data.lets.value,     var);
        case Stmt_Consts:   return expr_references_var(&stmt->data.consts.value,   var);
        case Stmt_Assigns:
            return expr_references_var(&stmt->data.assigns.target, var) ||
                   expr_references_var(&stmt->data.assigns.value,  var);

        case Stmt_Ifs: {
            if (expr_references_var(&stmt->data.ifs.cond, var)) return true;
            for (size_t i = 0; i < stmt->data.ifs.body_count; i++) if (stmt_references_var(&stmt->data.ifs.body[i], var)) return true;
            for (size_t i = 0; i < stmt->data.ifs.else_body_count; i++) if (stmt_references_var(&stmt->data.ifs.else_body[i], var)) return true;
            return false;
        }

        case Stmt_Whiles: {
            if (expr_references_var(&stmt->data.whiles.cond, var)) return true;
            for (size_t i = 0; i < stmt->data.whiles.body_count; i++) if (stmt_references_var(&stmt->data.whiles.body[i], var)) return true;
            return false;
        }

        case Stmt_Fors: {
            StringView for_var = string_range(stmt->data.fors._var);
            if (sv_equal(for_var, var)) return false;
            if (expr_references_var(&stmt->data.fors.iter, var)) return true;
            for (size_t i = 0; i < stmt->data.fors.body_count; i++) if (stmt_references_var(&stmt->data.fors.body[i], var)) return true;
            return false;
        }

        case Stmt_Matchs: {
            if (expr_references_var(&stmt->data.matchs.expr, var)) return true;
            for (size_t i = 0; i < stmt->data.matchs.cases_count; i++) {
                MatchArm* arm = &stmt->data.matchs.cases[i];
                for (size_t j = 0; j < arm->body_count; j++) if (stmt_references_var(&arm->body[j], var)) return true;
            }
            for (size_t i = 0; i < stmt->data.matchs.default_body_count; i++) if (stmt_references_var(&stmt->data.matchs.default_body[i], var)) return true;
            return false;
        }

        default: return false;
    }
}

StringView sv_of(SourceRange r) { return (StringView){ r.start, (size_t)(r.end - r.start) }; }

Type range_to_type(SourceRange r, Register *reg) {
    printf("[!] Usag of rang");
    if (range_eq(r, "int"))     return (Type){ .tag = Type_Int, .data.int_t.bits = 32 };
    if (range_eq(r, "int8"))    return (Type){ .tag = Type_Int, .data.int_t.bits = 8  };
    if (range_eq(r, "int16"))   return (Type){ .tag = Type_Int, .data.int_t.bits = 16 };
    if (range_eq(r, "int32"))   return (Type){ .tag = Type_Int, .data.int_t.bits = 32 };
    if (range_eq(r, "int64"))   return (Type){ .tag = Type_Int, .data.int_t.bits = 64 };
    if (range_eq(r, "uint"))    return (Type){ .tag = Type_Int, .data.int_t.bits = 32, .data.int_t.is_unsigned = true };
    if (range_eq(r, "uint8"))   return (Type){ .tag = Type_Int, .data.int_t.bits = 8,  .data.int_t.is_unsigned = true };
    if (range_eq(r, "uint16"))  return (Type){ .tag = Type_Int, .data.int_t.bits = 16, .data.int_t.is_unsigned = true };
    if (range_eq(r, "uint32"))  return (Type){ .tag = Type_Int, .data.int_t.bits = 32, .data.int_t.is_unsigned = true };
    if (range_eq(r, "uint64"))  return (Type){ .tag = Type_Int, .data.int_t.bits = 64, .data.int_t.is_unsigned = true };
    if (range_eq(r, "float32")) return (Type){ .tag = Type_Float, .data.float_t.bits = 32 };
    if (range_eq(r, "float64")) return (Type){ .tag = Type_Float, .data.float_t.bits = 64 };
    if (range_eq(r, "bool"))    return (Type){ .tag = Type_Bool };
    if (range_eq(r, "char"))    return (Type){ .tag = Type_Char };
    if (range_eq(r, "str"))     return (Type){ .tag = Type_Str  };
    if (range_eq(r, "void"))    return (Type){ .tag = Type_Void };

    RegisterEntry *e = register_get(reg, sv_of(r));
    if (e) return e->type;
    return (Type){ .tag = Type_Custom, .data.custom.name = r };
}

Type get_type(SourceRange r, Type* tree, Register* reg) {
    if (tree) {
        if (tree->tag == Type_Custom && 
            tree->data.custom.name.start == tree->data.custom.name.end) {
            CG_UNREACHABLE_MSG("Parser returned an empty Custom Type (expected valid type).");
        }
        return *tree;
    }

    if (!r.start || r.start == r.end) {
        CG_UNREACHABLE_MSG("Attempted to get type from null/empty SourceRange.");
    }

    return range_to_type(r, reg);
}

SourceRange mangle_method_name(SourceRange class_name, SourceRange method_name) {
    size_t clen = class_name.end - class_name.start;
    size_t mlen = method_name.end - method_name.start;
    size_t total = clen + 1 + mlen;

    char *buf = malloc(total + 1);
    memcpy(buf, class_name.start, clen);
    buf[clen] = '_';
    memcpy(buf + clen + 1, method_name.start, mlen);
    buf[total] = '\0';

    return (SourceRange){ buf, buf + total };
}

Type* type_heap(Type t) {
    switch (t.tag) {
        case Type_Void:
        case Type_Custom:
            return NULL;
        default: {
            Type* p = malloc(sizeof(Type));
            *p = t;
            return p;
        }
    }
}

StringView type_to_sv(Type t) {
    switch (t.tag) {
        case Type_Int:
            if (t.data.int_t.bits == 8)  return SV("int8");
            if (t.data.int_t.bits == 16) return SV("int16");
            if (t.data.int_t.bits == 64) return SV("int64");
            return SV("int32");
        case Type_Float:
            return t.data.float_t.bits == 64 ? SV("float64") : SV("float32");
        case Type_Bool:   return SV("bool");
        case Type_Char:   return SV("char");
        case Type_Str:    return SV("str");
        case Type_Void:   return SV("void");
        case Type_Custom: return (StringView){
            t.data.custom.name.start,
            (size_t)(t.data.custom.name.end - t.data.custom.name.start)
        };
        default: return SV("unknown");
    }
}

SourceRange type_to_source_range(Type t) {
    StringView sv = type_to_sv(t);
    return (SourceRange){ sv.ptr, sv.ptr + sv.len, 0 };
}
