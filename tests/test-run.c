#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pPEG.h"

/*
    # Example test file

    peg:
        date  = year '-' month '-' day
        year  = [0-9]*4
        month = [0-9]*2
        day   = [0-9]*2

    parse: date 2022-02-03

    End of test file.

A txt to read test files:

    file   = (block / line)*            
    block  = inset key [ \t]* line body  
    body   = (<gt inset> line / _blank)*
    key    = 'peg:' / 'parse:'

    line   = ~[\n\r]* _nl               
    inset  = [ \t]*                     
    _blank = [ \t]* [\n\r]+             
    _nl    = '\n' / '\r' '\n'? / !(~[])

This is hand coded here because the first test runs can't assume
that pPEG is working!  This txt can be one of the tests.

*/

// #include "../pPEG.h"

char* read_file(char* path) {
    char* buf = 0;
    int len;
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc(len);
        if (buf) {
            fread(buf, 1, len, f);
        }
        fclose(f);
    }
    if (buf) {
        buf[len] = '\0';
        return buf;
    }
    exit(-1);
}

int space(char *str, int pos) {
    while (str[pos] == ' ' || str[pos] == '\t') pos++;
    return pos;
}

int line(char *str, int pos) {
    while (str[pos] != '\0') {
        char c = str[pos++];
        if (c == '\r') break;
        if (c == '\n') {
            if (str[pos] == '\r') pos++;
            break;
        }
    }
    return pos;
}

int match(char *key, char *str, int pos) {
    while (*key++ == str[pos++]) {
        if (*key == '\0') return pos;
    }
    return -1;
}

char *keys[] = { "peg:", "use:", "parse:", "match:", "not:", ""};

enum KeyTag { PEG, USE, PARSE, MATCH, NOT };

int keytag = -1; // last key match

int key(char *str, int pos) {
    keytag = -1;
    for (int i=0;;i++) {
        char *kw = keys[i];
        if (kw[0] == '\0') return -1;
        int k = match(kw, str, pos);
        if (k>=0) {
            keytag = i;
            return k;
        }
    }
    exit(-1); // woops 
}

Peg* peg;

void process(char *txt, int i, int j) {
    while (txt[j-1] == '\n' || txt[j-1] == '\r') j--;

    if (keytag == PEG) {
        peg = peg_compile_text(txt, i, j);
        peg_print(peg);
    }
 
    if (keytag == USE) {
        peg = peg_compile_text(txt, i, j);
        if (peg_err(peg)) peg_print(peg);
    }

    if (keytag == PARSE) {
        Peg* p = peg_parse_text(peg, txt, i, j);
        peg_print(p);
    }    
    
    if (keytag == MATCH) {
        Peg* p = peg_parse_text(peg, txt, i, j);
        if (peg_err(p)) peg_print(p);
    }
    
    if (keytag == NOT) {
        Peg* p = peg_parse_text(peg, txt, i, j);
        if (!peg_err(p)) {
            printf("**** expected to fail:\n");
            peg_print(p);
        }
    }

}

// ========================================================

int main(int argc, char *argv[]) {
    printf("Hello pPEG test-run:\n");

    for (int i=1; i<argc; i++) {
        char* txt = read_file(argv[i]);
        printf("%s ...\n", argv[i]);
        int pos = 0;
        while (txt[pos] != '\0') {
            int i = space(txt, pos);
            int inset = i-pos;
            pos = line(txt, i);
            int j = key(txt, i); // sets keytag
            if (keytag >= 0) {
                int k = pos;
                while (txt[k] != '\0') {
                    int l = space(txt, k);
                    if (l-k <= inset) break;
                    k = line(txt, l);
                }
                pos = k;
                process(txt, space(txt, j), pos);
            }
        }
    }  

}
    