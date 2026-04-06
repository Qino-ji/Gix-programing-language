#include <./ast/ast.h>
#include <../../../import.h>

typedef struct {
    LexerToken* cur;
} Parser;

bool range_eq(SourceRange r, const char* str) {
    size_t len = r.end - r.start;
    return strlen(str) == len && memcmp(r.start, str, len) == 0;
}

Parser parser_new(LexerToken* tokens) {
    return (Parser){ .cur = tokens };
}

LexerToken parser_current(Parser* self) {
    return self->cur[0];
}

LexerToken parser_advance(Parser* self) {
    LexerToken tok = self->cur[0];
    self->cur++;
    return tok;
}

LexerToken parser_peek(Parser* self) {
    return self->cur[1];
}

bool is_type(SourceRange tok) {
    return range_eq(tok, "int")     ||
           range_eq(tok, "int8")    ||
           range_eq(tok, "int16")   ||
           range_eq(tok, "int32")   ||
           range_eq(tok, "int64")   ||
           range_eq(tok, "float")   ||
           range_eq(tok, "float32") ||
           range_eq(tok, "float64") ||
           range_eq(tok, "char")    ||
           range_eq(tok, "string");
}

Stmts parser_stmt(Parser* self);
Exprs parser_expr(Parser* self);
Type parser_type(Parser* self);
Exprs parser_function_call(Parser* self, SourceRange fn);
Exprs parser_method_calls(Parser* self, SourceRange class);
Exprs parser_struct_call(Parser* self, SourceRange str);
Exprs parser_enums_call(Parser* self, SourceRange en);
Stmts parser_functions(Parser* self, bool is_const, bool is_unsafe, bool is_pub);
Stmts parser_class(Parser* self, bool is_pub);
Stmts parser_structer(Parser* self, bool is_pub, bool is_unsafe);
Stmts parser_enums(Parser* self, bool is_pub, bool is_unsafe);
Stmts parser_traits(Parser* self, bool is_pub, bool is_unsafe);
Stmts parser_static(Parser* self, bool is_const, bool is_pub);
Stmts parser_vars(Parser* self, bool is_mut);
Stmts parser_lets(Parser* self, bool is_mut);
Stmts parser_if(Parser* self);
Stmts parser_elif(Parser* self);
Stmts parser_else(Parser* self);
Stmts parser_return(Parser* self);

int parser_precedence(LexerTokenTag tag) {
    switch (tag) {
        case Ors:                                        return 1;
        case Ands:                                       return 2;
        case Pipes:                                      return 3;
        case Carets:                                     return 4;
        case Ampersands:                                 return 5;
        case DoubleEqualss: case NotEqualss:             return 6;
        case Lesses: case Greaters:
        case LessEqualss: case GreaterEqualss:           return 7;
        case LeftShifts: case RightShifts:               return 8;
        case Plus: case Minuss:                          return 9;
        case Stars: case Slashs: case Percents:          return 10;
        default:                                         return -1;
    }
}

Exprs parser_expr_primary(Parser* self);

Exprs parser_expr_bp(Parser* self, int min_prec) {
    Exprs left = parser_expr_primary(self);

    while (1) {
        LexerToken tok = parser_current(self);
        int prec = parser_precedence(tok.tag);
        if (prec == -1 || prec < min_prec) break;

        LexerTokenTag op = tok.tag;
        parser_advance(self);

        Exprs* l = malloc(sizeof(Exprs));
        Exprs* r = malloc(sizeof(Exprs));
        *l = left;
        *r = parser_expr_bp(self, prec + 1);

        left = (Exprs){
            .tag  = Expr_BinaryOps,
            .data = { .binary_ops = { .left = l, .op = op, .right = r }}
        };
    }

    return left;
}

Exprs parser_expr(Parser* self) {
    return parser_expr_bp(self, 0);
}

