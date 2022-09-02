#include <stdlib.h>
#include <stdio.h>

#include "../pPEG.h"

// cc -c -o test-kit test-kit.c

void test_ok(char* grammar, char* input) {
    Peg* peg = peg_compile(grammar);
    if (peg_err(peg)) {
        printf("grammar error....\n");
        peg_print(peg);
        exit(1);
    }
    Peg* p = peg_parse(peg, input);
    if (peg_err(p)) {
        printf("parse error....\n");
        peg_print(p);
        exit(1);
    }
}

void test_show(char* grammar, char* input) {
    Peg* peg = peg_compile(grammar);
    if (peg_err(peg)) {
        printf("grammar error....\n");
        peg_print(peg);
        exit(1);
    }
    Peg* p = peg_parse(peg, input);
    peg_print(p); // AST input result
    printf("----\n");
}

void test_verbose(char* grammar, char* input) {
    Peg* peg = peg_compile(grammar);
    peg_print(peg); // AST for grammar
    printf("----\n");
    Peg* p = peg_parse(peg, input);
    peg_print(p); // AST for an input
    printf("----\n");
}

void test_trace(char* grammar, char* input) {
    Peg* peg = peg_compile(grammar);
    if (peg_err(peg)) {
        printf("grammar error....\n");
        peg_print(peg);
        exit(1);
    }
    Peg* p = peg_trace(peg, input);
    if (peg_err(p)) {
        printf("parse error....\n");
        peg_print(p);
        exit(1);
    }
}

// int main(void) {
//     printf("test-kit....\n");
// }
