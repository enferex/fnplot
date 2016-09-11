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
    auto db = new(db_t);

    for (const cs_file_t *f=cs->files; f; f=f->next) {
        for (const cs_sym_t *fndef=f->functions; fndef; fndef=fndef->next) {
            std::vector<const cs_sym_t *> callees;

            // Collect all calls this function (fndef) makes
            for (const cs_sym_t *call=fndef->u.callees; call; call=call->next)
              callees.push_back(call);

            // Add the funtion_def : calleess map entry
            auto pr = std::make_pair(fndef->name, callees);
            db->hash.insert(pr);
        }
    }

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
static void print_callers_rec(db_t *db, const char *fn_name, int depth)
{
    if (depth <= 0)
        return;

    for (auto pr: db->hash) {
        const char *item = pr.first.c_str();
        // Does 'item' call 'fn_name' ?
        if (is_caller_of(db, item, fn_name)) {
            cout << item << " -> " << fn_name << endl;
            print_callers_rec(db, item, depth - 1);
        }
    }
}

#ifdef __cplusplus
extern "C"
#endif
void print_callers(cs_db_t *db_ptr, const char *fn_name, int depth)
{
    auto db = reinterpret_cast<db_t *>(db_ptr);
    cout << "digraph {" << endl;
    print_callers_rec(db, fn_name, depth);
    cout << "}";
}