Exprs parser_expr_primary(Parser* self) {
    LexerToken tok = parser_current(self);
    SourceRange n = tok.range;

    if (tok.tag == Strings || tok.tag == Ints  ||
        tok.tag == Floats  || tok.tag == Chars ||
        tok.tag == Trues   || tok.tag == Falses) {
        parser_advance(self);
        return (Exprs){
            .tag  = Expr_Literals,
            .data = { .literals = { .range = tok.range }}
        };
    }

    if (tok.tag == Identifier) {
        SourceRange name = tok.range;
        parser_advance(self);

        LexerToken next = parser_current(self);
        if (next.tag == LeftParens || next.tag == LeftBrackets)
            return parser_function_call(self, name);

        if (next.tag == Dots)
            return parser_method_calls(self, name);

        return (Exprs){
            .tag  = Expr_Identifiers,
            .data = { .identifiers = { .name = name }}
        };
    }

    return (Exprs){0};
}

Exprs parser_function_call(Parser* self, SourceRange fn) {
    parser_advance(self);
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
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) { p.name = parser_current(self).range; parser_advance(self); }
            if (parser_current(self).tag == Colons) parser_advance(self);
    
            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;
    
            if (parser_current(self).tag == Commas) parser_advance(self);
        }

        if (parser_current(self).tag == RightParens) parser_advance(self);
    }

    return (Exprs){
        .tag = Expr_Function,
        .data = { .function_call = {
            .name = fn,
            .param = params,
            .param_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count
        }}
    };
}

Exprs parser_method_calls(Parser* self, SourceRange class) {
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
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) { 
                p.name = parser_current(self).range; 
                parser_advance(self); 
            }
            if (parser_current(self).tag == Colons) parser_advance(self);
            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightParens) parser_advance(self);
    }

    return (Exprs){
        .tag = Expr_Class_Calls,
        .data = { .class_calls = {
            .name = class,
            .function = function,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .param = params,
            .param_count = params_count
        }}
    };
}

Exprs parser_struct_call(Parser* self, SourceRange str) {
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
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
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
            if (parser_current(self).tag == Identifier) {  p.name = parser_current(self).range; parser_advance(self); }
            if (parser_current(self).tag == Colons) parser_advance(self);

            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;

            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBraces) parser_advance(self);
    }

    return (Exprs){
        .tag = Expr_Struct_Calls,
        .data = { .struct_calls = {
            .name = str,
            .function = function,
            .param = params,
            .param_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count
        }}
    };
}

Exprs parser_enums_call(Parser* self, SourceRange en) {
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
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
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
            if (parser_current(self).tag == Colons) parser_advance(self);
            p.c_type = parser_type(self).data.custom.name;
            params[params_count++] = p;
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightParens) parser_advance(self);
    }

    return (Exprs){
        .tag = Expr_Enum_Calls,
        .data = { .enum_calls = {
            .name = en,
            .field = field,
            .param = params,
            .param_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count
        }}
    };
}

Type parser_type(Parser* self) {
    switch (parser_current(self).tag) {
        case Ints: {
            SourceRange r = parser_current(self).range;
            int bits = range_eq(r, "int8") ? 8 : range_eq(r, "int16") ? 16 : range_eq(r, "int64") ? 64 : 32;
            parser_advance(self);

            return (Type){ .tag = Type_Int, .data = { .int_t = { .bits = bits } } };
        }
        case Floats: {
            SourceRange r = parser_current(self).range;
            int bits = range_eq(r, "float64") ? 64 : range_eq(r, "float32") ? 32 : 32;
            parser_advance(self);

            return (Type){ .tag = Type_Float, .data = { .float_t = { .bits = bits } } };
        }
        case Chars: { parser_advance(self); return (Type){ .tag = Type_Char }; }
        case Strings: { parser_advance(self); return (Type){ .tag = Type_Str  }; }
        case Trues: { parser_advance(self); return (Type){ .tag = Type_Bool }; }
        case Falses: { parser_advance(self); return (Type){ .tag = Type_Bool }; }
        case Identifier: {
            SourceRange r = parser_current(self).range;
            parser_advance(self);
            if (range_eq(r, "bool")) return (Type){ .tag = Type_Bool };
            if (range_eq(r, "void")) return (Type){ .tag = Type_Void };
            if (range_eq(r, "str"))  return (Type){ .tag = Type_Str  };
            if (range_eq(r, "char")) return (Type){ .tag = Type_Char };
            return (Type){ .tag = Type_Custom, .data = { .custom = { .name = r } } };
        }
        default: parser_advance(self); return (Type){ .tag = Type_Void };
    }
}

