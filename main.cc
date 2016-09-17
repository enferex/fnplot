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

#define __USE_POSIX
#define _POSIX_C_SOURCE 200809L
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include "cs.hh"

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

int main(int argc, char **argv)
{
    int opt;
    FILE *out;
    bool do_callees, do_callers;
    const char *fname, *fn_name, *out_fname;
    CS *cs;
    CSDB *db;
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

    // If the user did not specify callers or callees, do so for them!
    if (!do_callers && !do_callees)
      do_callees = true;

    if (out_fname && !(out = fopen(out_fname, "w"))) {
        fprintf(stderr, "Error opening output file %s: %s\n",
                out_fname, strerror(errno));
        exit(errno);
    }
    else if (!out_fname)
      out = stdout;

    // Load
    try {
        cs = new CS(fname);
    } 
    catch (const char *err) {
        fprintf(stderr, "Error loading cscope database: %s\n", err);
        exit(EXIT_FAILURE);
    }
    db = cs->buildDatabase();

    // Go!
    if (do_callers)
      csPrintCallers(out, db, fn_name, depth);
    if (do_callees)
      csPrintCallees(out, db, fn_name, depth);

    fclose(out);
    return 0;
}
