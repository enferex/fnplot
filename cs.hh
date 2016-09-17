#ifndef _CS_HH
#define _CS_HH
#include <string>
#include <vector>
#include <unordered_map>

using std::string;

// Database is exposed as an opaque pointer, onlu build.cc knows the magic
typedef void *cs_db_t;

// Forwards
struct cs_sym_t;
struct cs_file_t;

// Hash of symbols
typedef std::unordered_map<string, std::vector<const cs_sym_t *>> cs_sym_hash_t;

// Symbol
class cs_sym_t
{
public:
    cs_sym_t(const char *name, char mark, size_t line, const cs_file_t *file) :
        _name(name), _mark(mark), _line(line), _file(file) {}

    char getMark() { return _mark; }

private:
    string           _name;
    char             _mark;
    size_t           _line;
    const cs_file_t *_file;

    // Use 'mark' field to determine which variant to use
    union u {
        cs_sym_hash_t fn_defs; // Function definitions
        cs_sym_hash_t callees; // Function calls
    };
};


// A file entry contains a list of symbols
class cs_file_t
{
public:
    cs_file_t(const char *name, char mark):
        _name(name), _mark(mark) {}

    cs_sym_t *getCurrentFunction() { return _current_fn; }

private:
    string        _name;
    char          _mark;
    cs_sym_hash_t _functions;

    // The current function being added to (callees being added).
    cs_sym_t *_current_fn;
};


// cscope database (cscope.out) header
struct cs_hdr_t
{
    int         version;
    bool        compression;    /* -c */
    bool        inverted_index; /* -q */
    bool        prefix_match;   /* -T */
    size_t      syms_start;
    size_t      trailer;
    const char *dir;
};


// cscope database (cscope.out) trailer
struct cs_trl_t
{
    int    n_viewpaths;
    char **viewpath_dirs;
    int    n_srcs;
    char **srcs;
    int    n_incs;
    char **incs;
};


// cscope database, contains a list of file entries
struct cs_t
{
    cs_hdr_t    hdr;
    cs_trl_t    trailer;
    const char *name;
    int         n_files;
    int         n_functions;
    cs_file_t  *files;
};


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


#endif // _CS_HH
