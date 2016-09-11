#ifndef _CS_H
#define _CS_H
#include <stdbool.h>


/* Opaque handle */
typedef void *cs_db_t;


/* Symbol */
struct _cscope_file_t;
typedef struct _cscope_sym_t
{
    char                         mark;
    size_t                       line;
    const char                  *name;
    const struct _cscope_file_t *file;
    union {
        struct _cscope_sym_t    *fn_defs; /* Function definitions */
        struct _cscope_sym_t    *callees; /* Function calls       */
    } u; /* Use 'mark' field to determine which variant to use    */

    /* Used by cs_file_t to chain in a list of symbols (func def sybols) */
    struct _cscope_sym_t *next;
} cs_sym_t;


/* A file entry contains a list of symbols */
typedef struct _cscope_file_t
{
    char                   mark;
    const char            *name;
    int                    n_functions;
    cs_sym_t              *functions;
    struct _cscope_file_t *next;
} cs_file_t;


/* cscope database (cscope.out) header */
typedef struct _cscope_hdr_t
{
    int         version;
    _Bool       compression;    /* -c */
    _Bool       inverted_index; /* -q */
    _Bool       prefix_match;   /* -T */
    size_t      syms_start;
    size_t      trailer;
    const char *dir;
} cs_hdr_t;


/* cscope database (cscope.out) trailer */
typedef struct _cscope_trl_t
{
    int    n_viewpaths;
    char **viewpath_dirs;
    int    n_srcs;
    char **srcs;
    int    n_incs;
    char **incs;
} cs_trl_t;


/* cscope database, contains a list of file entries */
typedef struct _cscope_t
{
    cs_hdr_t    hdr;
    cs_trl_t    trailer;
    const char *name;
    int         n_files;
    int         n_functions;
    cs_file_t  *files;
} cs_t;


#define CS_FN_DEF  '$'
#define CS_FN_CALL '`'
static const char cs_marks[] = 
{
    '@', CS_FN_DEF, CS_FN_CALL,
    '}', '#', ')',
    '~', '=', ';', 
    'c', 'e', 'g', 
    'l', 'm', 'p', 
    's', 't', 'u'
};


#endif /* _CS_H */
