#include "token/lexer.h"
#include "import.h"

typedef struct {
    Lexer lexer;
    LexerToken next;
} Parser;

#define parser_current(self) lexer_peek(&(self)->lexer)

static bool range_eq(SourceRange range, const char* str) {
    size_t len = (size_t)(range.end - range.start);
    return strlen(str) == len && memcmp(range.start, str, len) == 0;
}

Parser parser_new(Lexer lexer) {
    return (Parser){ .lexer = lexer };
}

static LexerToken parser_advance(Parser* self) {
    LexerToken tok = parser_current(self);

    lexer_advance(&self->lexer);

    if (self->next.tag) {
        self->lexer.top = self->next;
        self->next = (LexerToken){0};
    }

    return tok;
}

static LexerToken parser_peek(Parser* self) {
    if (!self->next.tag) {
        Lexer saved = self->lexer;
        lexer_advance(&self->lexer);
        self->next = lexer_peek(&self->lexer);
        self->lexer = saved;
    }

    return self->next;
}

static Stmts parser_stmt(Parser* self);
static Exprs parser_expr(Parser* self);
static Type parser_type(Parser* self);
static Exprs parser_function_call(Parser* self, SourceRange fn);
static Exprs parser_method_calls(Parser* self, SourceRange class_name);
static Exprs parser_struct_call(Parser* self, SourceRange struct_name);
static Exprs parser_enums_call(Parser* self, SourceRange enum_name);
static Stmts parser_functions(Parser* self, bool is_const, bool is_unsafe, bool is_pub);
static Stmts parser_class(Parser* self, bool is_pub);
static Stmts parser_structer(Parser* self, bool is_pub, bool is_unsafe);
static Stmts parser_enums(Parser* self, bool is_pub, bool is_unsafe);
static Stmts parser_traits(Parser* self, bool is_pub, bool is_unsafe);
static Stmts parser_static(Parser* self, bool is_const, bool is_pub);
static Stmts parser_vars(Parser* self, bool is_mut);
static Stmts parser_lets(Parser* self, bool is_mut);
static Stmts parser_if(Parser* self);
static Stmts parser_elif(Parser* self);
static Stmts parser_else(Parser* self);
static Stmts parser_return(Parser* self);
static Exprs parser_expr_primary(Parser* self);

static int parser_precedence(LexerTokenTag tag) {
    switch (tag) {
        case Ors: return 1;
        case Ands: return 2;
        case Pipes: return 3;
        case Carets: return 4;
        case Ampersands: return 5;
        case DoubleEqualss:
        case NotEqualss: return 6;
        case Lesses:
        case Greaters:
        case LessEqualss:
        case GreaterEqualss: return 7;
        case LeftShifts:
        case RightShifts: return 8;
        case Plus:
        case Minuss: return 9;
        case Stars:
        case Slashs:
        case Percents: return 10;
        default: return -1;
    }
}

static Exprs parser_expr_bp(Parser* self, int min_prec) {
    Exprs left = parser_expr_primary(self);

    while (1) {
        LexerToken tok = parser_current(self);
        int prec = parser_precedence(tok.tag);
        if (prec == -1 || prec < min_prec) {
            break;
        }

        LexerTokenTag op = tok.tag;
        parser_advance(self);

        Exprs* left_ptr = checked_malloc(sizeof(*left_ptr));
        Exprs* right_ptr = checked_malloc(sizeof(*right_ptr));
        *left_ptr = left;
        *right_ptr = parser_expr_bp(self, prec + 1);

        left = (Exprs){
            .tag = Expr_BinaryOps,
            .data = { .binary_ops = { .left = left_ptr, .op = op, .right = right_ptr } },
        };
    }

    return left;
}

static Exprs parser_expr(Parser* self) {
    return parser_expr_bp(self, 0);
}

static Exprs parser_expr_primary(Parser* self) {
    LexerToken tok = parser_current(self);

    if (tok.tag == Strings || tok.tag == Ints || tok.tag == Floats ||
        tok.tag == Chars || tok.tag == Trues || tok.tag == Falses) {
        parser_advance(self);
        return (Exprs){
            .tag = Expr_Literals,
            .data = { .literals = { .range = tok.range } },
        };
    }

    if (tok.tag == Identifier) {
        SourceRange name = tok.range;
        LexerToken next = parser_peek(self);
        parser_advance(self);

        if (next.tag == LeftParens || next.tag == LeftBrackets) {
            return parser_function_call(self, name);
        }

        if (next.tag == Dots) {
            return parser_method_calls(self, name);
        }

        return (Exprs){
            .tag = Expr_Identifiers,
            .data = { .identifiers = { .name = name } },
        };
    }

    return (Exprs){0};
}

