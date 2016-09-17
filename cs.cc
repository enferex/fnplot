//******************************************************************************
// Copyright (c) 2016, enferex <mattdavis9@gmail.com>
//
// ISC License:
// https://www.isc.org/downloads/software-support-policy/isc-license/
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.
//******************************************************************************

#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include "cs.hh"

using std::cout;
using std::endl;

typedef struct { size_t off; size_t data_len; const uint8_t *data; } pos_t;

// Cscope data stream accessors
#define VALID(_p)     ((_p)->off <= (_p)->data_len)
#define NXT_VALID(_p) ((_p)->off+1 <= (_p)->data_len)
#define END(_p)       ((_p)->off >= (_p)->data_len)
#define CH(_p)        (VALID(_p) ? (_p)->data[(_p)->off] : EOF)

// Error handling and debugging
#define ERR(...) do {fprintf(stderr,__VA_ARGS__);fputc('\n', stderr);} while(0)
#ifdef DEBUG
#define DBG(...) do {fprintf(stdout,__VA_ARGS__);fputc('\n', stdout);} while(0)
#else
#define DBG(...)
#endif

static void getLine(pos_t *pos, char *buf, size_t buf_len)
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

static CSFile *newFile(const char *str)
{
    const char *c = str;

    // Skip whitespace
    while (isspace(*c))
      ++c;

    return new CSFile(c+1, *c);
}

static bool isMark(char c)
{
    unsigned i;
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
static void load_symbols_in_file(
    CSFile *file,
    pos_t  *pos,
    long    lineno)
{
    char line[1024], *c, mark;

    // Suck in only function calls or definitions for this lineno
    while (VALID(pos)) {
        // Skip <non-symbol text> and obtain:
        //
        // <optional mark><symbol text>
        // This will be <blank> if end of symbol data.
        getLine(pos, line, sizeof(line));
        if (strlen(line) == 0)
          break;

        c = line;

        // Skip spaces and not tabs
        while (*c == ' ')
          ++c;

        // <optional mark>
        mark = 0;
        if (c[0] == '\t' && isMark(c[1])) {
            mark = c[1];
            c += 2;
        }

        // Only accept function definitions or function calls
        if (!mark || (mark != CS_FN_DEF && mark != CS_FN_CALL))
          continue;

        // Skip lines only containing mark characters
        if (isMark(c[0]) && strlen(c+1) == 0)
          continue;

        if (mark == CS_FN_CALL) {
            CSFuncDef *fndef = file->getCurrentFunction();

            // This is probably a macro
            if (!fndef)
              continue;

            // We always allocate a new symbol
            // so the next pointer for a call will be used as a
            // list of all calls that the function defintiion makes.
            fndef->addCallee(new CSFuncCall(c, mark, lineno, file));
        }
        else if (mark == CS_FN_DEF) {
            // Add fn definition to file: Most recently defined is first
            file->addFunctionDef(new CSFuncDef(c, mark, lineno, file));
        }

        if (strlen(c) == 0)
          continue;

        // <non-symbol text>
        getLine(pos, line, sizeof(line));
    }
}

/* Extract the symbols for file
 * This must start with the <mark><file> line.
 */
static void fileLoadSymbols(CSFile *file, pos_t *pos)
{
    long lineno;
    char line[1024], *c;

    DBG("Loading: %s", file->name);

    /* <empty line> */
    getLine(pos, line, sizeof(line));

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
        getLine(pos, line, sizeof(line));
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
        load_symbols_in_file(file, pos, lineno);
    }
}

/* Load a cscope database and return a pointer to the data */
CS::CS(const char *fname)
{
    FILE *fp;
    uint8_t *data;
    struct stat st;

    if (!(fp = fopen(fname, "r")))
      throw("Could not open cscope database file");

    /* mmap the input cscope database */
    fstat(fileno(fp), &st);
    data = (uint8_t *)mmap(NULL, st.st_size,
                           PROT_READ, MAP_PRIVATE, fileno(fp), 0);
    if (!data)
      throw("Error memory maping cscope database");

    // Initialize the data
    initHeader(data, st.st_size);
    initTrailer(data, st.st_size);
    initSymbols(data, st.st_size);

    // Done loading data
    munmap(data, st.st_size);
    fclose(fp);
}

// Create a database
CSDB *CS::buildDatabase()
{
    int i = 0, spidx = 0;
    const char spin[] = "-\\|/";
    auto db = new CSDB;

    cout << "Building internal database: ";
    for (const CSFile *f=this->files; f; f=f->next) {
        for (const CSSym *fndef=f->functions; fndef; fndef=fndef->next) {
            std::vector<const CSSym *> callees;

            // Collect all calls this function (fndef) makes
            for (const CSSym *call=fndef->callees; call; call=call->next)
              callees.push_back(call);

            // Add the funtion_def : calleess map entry
            auto pr = std::make_pair(fndef->name, callees);
            db->hash.insert(pr);
            if ((++i % 1000) == 0)
              cout << '\b' << spin[spidx++ % 4] << std::flush;
        }
    }

    cout << endl;
    return db;
}