Stmts parser_functions(Parser* self, bool is_const, bool is_unsafe, bool is_pub) {
    parser_advance(self);

    SourceRange n = {0};
    Type return_type = { .tag = Type_Void };
    Param params[64];
    size_t params_count = 0;
    Stmts body[256];
    size_t body_count = 0;
    SourceRange generic_params[16];
    size_t generic_params_count = 0;

    if (parser_current(self).tag == Identifier) {  
        n = parser_current(self).range; 
        parser_advance(self); 
    }
    
    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) { 
                generic_params[generic_params_count++] = parser_current(self).range; 
                parser_advance(self); 
            }
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == LeftParens) { parser_advance(self); }

    while (parser_current(self).tag != RightParens) {
        Param p = {0};
        if (parser_current(self).tag == Identifier) { 
            p.name = parser_current(self).range; 
            parser_advance(self); 
        }
        if (parser_current(self).tag == Colons) { parser_advance(self); }
        p.c_type = parser_type(self).data.custom.name;
        params[params_count++] = p;
        if (parser_current(self).tag == Commas) parser_advance(self);
    }

    if (parser_current(self).tag == RightParens) parser_advance(self);
    if (parser_current(self).tag == Colons) { 
        parser_advance(self); 
        return_type = parser_type(self); 
    }

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) { 
        body[body_count++] = parser_stmt(self); 
    }
    if (parser_current(self).tag == Ends) parser_advance(self);

    return (Stmts){
        .tag = Stmt_Functions,
        .data = { .functions = {
            .name = n,
            .params = params,
            .params_count = params_count,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .return_type = return_type.data.custom.name,
            .body = body,
            .body_count = body_count,
            .is_unsafe = is_unsafe,
            .is_pub = is_pub,
        }}
    };
}

Stmts parser_class(Parser* self, bool is_pub) {
    parser_advance(self);

    SourceRange n = {0};
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
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
    }

    if (parser_current(self).tag == Identifier) {  
        n = parser_current(self).range; 
        parser_advance(self); 
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) { 
                generic_params[generic_params_count++] = parser_current(self).range; 
                parser_advance(self); 
            }
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == LeftParens) {
        parser_advance(self);
        while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
            Param p = {0};
            if (parser_current(self).tag == Identifier) { p.name = parser_current(self).range; parser_advance(self); }
            if (parser_current(self).tag == Colons) { parser_advance(self); }
    
            p.c_type = parser_type(self).data.custom.name;
            class_params[class_params_count++] = p;

            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightParens) parser_advance(self);
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
            StructParam f = {0};
            parser_advance(self);

            if (parser_current(self).tag == Identifier) { f.name = parser_current(self).range;  parser_advance(self); }
            if (parser_current(self).tag == Colons) { parser_advance(self); }

            f.c_type = parser_type(self).data.custom.name;
            fields[fields_count++] = f;
        } else if (parser_current(self).tag == Functions) {
            Stmts fn = parser_functions(self, false, false, false);
            FunctionMethod m = {0};
            m.name = fn.data.functions.name;
            m.params = fn.data.functions.params;
            m.params_count = fn.data.functions.params_count;
            m.body = fn.data.functions.body;
            m.body_count = fn.data.functions.body_count;
            m.is_pub = fn.data.functions.is_pub;
            m.is_unsafe = fn.data.functions.is_unsafe;
            methods[methods_count++] = m;
        } else {
            parser_advance(self);
        }
    }
    if (parser_current(self).tag == Ends) parser_advance(self);

    return (Stmts){
        .tag = Stmt_Classes,
        .data = { .classes = {
            .name = n,
            .class_params = class_params,
            .class_params_count = class_params_count,
            .fields = fields,
            .fields_count = fields_count,
            .methods = methods,
            .methods_count = methods_count,
            .parent = parent,
            .is_pub = is_pub,
        }}
    };
}

