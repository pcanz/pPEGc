#include "../pPEG.c"

// cc -c -o test-kit test-kit.c

void test_ok(char* grammar, char* input) {
    Peg peg = peg_compile(grammar);
    if (peg_err(&peg)) {
        printf("grammar error....\n");
        peg_print(&peg);
        abort();
    }
    Peg p = peg_parse(&peg, input);
    if (peg_err(&p)) {
        printf("parse error....\n");
        peg_print(&p);
        abort();
    }
}

void test_show(char* grammar, char* input) {
    Peg peg = peg_compile(grammar);
    if (peg_err(&peg)) {
        printf("grammar error....\n");
        peg_print(&peg);
        abort();
    }
    Peg p = peg_parse(&peg, input);
    peg_print(&p); // AST input result
    printf("----\n");
}

void test_verbose(char* grammar, char* input) {
    Peg peg = peg_compile(grammar);
    peg_print(&peg); // AST for grammar
    printf("----\n");
    Peg p = peg_parse(&peg, input);
    peg_print(&p); // AST for an input
    printf("----\n");
}

void test_trace(char* grammar, char* input) {
    Peg peg = peg_compile(grammar);
    if (peg_err(&peg)) {
        printf("grammar error....\n");
        peg_print(&peg);
        abort();
    }
    Peg p = peg_trace(&peg, input);
    if (peg_err(&p)) {
        printf("parse error....\n");
        peg_print(&p);
        abort();
    }
}

// int main(void) {
//     printf("test-kit....\n");
// }