static Exprs parser_function_call(Parser* self, SourceRange fn) {
    SourceRange generic_params[16];
    size_t generic_params_count = 0;
    Param params[64];
    size_t params_count = 0;

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) {
                p.name = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Colons) {
                parser_advance(self);
            }

            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;

            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }

        if (parser_current(self).tag == RightParens) {
            parser_advance(self);
        }
    }

    return (Exprs){
        .tag = Expr_Function,
        .data = { .function_call = {
            .name = fn,
            .param = params,
            .param_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
        } },
    };
}

static Exprs parser_method_calls(Parser* self, SourceRange class_name) {
    parser_advance(self);
    SourceRange function = {0};
    SourceRange generic_params[16];
    size_t generic_params_count = 0;
    Param params[64];
    size_t params_count = 0;

    if (parser_current(self).tag == Identifier) {
        function = parser_current(self).range;
        parser_advance(self);
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) {
                p.name = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Colons) {
                parser_advance(self);
            }
            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightParens) {
            parser_advance(self);
        }
    }

    return (Exprs){
        .tag = Expr_Class_Calls,
        .data = { .class_calls = {
            .name = class_name,
            .function = function,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .param = params,
            .param_count = params_count,
        } },
    };
}

static Exprs parser_struct_call(Parser* self, SourceRange struct_name) {
    parser_advance(self);
    SourceRange generic_params[16];
    SourceRange function = {0};
    size_t generic_params_count = 0;
    Param params[64];
    size_t params_count = 0;

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == Dots) {
        parser_advance(self);
        if (parser_current(self).tag == Identifier) {
            function = parser_current(self).range;
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == LeftBraces) {
        parser_advance(self);
        while (parser_current(self).tag != RightBraces && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) {
                p.name = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Colons) {
                parser_advance(self);
            }

            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;

            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBraces) {
            parser_advance(self);
        }
    }

    return (Exprs){
        .tag = Expr_Struct_Calls,
        .data = { .struct_calls = {
            .name = struct_name,
            .function = function,
            .param = params,
            .param_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
        } },
    };
}

static Exprs parser_enums_call(Parser* self, SourceRange enum_name) {
    parser_advance(self);
    SourceRange generic_params[16];
    size_t generic_params_count = 0;
    Param params[64];
    size_t params_count = 0;
    SourceRange field = {0};

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == Dots) {
        parser_advance(self);
        if (parser_current(self).tag == Identifier) {
            field = parser_current(self).range;
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) {
                p.name = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Colons) {
                parser_advance(self);
            }
            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightParens) {
            parser_advance(self);
        }
    }

    return (Exprs){
        .tag = Expr_Enum_Calls,
        .data = { .enum_calls = {
            .name = enum_name,
            .field = field,
            .param = params,
            .param_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
        } },
    };
}

static Type parser_type(Parser* self) {
    switch (parser_current(self).tag) {
        case Ints: {
            SourceRange range = parser_current(self).range;
            int bits = range_eq(range, "int8") ? 8 : range_eq(range, "int16") ? 16 : range_eq(range, "int64") ? 64 : 32;
            parser_advance(self);
            return (Type){ .tag = Type_Int, .data = { .int_t = { .bits = bits } } };
        }
        case Floats: {
            SourceRange range = parser_current(self).range;
            int bits = range_eq(range, "float64") ? 64 : 32;
            parser_advance(self);
            return (Type){ .tag = Type_Float, .data = { .float_t = { .bits = bits } } };
        }
        case Chars:
            parser_advance(self);
            return (Type){ .tag = Type_Char };
        case Strings:
            parser_advance(self);
            return (Type){ .tag = Type_Str };
        case Trues:
        case Falses:
            parser_advance(self);
            return (Type){ .tag = Type_Bool };
        case Identifier: {
            SourceRange range = parser_current(self).range;
            parser_advance(self);
            if (range_eq(range, "bool")) return (Type){ .tag = Type_Bool };
            if (range_eq(range, "void")) return (Type){ .tag = Type_Void };
            if (range_eq(range, "str")) return (Type){ .tag = Type_Str };
            if (range_eq(range, "char")) return (Type){ .tag = Type_Char };
            return (Type){ .tag = Type_Custom, .data = { .custom = { .name = range } } };
        }
        default:
            parser_advance(self);
            return (Type){ .tag = Type_Void };
    }
}

