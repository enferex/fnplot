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

#ifndef _CS_HH
#define _CS_HH
#include <string>
#include <vector>
#include <unordered_map>

using std::string;

// Forwards
struct CSSym;
struct CSFile;

// Hash of symbols
typedef std::unordered_map<string, const CSSym *> CSSymHash;
typedef std::unordered_map<string, std::vector<const CSSym *>> CSDB;

// Symbol: could be a function definition or function call
class CSSym
{
public:
    CSSym(const char *name, char mark, size_t line, const CSFile *file) :
        _name(name), _mark(mark), _line(line), _file(file) {}

    char getMark() const { return _mark; }
    string getName() const { return _name; }

private:
    string        _name;
    char          _mark;
    size_t        _line;
    const CSFile *_file;
    CSSymHash     _fn_defs; // Function definitions
};

// Symbol: could be a function definition or function call
class CSFuncCall : public CSSym
{
public:
    CSFuncCall(const char *name, char mark, size_t line, const CSFile *file) :
        CSSym(name, mark, line, file) {}
};

// Symbol: could be a function definition or function call
class CSFuncDef : public CSSym
{
public:
    CSFuncDef(const char *name, char mark, size_t line, const CSFile *file) :
        CSSym(name, mark, line, file) {}

    void getCallees(std::vector<const CSFuncCall *> &addem) {
        for (auto callee: _callees)
          addem.push_back(static_cast<const CSFuncCall *>(callee.second));
    }

    // Unique add
    void addCallee(const CSFuncCall *fncall) {
        auto pr = std::make_pair<string, const CSSym *>(fncall->getName(), fncall);
        _callees.insert(pr);
    }

private:
        CSSymHash _callees; // Function calls
};

// A file entry contains a list of symbols, we only collect function calls here.
class CSFile
{
public:
    CSFile(const char *name, char mark):
        _name(name), _mark(mark) {}

    CSFuncDef *getCurrentFunction() const { return _current_fndef; }
    string getName() const { return _name; }
    void applyToFunctions(void (*fn)(const CSFuncDef *fndef)) {
        for (auto fndef: _functions) {
            fn(static_cast<const CSFuncDef *>(fndef.second));
        }
    }
    const CSSymHash &getFunctions() { return _functions; }

    void addFunctionDef(CSFuncDef *fndef) {
        auto pr = std::make_pair<string, const CSSym *>(fndef->getName(), fndef);
        _functions.insert(pr);
        _current_fndef = fndef;
    }

private:
    string    _name;
    char      _mark;
    CSSymHash _functions;

    // The current function being added to (callees being added).
    CSFuncDef *_current_fndef;
};

// cscope database (cscope.out) header
struct CSHeader
{
    int         version;
    bool        compression;    /* -c */
    bool        inverted_index; /* -q */
    bool        prefix_match;   /* -T */
    size_t      syms_start;
    size_t      trailer;
    const char *dir;
};

//cscope database (cscope.out) trailer
struct CSTrailer
{
    int    n_viewpaths;
    char **viewpath_dirs;
    int    n_srcs;
    char **srcs;
    int    n_incs;
    char **incs;
};

// cscope database, contains a list of file entries
struct CS
{
public:
    CS(const char *fname);
    void addFile(CSFile *f) { _files.push_back(f); }
    CSDB *buildDatabase();

private:
    CSHeader               _hdr;
    CSTrailer              _trailer;
    const char            *_name;
    int                    _n_functions;
    std::vector<CSFile *>  _files;

    void initHeader(const uint8_t *data, size_t data_size);
    void initTrailer(const uint8_t *data, size_t data_size);
    void initSymbols(const uint8_t *data, size_t data_size);
    void loadCScope();
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


// Public routines
extern void csPrintCallers(FILE *out, CSDB *db, const char *fn_name, int depth);
extern void csPrintCallees(FILE *out, CSDB *db, const char *fn_name, int depth);


#endif // _CS_HH