// Does a call b?
static bool isCallerOf(db_t *db, const char *a, const char *b)
{
    // All the functions 'a' calls
    for (auto callee: db->hash[a]) {
        if (strcmp(callee->name, b) == 0)
            return true;
    }

    return false;
}

// Collect all of the callers to 'fn_name'
static void printCallersRec(
    FILE       *out,
    db_t       *db,
    const char *fn_name,
    int         depth)
{
    if (depth <= 0)
        return;

    for (auto pr: db->hash) {
        const char *item = pr.first.c_str();
        // Does 'item' call 'fn_name' ?
        if (isCallerOf(db, item, fn_name)) {
            fprintf(out, "    %s -> %s\n", item, fn_name);
            printCallersRec(out, db, item, depth - 1);
        }
    }
}

void csPrintCallers(FILE *out, CSDB *db, const char *fn_name, int depth)
{
    fprintf(out, "digraph \"Callers to %s\" {\n", fn_name);
    printCallersRec(out, db, fn_name, depth);
    fprintf(out, "}\n");
}

// Collect all of the callees to 'fn_name'
static void printCalleesRec(
    FILE       *out,
    db_t       *db,
    const char *fn_name,
    int         depth)
{
    if (depth <= 0)
        return;

    for (auto callee: db->hash[fn_name]) {
        fprintf(out, "    %s -> %s\n", fn_name, callee->name);
        printCalleesRec(out, db, callee->name, depth - 1);
    }
}

void csPrintCallees(FILE *out, CSDB *db_ptr, const char *fn_name, int depth)
{
    auto db = reinterpret_cast<db_t *>(db_ptr);
    fprintf(out, "digraph \"Callees of %s\" {\n", fn_name);
    print_callees_rec(out, db, fn_name, depth);
    fprintf(out, "}\n");
}

/* Header looks like:
 *     <cscope> <dir> <version> [-c] [-q <symbols>] [-T] <trailer>
 */
static void CS::initHeader(const uint8_t *data, size_t data_len)
{
    pos_t pos = {0};
    char buf[1024], *tok;

    pos.data = data;
    pos.data_len = data_len;
    getLine(&pos, buf, sizeof(buf));

    /* After the header are the symbols */
    this->hdr.syms_start = pos.off;

    /* Load in the header: <cscope> */
    tok = strtok(buf, " ");
    if (strncmp(tok, "cscope", strlen("cscope")))
    {
        ERR("This does not appear to be a cscope database");
        return;
    }

    /* Version */
    this->hdr.version = atoi(strtok(NULL, " "));

    /* Directory */
    this->hdr.dir = strndup(strtok(NULL, " "), 1024);

    /* Optionals: [-c] [-T] [-q <syms>] */
    while ((tok = strtok(NULL, " "))) {
        if (tok[0] == '-' && strlen(tok) == 2) {
            if (tok[1] == 'c')
              this->hdr.compression = true;
            else if (tok[1] == 'T')
              this->hdr.prefix_match = true;
            else if (tok[1] == 'q')
              this->hdr.inverted_index = true; // TODO
            else {
                ERR("Unrecognized header option");
                return;
            }
        }
        else {
            this->hdr.trailer = atol(tok);
            break;
        }
    }
}

static void CS::initTrailer(const uint8_t *data, size_t data_len)
{
    int i;
    char line[1024] = {0};
    pos_t pos = {0};

    pos.data = data;
    pos.data_len = data_len;
    pos.off = this->hdr.trailer;

    if (!VALID(&pos))
      return;

    /* Viewpaths */
    getLine(&pos, line, sizeof(line));
    this->trailer.n_viewpaths = atoi(line);
    for (i=0; i<this->trailer.n_viewpaths; ++i) {
        getLine(&pos, line, sizeof(line));
        DBG("[%d of %d] Viewpath: %s", i+1, this->trailer.n_viewpaths, line);
    }

    /* Sources */
    getLine(&pos, line, sizeof(line));
    this->trailer.n_srcs = atoi(line);
    for (i=0; i<this->trailer.n_srcs; ++i) {
        getLine(&pos, line, sizeof(line));
        DBG("[%d of %d] Sources: %s", i+1, this->trailer.n_srcs, line);
    }

    /* Includes */
    getLine(&pos, line, sizeof(line));
    this->trailer.n_incs = atoi(line);
    getLine(&pos, line, sizeof(line));
    for (i=0; i<this->trailer.n_incs; ++i) {
        getLine(&pos, line, sizeof(line));
        DBG("[%d of %d] Includes: %s", i+1, this->trailer.n_incs, line);
    }
}

static void CS::initSymbols(const uint8_t *data, size_t data_len)
{
    pos_t pos = {0};
    char line[1024];
    CSFile *file;

    pos.off = cs->hdr.syms_start;
    pos.data = data;
    pos.data_len = data_len;

    while (VALID(&pos) && pos.off <= this->gchdr.trailer) {
        // Get file info
        getLine(&pos, line, sizeof(line));
        file = newFile(line);
        fileLoadSymbols(file, &pos);

        // No-name file
        if (file->getName().size() == 0) {
            delete file;
            continue;
        }

        /* Add the file to the list of files */
        this->gcaddFile(file);
        this->gcn_functions += file->n_functions;
    }
}
