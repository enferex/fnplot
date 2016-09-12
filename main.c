#define __USE_POSIX
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cs.h"
#include "build.hh"


static void usage(const char *execname)
{
    printf("Usage: %s -c cscope.out -f fn_name "
           "[-o outputfile] [-d depth] <-x | -y>\n"
           "  -c cscope.out: cscope.out database file\n"
           "  -f fn_name:    Function name to plot callers of\n"
           "  -d depth:      Depth of traversal.\n"
           "  -o outputfile: Write results to outputfile.\n"
           "  -x:            Print callers of fn_name.\n"
           "  -y:            Print calless of fn_name.\n"
           "  -h:            This help message.\n",
           execname);
    exit(EXIT_SUCCESS);
}


typedef struct { size_t off; size_t data_len; const char *data; } pos_t;


#define VALID(_p)     ((_p)->off <= (_p)->data_len)
#define NXT_VALID(_p) ((_p)->off+1 <= (_p)->data_len)
#define END(_p)       ((_p)->off >= (_p)->data_len)
#define CH(_p)        (VALID(_p) ? (_p)->data[(_p)->off] : EOF)
#define ERR(...) do {fprintf(stderr,__VA_ARGS__);fputc('\n', stderr);} while(0)

#ifdef DEBUG
#define DBG(...) do {fprintf(stdout,__VA_ARGS__);fputc('\n', stdout);} while(0)
#else
#define DBG(...) 
#endif

static void get_line(pos_t *pos, char *buf, size_t buf_len)
{
    size_t len, st = pos->off;

    /* Get initial line (header line) */
    while (CH(pos) != EOF && CH(pos) != '\n')
      ++pos->off;

    len = pos->off - st; /* Don't return the '\n'     */
    ++pos->off;          /* +1 to advance to the '\n' */
    if (END(pos) || len > buf_len)
      return;

    memcpy(buf, pos->data + st, len);
    buf[len] = '\0';
}


/* Header looks like: 
 *     <cscope> <dir> <version> [-c] [-q <symbols>] [-T] <trailer>
 */
static void init_header(cs_t *cs, const char *data, size_t data_len)
{
    pos_t pos = {0};
    char buf[1024], *tok;
    
    pos.data = data;
    pos.data_len = data_len;
    get_line(&pos, buf, sizeof(buf));

    /* After the header are the symbols */
    cs->hdr.syms_start = pos.off;

    /* Load in the header: <cscope> */
    tok = strtok(buf, " ");
    if (strncmp(tok, "cscope", strlen("cscope")))
    {
        ERR("This does not appear to be a cscope database");
        return;
    }

    /* Version */
    cs->hdr.version = atoi(strtok(NULL, " "));
    
    /* Directory */
    cs->hdr.dir = strndup(strtok(NULL, " "), 1024);

    /* Optionals: [-c] [-T] [-q <syms>] */
    while ((tok = strtok(NULL, " ")))
    {
        if (tok[0] == '-' && strlen(tok) == 2)
        {
            if (tok[1] == 'c')
              cs->hdr.compression = true;
            else if (tok[1] == 'T')
              cs->hdr.prefix_match = true;
            else if (tok[1] == 'q')
              cs->hdr.inverted_index = true; // TODO
            else {
                ERR("Unrecognized header option");
                return;
            }
        }
        else
        {
            cs->hdr.trailer = atol(tok);
            break;
        }
    }
}


static void init_trailer(cs_t *cs, const void *data, size_t data_len)
{
    int i;
    char line[1024] = {0};
    pos_t pos = {0};

    pos.data = data;
    pos.data_len = data_len;
    pos.off = cs->hdr.trailer;

    if (!VALID(&pos))
      return;

    /* Viewpaths */
    get_line(&pos, line, sizeof(line));
    cs->trailer.n_viewpaths = atoi(line);
    for (i=0; i<cs->trailer.n_viewpaths; ++i) {
        get_line(&pos, line, sizeof(line));
        DBG("[%d of %d] Viewpath: %s", i+1, cs->trailer.n_viewpaths, line);
    }
    
    /* Sources */
    get_line(&pos, line, sizeof(line));
    cs->trailer.n_srcs = atoi(line);
    for (i=0; i<cs->trailer.n_srcs; ++i) {
        get_line(&pos, line, sizeof(line));
        DBG("[%d of %d] Sources: %s", i+1, cs->trailer.n_srcs, line);
    }
        
    /* Includes */
    get_line(&pos, line, sizeof(line));
    cs->trailer.n_incs = atoi(line);
    get_line(&pos, line, sizeof(line));
    for (i=0; i<cs->trailer.n_incs; ++i) {
        get_line(&pos, line, sizeof(line));
        DBG("[%d of %d] Includes: %s", i+1, cs->trailer.n_incs, line);
    }
}

