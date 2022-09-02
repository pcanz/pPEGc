#include "../pPEG.c"

int evaluate(Peg*, Node*);

int main(void) {
    printf("Hello pPEG C, tiny arith example...\n");

    // only two operators, no vars, no parens, no white-space, ...

    char* arith_grammar = 
    "add   = mult ('+' mult)*   \n"
    "mult  = int ('*' int)*     \n"
    "int   = [0-9]+             \n";

    Peg* arith_peg = peg_compile(arith_grammar);

    char test[] = "4+5*6+8";

    printf("expr = %s \n", test);

    Peg* arith = peg_parse(arith_peg, test);

    peg_print(arith); // parse tree

    int result = evaluate(arith, arith->tree);

    printf("answer = %d \n", result);
}

// evaluate parse tree.................................................

enum ARITH {ADD, MULT, INT}; // Must match peg_arith grammar rules

// This is a problem because it must be kept in sync with any changes
// to the grammar.  But it does keep everything nice and simple. 

int evaluate(Peg* ast, Node* node) {
    switch (node->tag) {
        case ADD: {
            int sum = 0;
            for (int i=0; i<node->count; i++) {
                sum += evaluate(ast, node->nodes[i]);
            }
            return sum;
        }
        case MULT: {
            int product = 1;
            for (int i=0; i<node->count; i++) {
                product *= evaluate(ast, node->nodes[i]);
            }
            return product;
        }
        case INT: {
            int val = 0;
            for (int i=node->start; i<node->end; i++) {
                char c = ast->src[i];
                val = val*10 + c-'0';
            }
            return val;
        }
        default: {
            printf("Woops, undefined AST tag %d\n", node->tag);
            return 0;
        }
    }
}

/*
    Hello pPEG C, tiny arith example...
    add
    ├─int "4"
    ├─mult
    │ ├─int "5"
    │ └─int "6"
    └─int "8"
    answer = 42 
*/

