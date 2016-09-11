#ifndef _BUILD_HH
#define _BUILD_HH
#include "cs.h"

extern cs_db_t *build_database(const cs_t *cs);
extern void print_callers(FILE *out, cs_db_t *db, const char *fn_name, int depth);
extern void print_callees(FILE *out, cs_db_t *db, const char *fn_name, int depth);

#endif // _BUILD_HH
