/*******************************************************************************
 * Copyright (c) 2016, enferex <mattdavis9@gmail.com>
 *
 * ISC License:
 * https://www.isc.org/downloads/software-support-policy/isc-license/
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#ifndef _BUILD_HH
#define _BUILD_HH
#include "cs.hh"

extern cs_db_t *build_database(const cs_t *cs);
extern void print_callers(FILE *out, cs_db_t *db, const char *fn_name, int depth);
extern void print_callees(FILE *out, cs_db_t *db, const char *fn_name, int depth);

#endif // _BUILD_HH
