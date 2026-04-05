#include "ast/ast.h"
#include "../../import.h"

static char peek_char(Lexer* self) {
    return self->source[self->pos + 1];
}

static char advance_char(Lexer* self) {
    return self->source[self->pos++];
}

static char current_char(Lexer* self) {
    return self->source[self->pos];
}

bool range_eq(SourceRange r, const char* str) {
    size_t len = r.end - r.start;
    return strlen(str) == len && memcmp(r.start, str, len) == 0;
}

static LexerToken make_token(LexerTokenTag tag, const char* start, const char* end) {
    return (LexerToken){ .tag = tag, .range = { .start = start, .end = end } };
}

static LexerToken lexer_next_word(Lexer* self) {
    const char* start = self->source + self->pos;

    while (current_char(self) != '\0' &&
           (isalpha(current_char(self)) ||
            isdigit(current_char(self)) ||
            current_char(self) == '_')) {
        self->pos++;
    }

    const char* end = self->source + self->pos;
    SourceRange r = { start, end };

    if (range_eq(r, "func"))     return make_token(Functions,  start, end);
    if (range_eq(r, "return"))   return make_token(Returns,    start, end);
    if (range_eq(r, "if"))       return make_token(Ifs,        start, end);
    if (range_eq(r, "elif"))     return make_token(Elifs,      start, end);
    if (range_eq(r, "else"))     return make_token(Elses,      start, end);
    if (range_eq(r, "while"))    return make_token(Whiles,     start, end);
    if (range_eq(r, "for"))      return make_token(Fors,       start, end);
    if (range_eq(r, "in"))       return make_token(Ins,        start, end);
    if (range_eq(r, "do"))       return make_token(Dos,        start, end);
    if (range_eq(r, "match"))    return make_token(Matchs,     start, end);
    if (range_eq(r, "case"))     return make_token(Cases,      start, end);
    if (range_eq(r, "default"))  return make_token(Defaults,   start, end);
    if (range_eq(r, "class"))    return make_token(Classes,    start, end);
    if (range_eq(r, "trait"))    return make_token(Traits,     start, end);
    if (range_eq(r, "enum"))     return make_token(Enums,      start, end);
    if (range_eq(r, "struct"))   return make_token(Structs,    start, end);
    if (range_eq(r, "type"))     return make_token(Types,      start, end);
    if (range_eq(r, "let"))      return make_token(Lets,       start, end);
    if (range_eq(r, "var"))      return make_token(Vars,       start, end);
    if (range_eq(r, "const"))    return make_token(Consts,     start, end);
    if (range_eq(r, "local"))    return make_token(Locals,     start, end);
    if (range_eq(r, "mut"))      return make_token(Muts,       start, end);
    if (range_eq(r, "ref"))      return make_token(Refs,       start, end);
    if (range_eq(r, "borrow"))   return make_token(Borrows,    start, end);
    if (range_eq(r, "pub"))      return make_token(Publics,    start, end);
    if (range_eq(r, "unsafe"))   return make_token(Unsafes,    start, end);
    if (range_eq(r, "self"))     return make_token(Selfs,      start, end);
    if (range_eq(r, "as"))       return make_token(Ass,        start, end);
    if (range_eq(r, "then"))     return make_token(Thens,      start, end);
    if (range_eq(r, "end"))      return make_token(Ends,       start, end);
    if (range_eq(r, "not"))      return make_token(Nots,       start, end);
    if (range_eq(r, "and"))      return make_token(Ands,       start, end);
    if (range_eq(r, "or"))       return make_token(Ors,        start, end);
    if (range_eq(r, "is"))       return make_token(Iss,        start, end);
    if (range_eq(r, "true"))     return make_token(Trues,      start, end);
    if (range_eq(r, "false"))    return make_token(Falses,     start, end);
    if (range_eq(r, "continue")) return make_token(Continues,  start, end);
    if (range_eq(r, "break"))    return make_token(Breaks,     start, end);
    if (range_eq(r, "int"))      return make_token(Ints,       start, end);
    if (range_eq(r, "int8"))     return make_token(Ints,       start, end);
    if (range_eq(r, "int16"))    return make_token(Ints,       start, end);
    if (range_eq(r, "int32"))    return make_token(Ints,       start, end);
    if (range_eq(r, "int64"))    return make_token(Ints,       start, end);
    if (range_eq(r, "float"))    return make_token(Floats,     start, end);
    if (range_eq(r, "float32"))  return make_token(Floats,     start, end);
    if (range_eq(r, "float64"))  return make_token(Floats,     start, end);
    if (range_eq(r, "char"))     return make_token(Chars,      start, end);
    if (range_eq(r, "string"))   return make_token(Strings,    start, end);

    return make_token(Identifier, start, end);
}