static Stmts parser_functions(Parser* self, bool is_const, bool is_unsafe, bool is_pub) {
    (void)is_const;
    parser_advance(self);

    SourceRange name = {0};
    Type return_type = { .tag = Type_Void };
    Param params[64];
    size_t params_count = 0;
    Stmts body[256];
    size_t body_count = 0;
    SourceRange generic_params[16];
    size_t generic_params_count = 0;

    if (parser_current(self).tag == Identifier) {
        name = parser_current(self).range;
        parser_advance(self);
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
    }

    while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
        Param p = {0};
        if (parser_current(self).tag == Identifier) {
            p.name = parser_current(self).range;
            parser_advance(self);
        }
        if (parser_current(self).tag == Colons) {
            parser_advance(self);
        }
        p.c_type = parser_type(self).data.custom.name;
        params[params_count++] = p;
        if (parser_current(self).tag == Commas) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == RightParens) {
        parser_advance(self);
    }
    if (parser_current(self).tag == Colons) {
        parser_advance(self);
        return_type = parser_type(self);
    }

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        body[body_count++] = parser_stmt(self);
    }
    if (parser_current(self).tag == Ends) {
        parser_advance(self);
    }

    return (Stmts){
        .tag = Stmt_Functions,
        .data = { .functions = {
            .name = name,
            .params = params,
            .params_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .return_type = return_type.data.custom.name,
            .body = body,
            .body_count = body_count,
            .is_unsafe = is_unsafe,
            .is_pub = is_pub,
        } },
    };
}

static Stmts parser_class(Parser* self, bool is_pub) {
    parser_advance(self);

    SourceRange name = {0};
    SourceRange parent = {0};
    Param class_params[64];
    size_t class_params_count = 0;
    StructParam fields[64];
    size_t fields_count = 0;
    FunctionMethod methods[64];
    size_t methods_count = 0;
    SourceRange generic_params[16];
    size_t generic_params_count = 0;
    SourceRange traits[16];
    size_t traits_count = 0;

    if (parser_current(self).tag == Fors) {
        parser_advance(self);
        while (parser_current(self).tag == Identifier) {
            traits[traits_count++] = parser_current(self).range;
            parser_advance(self);
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
    }

    if (parser_current(self).tag == Identifier) {
        name = parser_current(self).range;
        parser_advance(self);
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) {
                p.name = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Colons) {
                parser_advance(self);
            }

            p.c_type = parser_type(self).data.custom.name;
            class_params[class_params_count++] = p;

            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightParens) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == Greaters) {
        parser_advance(self);
        if (parser_current(self).tag == Identifier) {
            parent = parser_current(self).range;
            parser_advance(self);
        }
    }

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        if (parser_current(self).tag == Vars || parser_current(self).tag == Lets) {
            StructParam field = {0};
            parser_advance(self);

            if (parser_current(self).tag == Identifier) {
                field.name = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Colons) {
                parser_advance(self);
            }

            field.c_type = parser_type(self).data.custom.name;
            fields[fields_count++] = field;
        } else if (parser_current(self).tag == Functions) {
            Stmts fn = parser_functions(self, false, false, false);
            FunctionMethod method = {0};
            method.name = fn.data.functions.name;
            method.params = fn.data.functions.params;
            method.params_count = fn.data.functions.params_count;
            method.body = fn.data.functions.body;
            method.body_count = fn.data.functions.body_count;
            method.is_pub = fn.data.functions.is_pub;
            method.is_unsafe = fn.data.functions.is_unsafe;
            methods[methods_count++] = method;
        } else {
            parser_advance(self);
        }
    }
    if (parser_current(self).tag == Ends) {
        parser_advance(self);
    }

    return (Stmts){
        .tag = Stmt_Classes,
        .data = { .classes = {
            .name = name,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .class_params = class_params,
            .class_params_count = class_params_count,
            .fields = fields,
            .fields_count = fields_count,
            .methods = methods,
            .methods_count = methods_count,
            .parent = parent,
            .traits = traits,
            .traits_count = traits_count,
            .is_pub = is_pub,
        } },
    };
}

