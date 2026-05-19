#ifndef DIFF_H
#define DIFF_H

#include "import.h"
#include "register.h"
#include "file_manager.h"
#include "error.h"      // for CheckerErrList
#include "register.h"   // for Register, FuncBodyList etc
#include "parser.h"     // for parser_set_error_list, LineStarts

/* ── Change kinds ──────────────────────────────────────────────────────── */

typedef enum {
    DC_Added,        /* symbol exists in new but not old               */
    DC_Removed,      /* symbol exists in old but not new               */
    DC_TypeChanged,  /* symbol exists in both but type differs         */
    DC_Affected,     /* expression whose operand type changed          */
} DiffChangeKind;

/* ── Per-change record ─────────────────────────────────────────────────── */

typedef struct {
    DiffChangeKind  kind;

    const char*     sym_name;   /* null-terminated, points into register  */
    const char*     old_type;   /* type_tag_name() string, may be NULL    */
    const char*     new_type;

    /* source location of the declaration (from decl_name_range) */
    const char*     file;       /* source file path                       */
    uint32_t        line;       /* 1-based                                */
    uint16_t        col;        /* 1-based                                */
    uint32_t        line_end;
    uint16_t        col_end;

    /* for DC_Affected: the expression text */
    const char*     expr_start;
    size_t          expr_len;
} DiffRecord;

typedef ARR(DiffRecord) DiffRecordArr;

/* ── Parsed-file bundle (used by both diff.c and check.c) ──────────────── */

typedef struct {
    char*           source;
    FileManager     fm;
    FileId          fid;
    StmtsArr        program;
    LineStarts      ls;
    IDCounter       counter;
    Register        reg;
    FuncBodyList    bodies;
    CheckerErrList  errors;
} VixParsedFile;

/* ── Public API ────────────────────────────────────────────────────────── */

/*
 * Compare two registers built from the same source file.
 * Appends DiffRecord entries to *out.
 * new_fm / new_fid are used to resolve line/col for new-side symbols.
 */
void vix_diff_registers(
    Register*           old_reg,
    Register*           new_reg,
    Stmts*              new_prog,
    size_t              new_prog_len,
    const char*         new_src,
    const FileManager*  new_fm,
    FileId              new_fid,
    DiffRecordArr*      out
);

/* Pretty-print the diff records to stdout */
void vix_diff_print(
    const DiffRecordArr* records,
    const char*          old_path,
    const char*          new_path
);

/* Free heap memory owned by the records array */
void vix_diff_free(DiffRecordArr* records);

/*
 * High-level helper: parse both files, diff, print, free.
 * Returns number of changes, or -1 on error.
 */
int vix_diff_files(const char* old_path, const char* new_path);

/*
 * Parse a .vix source file into a VixParsedFile.
 * Returns true on success.  Caller must call vix_parsed_file_free().
 */
bool vix_parse_source(const char* path, const char* source, VixParsedFile* pf);

/* Release all memory owned by *pf */
void vix_parsed_file_free(VixParsedFile* pf);

#endif /* DIFF_H */