static LexerToken lexer_next_number(Lexer* self) {
    const char* start = self->source + self->pos;

    while (isdigit(current_char(self))) self->pos++;

    if (current_char(self) == '.' && isdigit(peek_char(self))) {
        self->pos++;
        while (isdigit(current_char(self))) self->pos++;
        return make_token(Floats, start, self->source + self->pos);
    }

    return make_token(Ints, start, self->source + self->pos);
}

static LexerToken lexer_next_string(Lexer* self) {
    const char* start = self->source + self->pos;
    advance_char(self);

    while (current_char(self) != '"' && current_char(self) != '\0') {
        if (current_char(self) == '\\') advance_char(self);
        advance_char(self);
    }

    advance_char(self);
    return make_token(Strings, start, self->source + self->pos);
}

static LexerToken lexer_next_char_literal(Lexer* self) {
    const char* start = self->source + self->pos;
    advance_char(self);

    if (current_char(self) == '\\') advance_char(self);
    advance_char(self);
    advance_char(self);

    return make_token(Chars, start, self->source + self->pos);
}

LexerToken lexer_next(Lexer* self) {
    while (current_char(self) == ' '  || current_char(self) == '\t' ||
           current_char(self) == '\r' || current_char(self) == '\n') {
        advance_char(self);
    }

    const char* start = self->source + self->pos;
    char c = advance_char(self);

    if (isalpha(c) || c == '_') { self->pos--; return lexer_next_word(self);        }
    if (isdigit(c)) { self->pos--; return lexer_next_number(self);      }
    if (c == '"') { self->pos--; return lexer_next_string(self);      }
    if (c == '\'') { self->pos--; return lexer_next_char_literal(self);}

    switch (c) {
        case '+': if (current_char(self) == '=') { advance_char(self); return make_token(PlusEqualss,      start, self->source + self->pos); } return make_token(Plus,       start, self->source + self->pos);
        case '-': if (current_char(self) == '=') { advance_char(self); return make_token(MinusEqualss,     start, self->source + self->pos); } return make_token(Minuss,     start, self->source + self->pos);
        case '*': if (current_char(self) == '=') { advance_char(self); return make_token(StarEqualss,      start, self->source + self->pos); } return make_token(Stars,      start, self->source + self->pos);
        case '%': if (current_char(self) == '=') { advance_char(self); return make_token(PercentEqualss,   start, self->source + self->pos); } return make_token(Percents,   start, self->source + self->pos);
        case '=': if (current_char(self) == '=') { advance_char(self); return make_token(DoubleEqualss,    start, self->source + self->pos); } return make_token(Equalss,    start, self->source + self->pos);
        case '!': if (current_char(self) == '=') { advance_char(self); return make_token(NotEqualss,       start, self->source + self->pos); } return make_token(Bangs,      start, self->source + self->pos);
        case '^': if (current_char(self) == '=') { advance_char(self); return make_token(CaretEqualss,     start, self->source + self->pos); } return make_token(Carets,     start, self->source + self->pos);
        case '&':
            if (current_char(self) == '&') { advance_char(self); return make_token(Ands,                 start, self->source + self->pos); }
            if (current_char(self) == '=') { advance_char(self); return make_token(AmpersandEqualss,     start, self->source + self->pos); }
            return make_token(Ampersands, start, self->source + self->pos);
        case '|':
            if (current_char(self) == '|') { advance_char(self); return make_token(Ors,                  start, self->source + self->pos); }
            if (current_char(self) == '=') { advance_char(self); return make_token(PipeEqualss,          start, self->source + self->pos); }
            return make_token(Pipes, start, self->source + self->pos);

        case '<':
            if (current_char(self) == '=') { advance_char(self); return make_token(LessEqualss,          start, self->source + self->pos); }
            if (current_char(self) == '<') { advance_char(self);
                if (current_char(self) == '=') { advance_char(self); return make_token(LeftShiftEqualss, start, self->source + self->pos); }
                return make_token(LeftShifts, start, self->source + self->pos);
            }
            return make_token(Lesses, start, self->source + self->pos);

        case '>':
            if (current_char(self) == '=') { advance_char(self); return make_token(GreaterEqualss,        start, self->source + self->pos); }
            if (current_char(self) == '>') { advance_char(self);
                if (current_char(self) == '=') { advance_char(self); return make_token(RightShiftEqualss, start, self->source + self->pos); }
                return make_token(RightShifts, start, self->source + self->pos);
            }
            return make_token(Greaters, start, self->source + self->pos);

        case '.':
            if (current_char(self) == '.') { advance_char(self);
                if (current_char(self) == '.') { advance_char(self); return make_token(TrippleDots, start, self->source + self->pos); }
                return make_token(DoubleDots, start, self->source + self->pos);
            }
            return make_token(Dots, start, self->source + self->pos);

        case '/':
            if (current_char(self) == '/') {
                while (current_char(self) != '\n' && current_char(self) != '\0') advance_char(self);
                return lexer_next(self);
            }
            if (current_char(self) == '*') { advance_char(self);
                while (!(current_char(self) == '*' && peek_char(self) == '/')) advance_char(self);
                advance_char(self); 
                advance_char(self);
    
                return lexer_next(self); 
            }
            if (current_char(self) == '=') { advance_char(self); return make_token(SlashEqualss,      start, self->source + self->pos); }
            return make_token(Slashs, start, self->source + self->pos);

        case '~':  return make_token(Tildes,        start, self->source + self->pos);
        case '(':  return make_token(LeftParens,     start, self->source + self->pos);
        case ')':  return make_token(RightParens,    start, self->source + self->pos);
        case '[':  return make_token(LeftBrackets,   start, self->source + self->pos);
        case ']':  return make_token(RightBrackets,  start, self->source + self->pos);
        case '{':  return make_token(LeftBraces,     start, self->source + self->pos);
        case '}':  return make_token(RightBraces,    start, self->source + self->pos);
        case ',':  return make_token(Commas,         start, self->source + self->pos);
        case ':':  return make_token(Colons,         start, self->source + self->pos);
        case ';':  return make_token(Semicolons,     start, self->source + self->pos);
        case '@':  return make_token(Ats,            start, self->source + self->pos);
        case '#':  return make_token(Hashs,          start, self->source + self->pos);
        case '?':  return make_token(Questions,      start, self->source + self->pos);
        case '\\': return make_token(Backslashs,     start, self->source + self->pos);
        case '`':  return make_token(Backticks,      start, self->source + self->pos);
        case '_':  return make_token(Underscores,    start, self->source + self->pos);
        case '\0': return make_token(EOFs,           start, self->source + self->pos);

        default: printf("Unknown char: %c\n", c); return lexer_next(self);
    }
}

LexerToken* Tokenize(Lexer* self) {
    size_t capacity = 64;
    size_t count    = 0;
    LexerToken* tokens = malloc(sizeof(LexerToken) * capacity);

    while (1) {
        LexerToken tok = lexer_next(self);
        if (count >= capacity) {
            capacity *= 2;
            tokens = realloc(tokens, sizeof(LexerToken) * capacity);
        }
        tokens[count++] = tok;
        if (tok.tag == EOFs) break;
    }

    return tokens;
}