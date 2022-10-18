# pPEGc

This is an implementation of [pPEG] in C.

A single file `pPEG.c`, with no dependencies other than std lib.

##  Example

``` c
    #include <stdio.h>
    #include "../pPEG.h"

    int main(void) {
        printf("Hello pPEG in C, URI example...\n");

        // Equivalent to the regular expression for
        // well-formed URI's in RFC 3986.

        char* uri_grammar =
        "    URI     = (scheme ':')? ('//' auth)?     \n" 
        "              path ('?' query)? ('#' frag)?  \n"
        "    scheme  = ~[:/?#]+                       \n"
        "    auth    = ~[/?#]*                        \n"
        "    path    = ~[?#]*                         \n"
        "    query   = ~'#'*                          \n"
        "    frag    = ~[ \t\n\r]*                    \n";

        Peg* uri_peg = peg_compile(uri_grammar);

        Peg* uri_tree = peg_parse(uri_peg, 
            "http://www.ics.uci.edu/pub/ietf/uri/#Related");

        peg_print(uri_tree); // parse tree example
    }
    /*
        Hello pPEG in C, URI example...
        URI
        ├─scheme "http"
        ├─auth "www.ics.uci.edu"
        ├─path "/pub/ietf/uri/"
        └─frag "Related"
    */
```

##  API

Use: `#include "../pPEG.h"` to obtain API:

    typedef struct Peg Peg;   // opaque, contains ptree and any error info
    typedef struct Node Node; // opaque, ptree node

    // returns a ptr to a parser for the grammar
    Peg* peg_compile(char* grammar);

    Peg* peg_compile_text(char* grammar, int start, int end);

    // parse input string using peg parser, return peg ptree..
    Peg* peg_parse(Peg* peg, char* input);

    Peg* peg_parse_text(Peg* peg, char* input, int start, int end);

    // display the parse tree or error report
    void peg_print(Peg* peg);

    bool peg_err(Peg* peg); // if error ...

    // print out a trace of the parse rule matches...
    Peg* peg_trace(Peg* peg, char* input);

    // print out a low level trace of the parser instructions..
    Peg* peg_debug(Peg* peg, char* input);

    // Peg* access ..........

    Node* peg_tree(Peg*); // ptree root Node

    // copies node rule name into `name` string arg, with a 0 end,
    // limit of max length, truncated if necessary.
    void peg_name(Peg*, Node*, char* name, int max);

    // copies node text span into `text` string arg, with a 0 end,
    // limit of max length, truncated if necessary.
    void peg_text(Peg*, Node*, char* text, int max);

    // Node* access ........

    // returns index of rule name
    int peg_tag(Node*);

    // returns count of children nodes
    int peg_count(Node*);

    // returns ptr to the ith child node
    Node* peg_nodes(Node*, int);

Compile and use:

    > cc pPeg.c -o pPEG.o

    > cc file.c -o file.o ../pPEG.o

    > ./file.o


[pPEG]: https://github.com/pcanz/pPEG