static cs_file_t *new_file(const char *str)
{
    cs_file_t *file;
    const char *c = str;
    
    /* Skip whitespace */
    while (isspace(*c))
      ++c;

    if (!(file = calloc(1, sizeof(cs_file_t))))
      ERR("Out of memory");
    file->mark = *c;
    file->name = strdup(c+1);
    return file;
}

static cs_sym_t *new_sym(void)
{
    cs_sym_t *sym;
    if (!(sym = calloc(1, sizeof(cs_sym_t))))
      ERR("Out of memory");
    return sym;
}

static _Bool is_mark(char c)
{
    int i;
    for (i=0; i<sizeof(cs_marks)/sizeof(cs_marks[0]); ++i)
      if (c == cs_marks[i])
        return true;
    return false;
}

/* Parse each line in the <file mark><file path>:
 * From docs:
 *
 * and for each source line containing a symbol
 *
 * <line number><blank><non-symbol text>
 * <optional mark><symbol>
 * <non-symbol text>
 * repeat above 2 lines as necessary
 * <empty line>
 *
 * Source:
 * ftp://ftp.eeng.dcu.ie/pub/ee454/cygwin/usr/share/doc/mlcscope-14.1.8/html/cscope.html
 *
 * Returns: function definition that was just added, or is being added to.
 */
static cs_sym_t *load_symbols_in_file(
    cs_file_t *file,
    pos_t     *pos,
    cs_sym_t  *fndef,
    long       lineno)
{
    char line[1024], *c, mark;
    cs_sym_t *sym;
    
    /* Suck in only function calls or definitions for this lineno */
    while (VALID(pos)) {
        /* Skip <non-symbol text> and obtain:
         *
         * <optional mark><symbol text> 
         * This will be <blank> if end of symbol data.
         */
        get_line(pos, line, sizeof(line));
        if (strlen(line) == 0)
          break;

        c = line;

        /* Skip spaces and not tabs */
        while (*c == ' ')
          ++c;

        /* <optional mark> */
        mark = 0;
        if (c[0] == '\t' && is_mark(c[1])) {
            mark = c[1];
            c += 2;
        }

        /* Only accept function definitions or function calls */
        if (!mark || (mark != CS_FN_DEF && mark != CS_FN_CALL))
          continue;

        /* Skip lines only containing mark characters */
        if (is_mark(c[0]) && strlen(c+1) == 0)
          continue;

        sym       = new_sym();
        sym->mark = mark;
        sym->line = lineno;
        sym->name = strdup(c);
        sym->file = file;

        /* If function call */
        if (sym->mark == CS_FN_CALL) {
            if (!fndef) {
                /* This is probably a macro */
                free(sym);
                continue;
            }

            /* We always allocate a new symbol
             * so the next pointer for a call will be used as a 
             * list of all calls that the function defintiion makes.
             */
             sym->next = fndef->u.callees;
             fndef->u.callees = sym;
        }
        else if (sym->mark == CS_FN_DEF) { /* If function def */
            /* Add fn definition to file */
            fndef = sym;
            fndef->next = file->functions;
            file->functions = fndef;
            ++file->n_functions;
        }

        if (strlen(c) == 0) {
            free(sym);
            if (mark == CS_FN_DEF)
              sym = NULL;
            continue;
        }

        /* <non-symbol text> */
        get_line(pos, line, sizeof(line));
    }

    return fndef;
}


/* Extract the symbols for file
 * This must start with the <mark><file> line.
 */
static void file_load_symbols(cs_file_t *file, pos_t *pos)
{
    long lineno;
    cs_sym_t *fn;
    char line[1024], *c;

    fn = NULL;
    DBG("Loading: %s", file->name);
   
    /* <empty line> */
    get_line(pos, line, sizeof(line));

    /* Now parse symbol information for eack line in 'file' */
    while (VALID(pos)) {
        /* Either this is a symbol, or a file.  If this is a file, then we are
         * done processing the current file.  So, in that case we break and
         * restore the position at the start of the next file's data:
         * <mark><file> line.
         *
         * So there are two cases here:
         * 1) New set of symbols: <lineno><blank><non-symbol text> 
         * 2) A new file: <mark><file>
         */
        get_line(pos, line, sizeof(line));
        c = line;
        while (isspace(*c))
          ++c;

        /* Case 2: New file */
        if (c[0] == '@') {
            pos->off -= strlen(line);
            return;
        }

        /* Case 1: Symbols at line!
         * <line number><blank>
         */
        lineno = atol(c);
        fn = load_symbols_in_file(file, pos, fn, lineno);
    }
}


