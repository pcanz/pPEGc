#include <stdio.h>
#include "../pPEG.h"

// Note: no way to put a 0 into a C string, used \1 instead...

int main(void) {
    printf("Hello pPEG JSON grammar...\n");

    // char* json_grammar = 
    // "    json   = _ value _                                  \n"
    // "    value  =  Str / Arr / Obj / num / lit               \n"  
    // "    Obj    = '{'_ (memb (_','_ memb)*)? _'}'            \n"  
    // "    memb   = Str _':'_ value                            \n"
    // "    Arr    = '['_ (value (_','_ value)*)? _']'          \n"
    // "    Str    = '\"' chars* '\"'                           \n"
    // "    chars  = ~([\\u0000-\\u001F]/'\\'/'\"')+ / '\\' esc \n"
    // "    esc    = [\"\\/bfnrt] / 'u' [0-9a-fA-F]*4           \n"
    // "    num    = _int _frac? _exp?                          \n"
    // "    _int   = '-'? ([1-9] [0-9]* / '0')                  \n"
    // "    _frac  = '.' [0-9]+                                 \n"
    // "    _exp   = [eE] [+-]? [0-9]+                          \n"
    // "    lit    = 'true' / 'false' / 'null'                  \n";
 
   char* json_grammar = 
    "    json   = _ value _                                  \n"
    "    value  =  Str / Arr / Obj / num / lit               \n"  
    "    Obj    = '{'_ (memb (_','_ memb)*)? _'}'            \n"  
    "    memb   = Str _':'_ value                            \n"
    "    Arr    = '['_ (value (_','_ value)*)? _']'          \n"
    "    Str    = _DQ chars* _DQ                             \n"
    "    chars  = ~(_0-1F/_BS/_DQ)+ / _BS esc                \n"
    "    esc    = [/bfnrt] / _DQ / _BS / 'u' [0-9a-fA-F]*4   \n"
    "    num    = _int _frac? _exp?                          \n"
    "    _int   = '-'? ([1-9] [0-9]* / '0')                  \n"
    "    _frac  = '.' [0-9]+                                 \n"
    "    _exp   = [eE] [+-]? [0-9]+                          \n"
    "    lit    = 'true' / 'false' / 'null'                  \n";
    //   _DQ    = '"'
    //   _BS    = '\'
    //   _0-1F  = '\u0000-001F'

    Peg* json_peg = peg_compile(json_grammar);

    peg_print(json_peg); // ptree AST for the json grammar

    Peg* js1 = peg_parse(json_peg,
    "{ \"answer\": 42,                                     \n"
    "  \"mixed\": [1, 2.3, \"a\\tstring\", true, [4, 5]],  \n"
    "  \"empty\": {}                                       \n"
    "}                                                     \n");

    peg_print(js1); // ptree for json

}

/*
> cc -o json.o json.c ../pPEG.c
> ./json.o

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