static Stmts parser_structer(Parser* self, bool is_pub, bool is_unsafe) {
    (void)is_unsafe;
    parser_advance(self);

    SourceRange generic_params[16];
    SourceRange name = {0};
    StructParam fields[64];
    size_t fields_count = 0;
    size_t generic_params_count = 0;

    if (parser_current(self).tag == Identifier) {
        name = parser_current(self).range;
        parser_advance(self);
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == Colons) {
        parser_advance(self);
    }

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        StructParam field = {0};
        switch (parser_current(self).tag) {
            case Vars:
                field.mode = (VarMode){ .tag = VarMode_Value, .mutability = Mutability_Mutable };
                parser_advance(self);
                if (parser_current(self).tag == Identifier) {
                    field.name = parser_current(self).range;
                    parser_advance(self);
                }
                if (parser_current(self).tag == Equalss) {
                    parser_advance(self);
                }
                field.c_type = parser_type(self).data.custom.name;
                fields[fields_count++] = field;
                break;
            case Identifier:
                field.mode = (VarMode){ .tag = VarMode_Value, .mutability = Mutability_Immutable };
                field.name = parser_current(self).range;
                parser_advance(self);
                if (parser_current(self).tag == Equalss) {
                    parser_advance(self);
                }
                field.c_type = parser_type(self).data.custom.name;
                fields[fields_count++] = field;
                break;
            default:
                parser_advance(self);
                break;
        }
    }
    if (parser_current(self).tag == Ends) {
        parser_advance(self);
    }

    return (Stmts){
        .tag = Stmt_Structs,
        .data = { .structs = {
            .name = name,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .fields = fields,
            .fields_count = fields_count,
            .is_pub = is_pub,
        } },
    };
}

static Stmts parser_enums(Parser* self, bool is_pub, bool is_unsafe) {
    (void)is_unsafe;
    parser_advance(self);

    EnumVariant variants[64];
    size_t variants_count = 0;
    size_t generic_params_count = 0;
    SourceRange name = {0};
    SourceRange generic_params[16];

    if (parser_current(self).tag == Identifier) {
        name = parser_current(self).range;
        parser_advance(self);
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                generic_params[generic_params_count++] = parser_current(self).range;
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == Colons) {
        parser_advance(self);
    }

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        EnumVariant variant = {0};

        if (parser_current(self).tag == Identifier) {
            variant.name = parser_current(self).range;
            parser_advance(self);

            if (parser_current(self).tag == LeftParens) {
                parser_advance(self);
                EnumField fields[64];
                size_t fields_count = 0;

                while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
                    SourceRange field_name = {0};
                    SourceRange field_type = {0};
                    if (parser_current(self).tag == Identifier) {
                        field_name = parser_current(self).range;
                        parser_advance(self);
                    }
                    if (parser_current(self).tag == Colons) {
                        parser_advance(self);
                    }

                    field_type = parser_type(self).data.custom.name;
                    fields[fields_count++] = (EnumField){
                        .first = checked_malloc(sizeof(SourceRange)),
                        .second = checked_malloc(sizeof(SourceRange)),
                    };
                    *fields[fields_count - 1].first = field_name;
                    *fields[fields_count - 1].second = field_type;

                    if (parser_current(self).tag == Commas) {
                        parser_advance(self);
                    }
                }
                if (parser_current(self).tag == RightParens) {
                    parser_advance(self);
                }

                variant.fields = checked_malloc(sizeof(*variant.fields) * fields_count);
                for (size_t i = 0; i < fields_count; i++) {
                    variant.fields[i] = *fields[i].first;
                    free(fields[i].first);
                    free(fields[i].second);
                }
                variant.fields_count = fields_count;
            }
            variants[variants_count++] = variant;
        }
        if (parser_current(self).tag == Commas) {
            parser_advance(self);
        }
    }
    if (parser_current(self).tag == Ends) {
        parser_advance(self);
    }

    return (Stmts){
        .tag = Stmt_Enums,
        .data = { .enums = {
            .name = name,
            .variants = variants,
            .variants_count = variants_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .is_pub = is_pub,
        } },
    };
}

