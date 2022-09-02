#include <stdio.h>

#include "../pPEG.h"

int main(void) {
    printf("Hello pPEG in C, URI example...\n");

    // Equivalent to the regular expression for well-formed URI's in RFC 3986.

    char* uri_grammar =
    "    URI     = (scheme ':')? ('//' auth)? path  \n"
    "              ('?' query)? ('#' frag)?         \n"
    "    scheme  = ~[:/?#]+                         \n"
    "    auth    = ~[/?#]*                          \n"
    "    path    = ~[?#]*                           \n"
    "    query   = ~'#'*                            \n"
    "    frag    = ~_WS*                            \n";

    Peg* peg_uri = peg_compile(uri_grammar);

    Peg* uri_peg = peg_parse(peg_uri, 
        "http://www.ics.uci.edu/pub/ietf/uri/#Related");

    peg_print(uri_peg); // parse tree example

}
/*
    Hello pPEG in C, URI example...
    URI
    ├─scheme "http"
    ├─auth "www.ics.uci.edu"
    ├─path "/pub/ietf/uri/"
    └─frag "Related"
*/
    