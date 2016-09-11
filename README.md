## fnplot: Print call graph (dot) from a cscope file.

### What
fnplot generates a graphviz output file (.dot) format for a given function.
The output displays the callers or callees of the specified function.

### Depends
To create a cscope database you will need cscope, which can be obtained here:
http://cscope.sourceforge.net/

To generate an actual graph from the output of fnplot, you will need graphviz:
http://www.graphviz.org/

### Build
Run `make'

### Run
To build a cscope database run cscope with the '-b' option.  For example:
    cscope -b *.c

The latter command will search all .c files in your current working directory
and produce a cscope.out file.  That file, the cscope.out, is the cscope
database that fnplot takes as input.

### Note
The cscope parsing functionality originated from my other project:
https://github.com/enferex/coogle

### Contact
Matt Davis (enferex)
mattdavis9@gmail.com