static Stmts parser_traits(Parser* self, bool is_pub, bool is_unsafe) {
    (void)is_unsafe;
    parser_advance(self);

    TraitMethod methods[64];
    size_t methods_count = 0;
    SourceRange name = {0};

    if (parser_current(self).tag == Identifier) {
        name = parser_current(self).range;
        parser_advance(self);
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) {
                parser_advance(self);
            }
            if (parser_current(self).tag == Commas) {
                parser_advance(self);
            }
        }
        if (parser_current(self).tag == RightBrackets) {
            parser_advance(self);
        }
    }

    if (parser_current(self).tag == Colons) {
        parser_advance(self);
    }

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        switch (parser_current(self).tag) {
            case Types:
                parser_advance(self);
                if (parser_current(self).tag == Identifier) {
                    parser_advance(self);
                }
                break;
            case Functions: {
                Stmts fn = parser_functions(self, false, false, false);
                TraitMethod method = {0};
                method.name = fn.data.functions.name;
                method.params = fn.data.functions.params;
                method.params_count = fn.data.functions.params_count;
                method.body = fn.data.functions.body;
                method.body_count = fn.data.functions.body_count;
                method.is_pub = fn.data.functions.is_pub;
                method.return_type = fn.data.functions.return_type;
                methods[methods_count++] = method;
                break;
            }
            default:
                parser_advance(self);
                break;
        }
    }
    if (parser_current(self).tag == Ends) {
        parser_advance(self);
    }

    return (Stmts){
        .tag = Stmt_Traits,
        .data = { .traits = {
            .name = name,
            .methods = methods,
            .methods_count = methods_count,
            .is_pub = is_pub,
        } },
    };
}

static Stmts parser_return(Parser* self) {
    SourceRange range = parser_current(self).range;
    parser_advance(self);
    Exprs value = {0};

    if (parser_current(self).tag != Ends && parser_current(self).tag != Semicolons && parser_current(self).tag != EOFs) {
        value = parser_expr(self);
    }

    return (Stmts){
        .tag = Stmt_Returns,
        .data = { .returns = {
            .expr = value,
            .range = range,
        } },
    };
}

static Stmts parser_static(Parser* self, bool is_const, bool is_pub) {
    (void)self;
    (void)is_const;
    (void)is_pub;
    return (Stmts){0};
}

static Stmts parser_vars(Parser* self, bool is_mut) {
    (void)self;
    (void)is_mut;
    return (Stmts){0};
}

static Stmts parser_lets(Parser* self, bool is_mut) {
    (void)self;
    (void)is_mut;
    return (Stmts){0};
}

static Stmts parser_if(Parser* self) {
    (void)self;
    return (Stmts){0};
}

static Stmts parser_elif(Parser* self) {
    (void)self;
    return (Stmts){0};
}

static Stmts parser_else(Parser* self) {
    (void)self;
    return (Stmts){0};
}

static Stmts parser_stmt(Parser* self) {
    LexerToken tok = parser_current(self);

    if (tok.tag == Publics) {
        parser_advance(self);
        if (parser_current(self).tag == Unsafes) {
            parser_advance(self);
            switch (parser_current(self).tag) {
                case Functions: return parser_functions(self, false, true, true);
                case Structs: return parser_structer(self, true, true);
                case Enums: return parser_enums(self, true, true);
                case Traits: return parser_traits(self, true, true);
                default: parser_advance(self); break;
            }
        } else {
            switch (parser_current(self).tag) {
                case Functions: return parser_functions(self, false, false, true);
                case Classes: return parser_class(self, true);
                case Structs: return parser_structer(self, true, false);
                case Enums: return parser_enums(self, true, false);
                case Traits: return parser_traits(self, true, false);
                case Locals: return parser_static(self, false, true);
                default: parser_advance(self); break;
            }
        }
    } else if (tok.tag == Unsafes) {
        parser_advance(self);
        switch (parser_current(self).tag) {
            case Functions: return parser_functions(self, false, true, false);
            case Structs: return parser_structer(self, true, false);
            case Enums: return parser_enums(self, true, false);
            case Traits: return parser_traits(self, true, false);
            default: parser_advance(self); break;
        }
    } else if (tok.tag == Consts) {
        parser_advance(self);
        switch (parser_current(self).tag) {
            case Functions: return parser_functions(self, true, false, false);
            case Locals: return parser_static(self, false, true);
            case Vars: return parser_vars(self, false);
            case Lets: return parser_lets(self, false);
            default: parser_advance(self); break;
        }
    } else {
        switch (tok.tag) {
            case Functions: return parser_functions(self, false, false, false);
            case Classes: return parser_class(self, false);
            case Structs: return parser_structer(self, false, false);
            case Enums: return parser_enums(self, false, false);
            case Traits: return parser_traits(self, false, false);
            case Ifs: return parser_if(self);
            case Elifs: return parser_elif(self);
            case Elses: return parser_else(self);
            case Returns: return parser_return(self);
            default: parser_advance(self); break;
        }
    }

    return (Stmts){0};
}
