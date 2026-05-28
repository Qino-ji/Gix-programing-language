#include "footprint.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cg_dump(CG *cg) { LLVMDumpModule(cg->mod); }

int cg_verify(CG *cg) {
    char *err = nullptr;
    int   bad = LLVMVerifyModule(cg->mod, LLVMPrintMessageAction, &err);
    if (err) LLVMDisposeMessage(err);
    return !bad;
}

static void cg_emit_ll(CG *cg, const char *project_name, const char *source_file) {
    char *path = fp_obj_path(project_name, source_file);

    size_t len = strlen(path);
    if (len >= 2 && path[len-2] == '.' && path[len-1] == 'o') {
        path[len-1] = 'l';
        char *ll_path = (char*)malloc(len + 2);

        memcpy(ll_path, path, len);
        ll_path[len-1] = 'l';
        ll_path[len]   = 'l';
        ll_path[len+1] = '\0';
        free(path);
        path = ll_path;
    }

    char *err = NULL;
    if (LLVMPrintModuleToFile(cg->mod, path, &err)) {
        fprintf(stderr, "cg: failed to write IR to %s: %s\n", path, err);
        LLVMDisposeMessage(err);
    } else {
        fprintf(stderr, "[debug] IR written to %s\n", path);
    }

    free(path);
}

int cg_emit_obj(CG *cg, const char *project_name, const char *source_file) {
    fp_ensure_tree(project_name);

    cg_emit_ll(cg, project_name, source_file);

    char *path = fp_obj_path(project_name, source_file);


    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    char *err = nullptr;

    if (LLVMGetTargetFromTriple(triple, &target, &err)) {
        fprintf(stderr, "LLVM target error: %s\n", err);
        LLVMDisposeMessage(err);
        LLVMDisposeMessage(triple);
        free(path);
        return 0;
    }

    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    LLVMDisposeMessage(triple);
    LLVMSetModuleDataLayout(cg->mod, LLVMCreateTargetDataLayout(tm));

    if (LLVMTargetMachineEmitToFile(tm, cg->mod, path, LLVMObjectFile, &err)) {
        fprintf(stderr, "Emit error: %s\n", err);
        LLVMDisposeMessage(err);
        LLVMDisposeTargetMachine(tm);
        free(path);
        return 0;
    }

    LLVMDisposeTargetMachine(tm);
    free(path);
    return 1;
}

int cg_link_exe(const char *project_name, const char *source_file) {
    char *obj  = fp_obj_path(project_name, source_file);
    char *exe  = fp_exe_path(project_name, source_file);
    char cmd[1024];

    snprintf(cmd, sizeof(cmd), "cc %s -o %s", obj, exe);
    int ret = system(cmd);

    free(obj);
    free(exe);
    return ret == 0;
}