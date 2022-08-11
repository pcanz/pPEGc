#include "../pPEG.c"

int main(void) {
    printf("Hello pPEG in C, URI example...\n");

    // Equivalent to the regular expression for well-formed URI's in RFC 3986.

    char* uri_grammar =
    "    URI     = (scheme ':')? ('//' auth)? path ('?' query)? ('#' frag)?  \n"
    "    scheme  = ~[:/?#]+     \n"
    "    auth    = ~[/?#]*      \n"
    "    path    = ~[?#]*       \n"
    "    query   = ~'#'*        \n"
    "    frag    = ~[ \t\n\r]*  \n";

    Peg uri_peg = peg_compile(uri_grammar);

    Peg uri_tree = peg_parse(&uri_peg, 
        "http://www.ics.uci.edu/pub/ietf/uri/#Related");

    peg_print(&uri_tree); // parse tree example
}
/*
    Hello pPEG in C, URI example...
    URI
    ├─scheme "http"
    ├─auth "www.ics.uci.edu"
    ├─path "/pub/ietf/uri/"
    └─frag "Related"
*/
    