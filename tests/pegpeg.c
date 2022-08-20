#include "test-kit.c"

int main(void) {
    printf("Test pPEG grammar ...\n");

    char* peg_grammar = 
    "    Peg   = _ rule+ _                            \n"
    "    rule  = id _ '=' _ alt                       \n"
    "                                                 \n"
    "    alt   = seq ('/' _ seq)*                     \n"
    "    seq   = rep*                                 \n"
    "    rep   = pre sfx? _                            \n"
    "    pre   = pfx? term                            \n"
    "    term  = call / quote / chars / group / extn  \n"
    "    group = '(' _ alt ')'                        \n"
    "                                                 \n"
    "    call  = at? id multi? _ !'='                 \n"
    "    at    = '@'                                  \n"
    "    multi = ':' id                               \n"
    "    id    = [a-zA-Z_] [a-zA-Z0-9_]*              \n"
    "                                                 \n"
    "    pfx   = [~!&]                                \n"
    "    sfx   = [+?] / '*' range?                    \n"
    "    range = num ('..' num)?                      \n"
    "    num   = [0-9]+                               \n"
    "                                                 \n"
    "    quote = ['] sq ['] 'i'?                      \n"
    "    sq    = ~[']*                                \n"
    "    chars = '[' chs ']'                          \n"
    "    chs   =  ~']'*                               \n"
    "    extn  = '<' ~'>'* '>'                        \n"
    "    _     = ([ \n\r\t]+ / '#' ~[\n\r]*)*         \n";

    test_show(peg_grammar, peg_grammar);


    printf("OK, peg-peg grammar tests done...\n");
}