static void init_symbols(cs_t *cs, const char *data, size_t data_len)
{
    pos_t pos = {0};
    char line[1024]; 
    cs_file_t *file;
   
    pos.off = cs->hdr.syms_start; 
    pos.data = data;
    pos.data_len = data_len;

    while (VALID(&pos) && pos.off <= cs->hdr.trailer) {
        /* Get file info */
        get_line(&pos, line, sizeof(line));
        file = new_file(line);
        file_load_symbols(file, &pos);

        /* No-name file */
        if (strlen(file->name) == 0) {
            free(file);
            continue;
        }

        /* Add the file to the list of files */
        file->next = cs->files;
        cs->files = file;

        ++cs->n_files;
        cs->n_functions += file->n_functions;
    }
}


/* Load a cscope database and return a pointer to the data */
static cs_t *load_cscope(const char *fname)
{
    FILE *fp;
    cs_t *cs;
    void *data;
    struct stat st;

    if (!(fp = fopen(fname, "r"))) {
        fprintf(stderr, "Could not open cscope database file: %s\n", fname);
        exit(EXIT_FAILURE);
    }

    /* mmap the input cscope database */
    fstat(fileno(fp), &st);
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
    if (!data) {
        fprintf(stderr, "Error maping cscope database\n");
        exit(EXIT_FAILURE);
    }

    if (!(cs = calloc(sizeof(cs_t), 1))) {
        ERR("Could not allocate enough room for cscope parent structure");
        return NULL;
    }

    cs->name = strdup(fname);
    init_header(cs, data, st.st_size);
    init_trailer(cs, data, st.st_size);
    init_symbols(cs, data, st.st_size);
    
    munmap(data, st.st_size);
    fclose(fp);
    return cs;
}


#if 0
static void cs_dump(FILE *fp, const cs_t *cs)
{
    int i;
    const cs_sym_t  *fndef, *fncall;
    const cs_file_t *f;

    for (f=cs->files; f; f=f->next) {
        printf("%s: %d function definitions\n", f->name, f->n_functions);
        i = 0;
        for (fndef=f->functions; fndef; fndef=fndef->next) {
            assert(fndef->mark == CS_FN_DEF);
            fprintf(fp, "%d) %s: %s := {", ++i, f->name, fndef->name);
            for (fncall=fndef->u.callees; fncall; fncall=fncall->next)
              printf("%s, ", fncall->name);
            if (fndef->u.callees)
              printf("\b\b}\n");
            else
              printf("}\n");
        }
    }
}
#endif

int main(int argc, char **argv)
{
    int opt;
    FILE *out;
    _Bool do_callees, do_callers;
    const char *fname, *fn_name, *out_fname;
    cs_t *cs;
    cs_db_t *db;
    int depth = 2;

    do_callers = do_callees = false;
    fname = out_fname = fn_name = NULL;

    while ((opt = getopt(argc, argv, "c:d:f:o:hxy")) != -1) {
        switch (opt) {
        case 'c': fname = optarg; break;
        case 'd': depth = atoi(optarg); break;
        case 'f': fn_name = optarg; break;
        case 'o': out_fname = optarg; break;
        case 'x': do_callers = true; break;
        case 'y': do_callees = true; break;
        case 'h': usage(argv[0]); break;
        default: return EXIT_FAILURE;
        }
    }

    if (!fname || !fn_name || depth < 0) {
        fprintf(stderr, "Invalid options, see '-h'\n");
        return EXIT_FAILURE;
    }

    /* If the user did not specify callers or callees, do so for them! */
    if (!do_callers && !do_callees)
      do_callees = true;

    if (out_fname && !(out = fopen(out_fname, "w"))) {
        fprintf(stderr, "Error opening output file %s: %s\n",
                out_fname, strerror(errno));
        exit(errno);
    }
    else if (!out_fname)
      out = stdout;

    /* Load */
    cs = load_cscope(fname);
    db = build_database(cs);

    /* Go! */
    if (do_callers)
      print_callers(out, db, fn_name, depth);
    if (do_callees)
      print_callees(out, db, fn_name, depth);

    fclose(out);
    return 0;
}
