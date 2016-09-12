#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cs.h"

using std::cout;
using std::endl;

// A database is a collection of function definitions mapping to a vector
// of the calls that the function defintion makes.
typedef std::unordered_map<std::string, std::vector<const cs_sym_t *>> htype;
struct db_t
{
    htype hash;
};

// Create a database 
#ifdef __cplusplus
extern "C"
#endif
cs_db_t build_database(const cs_t *cs)
{
    int i = 0, spidx = 0;
    const char spin[] = "-\\|/";
    auto db = new(db_t);

    cout << "Building internal database  ";

    for (const cs_file_t *f=cs->files; f; f=f->next) {
        for (const cs_sym_t *fndef=f->functions; fndef; fndef=fndef->next) {
            std::vector<const cs_sym_t *> callees;

            // Collect all calls this function (fndef) makes
            for (const cs_sym_t *call=fndef->u.callees; call; call=call->next)
              callees.push_back(call);

            // Add the funtion_def : calleess map entry
            auto pr = std::make_pair(fndef->name, callees);
            db->hash.insert(pr);
            if (++i % 10000 == 0)
              cout << '\b' << spin[spidx++ % 4] << std::flush;
        }
    }

    cout << endl;
    return static_cast<cs_db_t>(db);
}

// Does a call b?
static bool is_caller_of(db_t *db, const char *a, const char *b)
{
    // All the functions 'a' calls
    for (auto callee: db->hash[a]) {
        if (strcmp(callee->name, b) == 0)
            return true;
    }

    return false;
}

// Collect all of the callers to 'fn_name'
static void print_callers_rec(
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
        if (is_caller_of(db, item, fn_name)) {
            fprintf(out, "    %s -> %s\n", item, fn_name);
            print_callers_rec(out, db, item, depth - 1);
        }
    }
}

#ifdef __cplusplus
extern "C"
#endif
void print_callers(FILE *out, cs_db_t *db_ptr, const char *fn_name, int depth)
{
    auto db = reinterpret_cast<db_t *>(db_ptr);
    fprintf(out, "digraph \"Callers to %s\" {\n", fn_name);
    print_callers_rec(out, db, fn_name, depth);
    fprintf(out, "}\n");
}

// Collect all of the callees to 'fn_name'
static void print_callees_rec(
    FILE       *out,
    db_t       *db,
    const char *fn_name,
    int         depth)
{
    if (depth <= 0)
        return;

    for (auto callee: db->hash[fn_name]) {
        fprintf(out, "    %s -> %s\n", fn_name, callee->name);
        print_callees_rec(out, db, callee->name, depth - 1);
    }
}

#ifdef __cplusplus
extern "C"
#endif
void print_callees(FILE *out, cs_db_t *db_ptr, const char *fn_name, int depth)
{
    auto db = reinterpret_cast<db_t *>(db_ptr);
    fprintf(out, "digraph \"Callees of %s\" {\n", fn_name);
    print_callees_rec(out, db, fn_name, depth);
    fprintf(out, "}\n");
}