Stmts parser_structer(Parser* self, bool is_pub, bool is_unsafe) {
    parser_advance(self);

    SourceRange generic_params[16];
    SourceRange n = {0};
    StructParam fields[64];
    size_t fields_count = 0;
    size_t generic_params_count = 0;

    if (parser_current(self).tag == Identifier) {  
        n = parser_current(self).range; 
        parser_advance(self); 
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) { 
                generic_params[generic_params_count++] = parser_current(self).range; 
                parser_advance(self); 
            }
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == Colons) parser_advance(self);

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        StructParam f = {0};
        switch (parser_current(self).tag) {
            case Vars: {
                f.mode = (VarMode){ .tag = VarMode_Value, .mutability = Mutability_Mutable };
                parser_advance(self);

                if (parser_current(self).tag == Identifier) { f.name = parser_current(self).range;  parser_advance(self); }
                if (parser_current(self).tag == Equalss) { parser_advance(self); }

                f.c_type = parser_type(self).data.custom.name;
                fields[fields_count++] = f;

                break;
            }
            case Identifier: {
                f.mode = (VarMode){ .tag = VarMode_Value, .mutability = Mutability_Immutable };
                f.name = parser_current(self).range;
                parser_advance(self);

                if (parser_current(self).tag == Equalss) { parser_advance(self); }

                f.c_type = parser_type(self).data.custom.name;
                fields[fields_count++] = f;

                break;
            }
            default: parser_advance(self); break;
        }
    }
    if (parser_current(self).tag == Ends) parser_advance(self);

    return (Stmts){
        .tag = Stmt_Structs,
        .data = { .structs = {
            .name = n,
            .generic_params = generic_params,
            .generic_params_count = generic_params_count,
            .fields = fields,
            .fields_count = fields_count,
            .is_pub = is_pub,
        }}
    };
}

Stmts parser_enums(Parser* self, bool is_pub, bool is_unsafe) {
    parser_advance(self);

    EnumVariant variants[64];
    size_t variants_count = 0;
    size_t generic_params_count = 0;
    SourceRange n = {0};
    SourceRange generic_params[16];

    if (parser_current(self).tag == Identifier) {  
        n = parser_current(self).range; 
        parser_advance(self); 
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) { 
                generic_params[generic_params_count++] = parser_current(self).range; 
                parser_advance(self); 
            }
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == Colons) parser_advance(self);

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        EnumVariant v = {0};
        v.fields = NULL;
        v.fields_count = 0;

        if (parser_current(self).tag == Identifier) {
            v.name = parser_current(self).range;
            parser_advance(self);

            if (parser_current(self).tag == LeftParens) {
                parser_advance(self);
                EnumField fields[64];
                size_t fields_count = 0;

                while (parser_current(self).tag != RightParens && parser_current(self).tag != EOFs) {
                    SourceRange fname = {0};
                    SourceRange ftype = {0};
                    if (parser_current(self).tag == Identifier) { fname = parser_current(self).range;  parser_advance(self); }
                    if (parser_current(self).tag == Colons) { parser_advance(self); }

                    ftype = parser_type(self).data.custom.name;
                    fields[fields_count++] = (EnumField){ fname, ftype };

                    if (parser_current(self).tag == Commas) parser_advance(self);
                }
                if (parser_current(self).tag == RightParens) parser_advance(self);

                v.fields = malloc(sizeof(*v.fields) * fields_count);
                if (v.fields) {
                    memcpy(v.fields, fields, sizeof(*v.fields) * fields_count);
                    v.fields_count = fields_count;
                }
            }
            variants[variants_count++] = v;
        }
        if (parser_current(self).tag == Commas) parser_advance(self);
    }
    if (parser_current(self).tag == Ends) parser_advance(self);

    return (Stmts){
        .tag = Stmt_Enums,
        .data = { .enums = {
            .name  = n,
            .variants  = variants,
            .generic_params  = generic_params,
            .generic_params_count = generic_params_count,
            .variants_count = variants_count,
            .is_pub = is_pub,
        }}
    };
}

