#include <stdbool.h>

typedef struct Peg Peg;
typedef struct Node Node;

// returns a ptr to a parser for the grammar
Peg* peg_compile(char* grammar);

Peg* peg_compile_text(char* grammar, int start, int end);

// parse input string using peg parser, return peg ptree..
Peg* peg_parse(Peg* peg, char* input);

Peg* peg_parse_text(Peg* peg, char* input, int start, int end);

// display the parse tree or error report
void peg_print(Peg* peg);

bool peg_err(Peg* peg); // if error ...

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


// print out a trace of the parse rule matches...
Peg* peg_trace(Peg* peg, char* input);
Peg* peg_trace_text(Peg* peg, char* input, int start, int end);

// print out a low level trace of the parser instructions..
Peg* peg_debug(Peg* peg, char* input);
Peg* peg_debug_text(Peg* peg, char* input, int start, int end);
