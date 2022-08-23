#include "../pPEG.c"

// Note: no way to put a 0 into a C string, used \1 instead...

int main(void) {
    printf("Hello pPEG JSON grammar...\n");

    char* json_grammar = 
    "    json   = _ value _                         \n"
    "    value  =  Str / Arr / Obj / num / lit      \n"  
    "    Obj    = '{'_ (memb (_','_ memb)*)? _'}'   \n"  
    "    memb   = Str _':'_ value                   \n"
    "    Arr    = '['_ (value (_','_ value)*)? _']' \n"
    "    Str    = '\"' chars* '\"'                  \n"
    "    chars  = ~[\x01-\x1F\\\"]+ / '\\' esc      \n"
    "    esc    = [\"\\/bfnrt] / 'u' [0-9a-fA-F]*4  \n"
    "    num    = _int _frac? _exp?                 \n"
    "    _int   = '-'? ([1-9] [0-9]* / '0')         \n"
    "    _frac  = '.' [0-9]+                        \n"
    "    _exp   = [eE] [+-]? [0-9]+                 \n"
    "    lit    = 'true' / 'false' / 'null'         \n"
    "    _      = [ \t\n\r]*                        \n";

    char* json_grammar = 
    "    json   = _ value _                         \n"
    "    value  =  Str / Arr / Obj / num / lit      \n"  
    "    Obj    = '{'_ (memb (_','_ memb)*)? _'}'   \n"  
    "    memb   = Str _':'_ value                   \n"
    "    Arr    = '['_ (value (_','_ value)*)? _']' \n"
    "    Str    = '\"' chars* '\"'                  \n"
    "   # chars  = ~[\x01-\x1F\\\"]+ / '\\' esc      \n"
    "    chars  = ~(_0-1F/'\\'/'\"')+ / '\\' esc    \n"
    "    esc    = [\"\\/bfnrt] / 'u' [0-9a-fA-F]*4  \n"
    "    num    = _int _frac? _exp?                 \n"
    "    _int   = '-'? ([1-9] [0-9]* / '0')         \n"
    "    _frac  = '.' [0-9]+                        \n"
    "    _exp   = [eE] [+-]? [0-9]+                 \n"
    "    lit    = 'true' / 'false' / 'null'         \n";  // TODO _WS JSON?
    //"    _      = [ \t\n\r]*                        \n";

    Peg json_peg = peg_compile(json_grammar);

    peg_print(&json_peg); // AST for the json grammar

    Peg js1 = peg_parse(&json_peg,
    "{ \"answer\": 42,                                     \n"
    "  \"mixed\": [1, 2.3, \"a\\tstring\", true, [4, 5]],  \n"
    "  \"empty\": {}                                       \n"
    "}                                                     \n");

    peg_print(&js1); // AST for a json

}

/*
Obj
├─memb
│ ├─Str
│ │ └─chars "answer"
│ └─num "42"
├─memb
│ ├─Str
│ │ └─chars "mixed"
│ └─Arr
│   ├─num "1"
│   ├─num "2.3"
│   ├─Str
│   │ ├─chars "a"
│   │ ├─esc "t"
│   │ └─chars "string"
│   ├─lit "true"
│   └─Arr
│     ├─num "4"
│     └─num "5"
└─memb
  ├─Str
  │ └─chars "empty"
  └─Obj "{}"
*/