Stmts parser_traits(Parser* self, bool is_pub, bool is_unsafe) {
    parser_advance(self);

    TraitMethod methods[64];
    size_t methods_count = 0;
    size_t types_count = 0;
    size_t generic_params_count = 0;
    SourceRange n = {0};
    SourceRange generic_params[16];
    SourceRange types[16];

    if (parser_current(self).tag == Identifier) {  
        n = parser_current(self).range; 
        parser_advance(self); 
    }

    if (parser_current(self).tag == LeftBrackets) {
        parser_advance(self);
        while (parser_current(self).tag != RightBrackets && parser_current(self).tag != EOFs) {
            if (parser_current(self).tag == Identifier) { 
                generic_params[generic_params_count++] = parser_current(self).range; 
                parser_advance(self); 
            }
            if (parser_current(self).tag == Commas) parser_advance(self);
        }
        if (parser_current(self).tag == RightBrackets) parser_advance(self);
    }

    if (parser_current(self).tag == Colons) parser_advance(self);

    while (parser_current(self).tag != Ends && parser_current(self).tag != EOFs) {
        switch (parser_current(self).tag) {
            case Types: {
                parser_advance(self);
                if (parser_current(self).tag == Identifier) { 
                    types[types_count++] = parser_current(self).range; 
                    parser_advance(self); 
                }
                break;
            }
            case Functions: {
                Stmts fn = parser_functions(self, false, false, false);
                TraitMethod m = {0};
                m.name = fn.data.functions.name;
                m.params = fn.data.functions.params;
                m.params_count = fn.data.functions.params_count;
                m.body = fn.data.functions.body;
                m.body_count = fn.data.functions.body_count;
                m.is_pub = fn.data.functions.is_pub;
                m.return_type = fn.data.functions.return_type;
                methods[methods_count++] = m;
                break;
            }
            default: parser_advance(self); break;
        }
    }
    if (parser_current(self).tag == Ends) parser_advance(self);

    return (Stmts){
        .tag = Stmt_Traits,
        .data = { .traits = {
            .name = n,
            .methods = methods,
            .methods_count = methods_count,
            .is_pub = is_pub,
        }}
    };
}

Stmts parser_return(Parser* self) {
    SourceRange range = {0};
    range = parser_current(self).range;
    parser_advance(self);
    Exprs value = {0};

    if (parser_current(self).tag != Ends &&
        parser_current(self).tag != Semicolons &&
        parser_current(self).tag != EOFs) {
        value = parser_expr(self);
    }

    return (Stmts){
        .tag = Stmt_Returns,
        .data = { .returns = {
            .expr  = value,
            .range = range
        }}
    };
}

Stmts parser_static(Parser* self, bool is_const, bool is_pub) { return (Stmts){0}; }
Stmts parser_vars(Parser* self, bool is_mut) { return (Stmts){0}; }
Stmts parser_lets(Parser* self, bool is_mut) { return (Stmts){0}; }
Stmts parser_if(Parser* self) { return (Stmts){0}; }
Stmts parser_elif(Parser* self) { return (Stmts){0}; }
Stmts parser_else(Parser* self) { return (Stmts){0}; }

Stmts parser_stmt(Parser* self) {
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