#include "import.h"
#include "register.h"
#include "error.h"
#include "helper.h"

static Type apply_bindings(Type t, GenericBinding* bindings, size_t count) {
    if (t.tag != Type_Custom) return t;
    size_t tlen = t.data.custom.name.end - t.data.custom.name.start;

    for (size_t i = 0; i < count; i++) {
        size_t plen = bindings[i].param.end - bindings[i].param.start;

        if (plen == tlen && memcmp(bindings[i].param.start, t.data.custom.name.start, plen) == 0) return bindings[i].bound;
    }

    return t;
}

char* mangle_name(StringView base, GenericBinding* bindings, size_t count) {
    size_t total = base.len + 1;
    size_t pos = 0;
    char* out = (char*)malloc(total);

    for (size_t i = 0; i < count; i++) {
        total += 2;
        total += type_to_sv(bindings[i].bound).len;
    }
   
    memcpy(out + pos, base.ptr, base.len); pos += base.len;
    for (size_t i = 0; i < count; i++) {
        out[pos++] = '_'; out[pos++] = '_';
        StringView bsv = type_to_sv(bindings[i].bound);
        memcpy(out + pos, bsv.ptr, bsv.len); pos += bsv.len;
    }
    out[pos] = '\0';
    return out;
}

void resolve_generic_call(StringView def_name, SourceRange* call_generic_args, size_t call_generic_count, Register* reg, GenericRegistry* mono, CheckerErrList* errors, SourceRange error_range) {
    GenericParam* def_params = NULL;
    size_t def_params_count = 0;
    RegisterEntry* entry = register_get(reg, def_name);
    ARR(GenericBinding) bindings_arr = {0};
    char* mangled = mangle_name(def_name, bindings_arr.data, bindings_arr.len);
    StringView mangled_sv = { mangled, strlen(mangled) };
    khint_t existing = mono_table_get(mono->table, mangled_sv);
    int absent = 0;

    if (!entry) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_VSF,
            .data.vsf = { .range = error_range, .var_name = def_name }
        });
        return;
    }

    switch (entry->tag) {
    case Reg_Function: def_params = entry->data.function.generic_params; def_params_count = entry->data.function.generic_params_count; break;
    case Reg_Struct: def_params = entry->data.strct.generic_params; def_params_count = entry->data.strct.generic_params_count; break;
    case Reg_Enum: def_params = entry->data.enm.generic_params; def_params_count = entry->data.enm.generic_params_count; break;
    case Reg_Class: def_params = entry->data._class.generic_params; def_params_count = entry->data._class.generic_params_count; break;
        default:
            checker_err_push(errors, (CheckerErr){
                .tag = Err_Tag_TNC,
                .data.tnc = {
                    .range       = error_range,
                    .type_name   = def_name,
                    .actual_kind = string("non-generic"),
                }
            });
            return;
    }

    if (def_params_count != call_generic_count) {
        checker_err_push(errors, (CheckerErr){
            .tag = Err_Tag_WFC,
            .data.wfc = {
                .range = error_range,
                .expected_count = def_params_count,
                .actual_count = call_generic_count,
            }
        });
        return;
    }

    if (call_generic_count == 0) return;

    for (size_t i = 0; i < call_generic_count; i++) {
        ARR_PUSH(bindings_arr, ((GenericBinding){
            .param = def_params[i].name,   // was def_params[i]
            .bound = range_to_type(call_generic_args[i], reg),
        }));
    }

    if (existing != kh_end(mono->table)) {
        free(mangled);
        ARR_FREE(bindings_arr);
        return;
    }

    khint_t it = mono_table_put(mono->table, mangled_sv, &absent);
    kh_val(mono->table, it) = (GenericInstance){
        .mangled_name    = mangled,
        .bindings        = bindings_arr.data,
        .bindings_count  = bindings_arr.len,
        .def_kind        = entry->tag,
        .def_name        = def_name,
    };

    switch (entry->tag) {
        case Reg_Struct: {
            ARR(StructParam) new_fields = {0};

            for (size_t i = 0; i < entry->data.strct.fields_count; i++) {
                StructParam f = entry->data.strct.fields[i];
                Type bound = apply_bindings(
                    range_to_type(f.c_type, reg),
                    bindings_arr.data, bindings_arr.len
                );
                f.c_type = type_to_source_range(bound);
                ARR_PUSH(new_fields, f);
            }
            register_insert(reg, mangled_sv, (RegisterEntry){
                .tag  = Reg_Struct,
                .name = NULL,
                .type = (Type){ .tag = Type_Custom, .data.custom.name = error_range },
                .data.strct = {
                    .fields       = new_fields.data,
                    .fields_count = new_fields.len,
                    .is_pub       = entry->data.strct.is_pub,
                }
            });
            break;
        }

        case Reg_Enum: {
            ARR(EnumVariant) new_variants = {0};
            for (size_t v = 0; v < entry->data.enm.variants_count; v++) {
                EnumVariant var = entry->data.enm.variants[v];
                ARR(EnumField) new_fields = {0};
                for (size_t f = 0; f < var.fields_count; f++) {
                    EnumField ef = var.fields[f];
                    Type bound = apply_bindings(
                        range_to_type(ef.second, reg),
                        bindings_arr.data, bindings_arr.len
                    );
                    ef.second = type_to_source_range(bound);
                    ARR_PUSH(new_fields, ef);
                }
                var.fields       = new_fields.data;
                var.fields_count = new_fields.len;
                ARR_PUSH(new_variants, var);
            }
            register_insert(reg, mangled_sv, (RegisterEntry){
                .tag = Reg_Enum,
                .name = NULL,
                .type = (Type){ .tag = Type_Custom, .data.custom.name = error_range },
                .data.enm = {
                    .variants = new_variants.data,
                    .variants_count = new_variants.len,
                    .is_pub = entry->data.enm.is_pub,
                }
            });
            break;
        }

        case Reg_Class: {
            ARR(StructParam) new_fields = {0};
            for (size_t i = 0; i < entry->data._class.fields_count; i++) {
                StructParam f = entry->data._class.fields[i];
                Type bound = apply_bindings(range_to_type(f.c_type, reg), bindings_arr.data, bindings_arr.len);

                f.c_type = type_to_source_range(bound);
                ARR_PUSH(new_fields, f);
            }

            ARR(FunctionMethod) new_methods = {0};
            for (size_t m = 0; m < entry->data._class.methods_count; m++) {
                FunctionMethod meth = entry->data._class.methods[m];
                Type ret_bound = apply_bindings(range_to_type(meth.return_type, reg), bindings_arr.data, bindings_arr.len);
                ARR(Param) new_params = {0};

                meth.return_type = type_to_source_range(ret_bound);

                for (size_t p = 0; p < meth.params_count; p++) {
                    Param param = meth.params[p];
                    Type pt = apply_bindings(
                        range_to_type(param.c_type, reg),
                        bindings_arr.data, bindings_arr.len
                    );
                    param.c_type = type_to_source_range(pt);
                    ARR_PUSH(new_params, param);
                }
                meth.params       = new_params.data;
                meth.params_count = new_params.len;
                ARR_PUSH(new_methods, meth);
            }
            register_insert(reg, mangled_sv, (RegisterEntry){
                .tag  = Reg_Class,
                .name = NULL,
                .type = (Type){ .tag = Type_Custom, .data.custom.name = error_range },
                .data._class = {
                    .fields = new_fields.data,
                    .fields_count = new_fields.len,
                    .methods = new_methods.data,
                    .methods_count = new_methods.len,
                    .is_pub = entry->data._class.is_pub,
                    .attached_tag = entry->data._class.attached_tag,
                    .attached_fields = entry->data._class.attached_fields,
                    .attached_fields_count= entry->data._class.attached_fields_count,
                }
            });
            break;
        }

        case Reg_Function: {
            Type ret_bound = apply_bindings(range_to_type(entry->data.function.return_type.tag == Type_Void ? (SourceRange){0} : error_range, reg), bindings_arr.data, bindings_arr.len);
            ARR(Param) new_params = {0};

            for (size_t p = 0; p < entry->data.function.params_count; p++) {
                Param param = entry->data.function.params[p];
                Type pt = apply_bindings(
                    range_to_type(param.c_type, reg),
                    bindings_arr.data, bindings_arr.len
                );
                param.c_type = type_to_source_range(pt);
                ARR_PUSH(new_params, param);
            }
            register_insert(reg, mangled_sv, (RegisterEntry){
                .tag  = Reg_Function,
                .name = NULL,
                .type = apply_bindings(entry->type, bindings_arr.data, bindings_arr.len),
                .data.function = {
                    .return_type  = ret_bound,
                    .params = new_params.data,
                    .params_count = new_params.len,
                    .is_pub = entry->data.function.is_pub,
                }
            });
            break;
        }

        default: break;
    }
}

void maybe_resolve_generic(SourceRange name, SourceRange* generic_args, size_t generic_count, Register* reg, CheckerErrList* errors) {
    if (generic_count == 0 || !reg->mono) return;
    StringView sv = string_range(name);

    resolve_generic_call(sv, generic_args, generic_count, reg, reg->mono, errors, name);
}
