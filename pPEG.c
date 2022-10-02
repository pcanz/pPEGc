
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "pPEG.h"

#define MAX_STACK 512

// -- peg grammar --------------------------------

// TODO update grammar..... seq, rop, group, dq, ...

char* peg_grammar = 
"    Peg   = _ (rule _)+                          \n"
"    rule  = id _ '=' _ alt                       \n"
"                                                 \n"
"    alt   = seq ('/' _ seq)*                     \n"
"    seq   = rep*                                 \n"
"    rep   = pre sfx? _                           \n"
"    pre   = pfx? term                            \n"
"    term  = call / quote / chars / group / extn  \n"
"    group = '('_ alt ')'                         \n"
"                                                 \n"
"    call  = at? id _ multi? !'='                 \n"
"    at    = '@'                                  \n"
"    multi = '->' _ id _                            \n"
"    id    = [a-zA-Z_] [a-zA-Z0-9_-]*             \n"
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
"    extn  = '<' (id ' '*)* ~'>'* '>'             \n"
"    _     = (_WS+ / '#' ~_EOL*)*                 \n";
//   _WS   = [ \t\n\r]
//   _EOL  = [\n\r]


char *peg_names[] = { // used for the bootstrap rule_index
    "Peg", "rule",
    "alt", "seq", "rep", "pre", "term", "group",
    "call", "at", "multi",
    "id", "pfx", "sfx", "range", "num",
    "quote", "sq", "chars", "chs", "extn", "_"
};

// -- parser machine op-codes ------------------------------------------

// The op codes are matched one-one with peg_grammar rules so that the
// bootstrap op codes have the same values as the rule name index for the
// corresponding peg rule names. i.e peg tags == op-codes.

// BOOT = boot_code();  which must agree with these op codes. 

enum op {
    PEG, RULE,
    ALT, SEQ, REP, PRE, TERM, GROUP,
    CALL, AT, MULTI,
    ID, PFX, SFX, RANGE, NUM,
    QUOTE, SQ, CHARS, CHS, EXTN, WS
};

// Parser machine instruction op-codes:
// ALT, SEQ, REP, PRE, ID, SQ, CHS
// CALL, AT, MULTI

// -- implicit rule names ---------------------------------------------------

static char* implicit_names[] = { // _NULL = 0 is not an implicit rule name
    "_NULL", "_CHAR",             // _CHAR = 1 is an implicit char code
    "_TAB", "_CR", "_LF",         // other implicit rules...
    "_BS", "_DQ", "_SQ", "_BT",
    "_EOL", "_ANY",
    "_EOF", "_WS", "_NL", "_"          
};

enum IMPLICIT {     // NOTE: these must match with implicit_names[]
    _NULL, _CHAR,
    _TAB, _CR, _LF,
    _BS, _DQ, _SQ, _BT,
    _EOL, _ANY,
    _EOF, _WS, _NL, _UNDERSCORE  // these are builtins, not simple char ranges
};

// -- parse tree node: slot for application data (used by parser) ------

enum DATA_USE { NO_DATA, 
    DATA_VALS, RANGE_DATA, BUILTIN, 
    HEAP_STR, HEAP_ARR};

typedef union { 
    struct { // DATA_VALS, opx used for parser op code data...
        short int idx;
        char min;       // *N..M repeat
        char max;       // max 256 numeric repeat
        char sign;      // ~ ! & pfx
        char builtin;   // implicit rule, e.g. _NL
        char is_multi;  // bool flag multi usage
        char multi;     // B idx for A::B (idx may be 0)
    } opx;
    struct { // RANGE_DATA, implicit rule name UTF8 char range
        int min, max;
    } range;
    // HEAP_DATA, pointer for application data
    struct { 
        char *chars;
    } str;
    struct { 
        int *ints;
    } arr;
} Slot;

// -- parse tree node ------------------------------

// typedef struct Node Node; // in pPEG.h

struct Node {
    short int tag; // rule name index
    char data_use; // enum DATE_USE (use of Slot data)
    int start;     // start string span
    int end;       // last+1 string span
    int count;     // nodes count
    Slot data;     // application data -- op codes
    Node* nodes[]; // children node pointers
};

enum PEG_ERR { PEG_OK, PEG_PANIC, PEG_FELL_SHORT, PEG_FAILED };

char* peg_err_msg[] = {
    "ok", "PANIC",
    "Parse fell short",
    "Parse failed"
};

void panic(char* msg) {
    printf("*** panic: %s\n", msg);
    exit(PEG_PANIC);
}

static Node *newNode(int tag, int i, int j, int n) {
    Node *nd = (Node *)malloc(sizeof(Node) + n*sizeof(Node *));
    if (nd == NULL) panic("malloc");
    nd->tag = tag;
    nd->data_use = NO_DATA;
    nd->start = i;
    nd->end = j;
    nd->count = n; // number of children nodes
    return nd;
};

static void drop(Node* node) {
    if (node->data_use == HEAP_STR) {
        free(node->data.str.chars);
    }
    if (node->data_use == HEAP_ARR) {
        free(node->data.arr.ints);
    }
    for (int i=0; i<node->count; i+=1) {
        drop(node->nodes[i]);
    }
    free(node);
}

typedef struct Err Err;

struct Err {
    int err; // PEG_ERR
    int fail_rule;
    int pos;
    Node* expected;
};

Err* newErr(int code, int pos) {
    Err* err = malloc(sizeof(Err));
    if (!err) panic("malloc..");
    err->err = code;
    err->pos = pos;
    return err;
}

// == Peg peg ===================================================

// typedef struct Peg Peg; // in pPEG.h

struct Peg { // parse tree.........
    char* src;   // input string
    Node* tree;  // parse tree indexes into src string
    Peg* peg;    // grammar parse tree
    Err* err;    // error info
};

Peg* newPeg(char* src, Node* tree, Peg* gram, Err* err) {
    Peg* peg = malloc(sizeof(Peg));
    if (!peg) panic("malloc..");
    peg->src = src;
    peg->tree = tree;
    peg->peg = gram;
    peg->err = err;
    return peg;
}

// == Env for parser machine ========================================

typedef struct {
    char* grammar;  // source text
    Node* tree;     // peg rules
    char* input;
    int pos;        // parser cursor
    int end;        // end of input string (or end of % span)
    int depth;      // rule call depth (catch recursion)
    int stack;      // index into parse result Nodes
    Node* results[MAX_STACK]; // TODO elastic?

    int multi; // multi-rule node count

    int flags; // debug, trace
    int trace_pos;
    int trace_depth;
    bool trace_open;
    int fail;
    int fail_rule;
    Node* expected;
} Env;

/* -- UTF8 utils ----------------------------------------------

    0-7F            0xxx xxxx
    80-7FF          110x xxxx 10xx xxxx
    800-7FFF        1110 xxxx 10xx xxxx 10xx xxxx
    10000-10FFFF    1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx

*/

int utf8_size(int x) {
    if (x < 128) return 1;
    if (x < 0x800) return 2;
    if (x < 0x8000) return 3;
    return 4;
}

int utf8_len(char* p) {
    unsigned int c = (unsigned char)p[0];
    if (c < 127) return 1;            
    int x = c<<1, i = 2;
    while ((x <<= 1) & 0x80) i++;
    return i;
}

int utf8_read(char* p) {
    unsigned int c = (unsigned char)p[0];
    if (c < 127) return c;            
    int x = c<<1, i = 2;
    while ((x <<= 1) & 0x80) i++;
    x = (x & 0xFF) >> i;
    for (int n=1; n<i; n++) x = (x<<6)+((unsigned char)p[n] & 0x3F);
    return x;
}

int utf8_write(char* p, int x) {
    if (x < 128) {
        *p = x;
        return 1;
    }            
    if (x < 0x800) { // 110x xxxx 10xx xxxx
        *p = 0xC0 + (x >> 6);
        *(p+1) = 0x80 + (x & 0x3F);
        return 2;
    }
    if (x < 0x8000) { // 1110 xxxx 10xx xxxx 10xx xxxx
        *p = 0xE0 + (x >> 12);
        *(p+1) = 0x80 + ((x >> 6) & 0x3F);
        *(p+2) = 0x80 + (x & 0x3F);       
        return 3;
    }
    // 10000-10FFFF    1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx
    *p = 0xF0 + (x >> 18);
    *(p+1) = 0x80 + ((x >> 12) & 0x3F);
    *(p+2) = 0x80 + ((x >> 6) & 0x3F);
    *(p+3) = 0x80 + (x & 0x3F);       
    return 4;
}

// -- node utils ---------------------------------

char* node_text(char* str, Node* nd, char* out, int len) { // TODO len count escapes!!!
    if (len < 1) return out;
    int span = nd->end - nd->start; // text len
    int mid = len >> 1;
    int end = nd->end;
    int skip = end; // don't skip ... truncate to len if necessary
    int rest = end; // not used unless skip ...
    if (len < span+1) { // truncate or skip ... (allow for '\0' inside len)
        if (len > 11) { // skip ...
            skip = nd->start + mid - 3; // " ... " in approx middle
            rest = nd->end - mid + 3 ;  // " ... " 5 chars + 1 for \0
        } else { // truncate
            end = nd->start+len-2;
        }
    }
    for (int i = nd->start; i < end; i += 1) {
        unsigned char c = str[i];
        if (c < ' ') {
            if (c == '\n') strcpy(out, "\\n");
            else if (c == '\r') strcpy(out, "\\r");
            else if (c == '\t') strcpy(out, "\\t");
            else { // \x12 ...
                sprintf(out, "\\x%02X", c);
                out += 2;
            } 
            out+=2;
            continue;
        }
        if (c == '"' || c == '\\') *out++ = '\\';
        *out++ = c;
        if (i == skip) {
            strcpy(out, " ... ");
            out += 5;
            i = rest; // skip to rest
        } 
    }
    return out;
}

// -- peg print display ---------------------------

void print_text(char* str, Node* node) {
    char line[80];
    char* p = node_text(str, node, line, 80);
    *p = '\0';
    printf("%s", line);
}

void print_tag(Peg* peg, int tag) {
    if (!peg->peg) { // boot grammar...
        printf("%s", peg_names[tag]);
        return;
    }
    char* src = peg->peg->src;
    Node* tree = peg->peg->tree;
    Node* nd = tree->nodes[tag]->nodes[0]; // ID i..j
    print_text(src, nd);
}

char* node_quote(char* str, Node* nd, char* out, int len) {
    *out++ = '"';
    out = node_text(str, nd, out, len-2);
    *out++ = '"';
    return out;
}    

void print_tree(Peg* peg, Node* nd, int inset, unsigned long long last) {
    // last is a bit map to flag the last child nodes 
    for (int j=0; j<inset; j++) {
        if (j == inset-1) { // max inset ...
            if (last&(0x1ULL<<inset)) printf("%s", "\u2514\u2500");  // `-
                else printf("%s", "\u251C\u2500");   // |-
        }
        else if (last&(0x1ULL<<(j+1))) printf("%s", "  ");
            else printf("%s", "\u2502 ");  // |
    }
    if (!nd) { printf("NULL\n"); return; }
    if (nd->count > 0) {
        print_tag(peg, nd->tag);
        printf("\n");
        for (int i=0; i<nd->count; i++) {
            if (i == nd->count-1) last = last|(0x1ULL<<(inset+1));
            print_tree(peg, nd->nodes[i], inset+1, last);
        };
        return;
    }
    // leaf node...
    print_tag(peg, nd->tag);
    char txt[50];
    char* t = &txt[0];
    t = node_quote(peg->src, nd, t, 49);
    *t = '\0';       
    printf(" %s\n", txt);
}

void print_ptree(Peg* peg) {
    if (peg && peg->tree) {
        print_tree(peg, peg->tree, 0, 0);
    } else {
        printf("No ptree...\n");
    }
}

// -- fault reporting ----------------------------------------

void print_cursor(char* p, int pos) {
    int i = pos;
    while (i > 0 && p[i] != '\n' && p[i] != '\r') i--;
    if (i>0) i++; // after line break
    int j = pos;
    while (p[j] != 0 && p[j] != '\n' && p[j] != '\r') j++;
    for (int k=i; k<j; k++) printf("%c", p[k]);
    printf("\n");
    for (int k=i; k<pos; k++) {
        printf(" ");
        k += utf8_len(p+k)-1;
    }
    printf("^\n");
}

void print_line_num(char* p, int pos) {
    int cr = 0;
    int lf = 0;
    int i = pos;
    while (i > 0 && (unsigned char)p[i] >= ' ') i--;
    int col = pos-i;
    do {
        if (p[i] == '\n') lf++;
        if (p[i] == '\r') cr++;
    } while (i-- > 0);
    int ln = lf > cr? lf : cr;
    printf("%d.%d", ln+1, col+1);
}

// -- fault report -----------------------------------------

void fault_report(Peg* peg) {
    printf("%s in rule: ", peg_err_msg[peg->err->err]);
    print_tag(peg, peg->err->fail_rule);
    printf("\n");
    if (peg->err->expected) {
        char open = ' ';
        char close = ' ';
        if (peg->err->expected->tag == SQ) { open = '\''; close = '\''; }
        if (peg->err->expected->tag == CHS) { open = '['; close = ']'; }
        printf("expected: %c", open);
        print_text(peg->peg->src, peg->err->expected);
        printf("%c\n", close);
    }
    printf("on line: ");
    print_line_num(peg->src, peg->err->pos);
    printf(" at: %d of %lu\n", peg->err->pos, strlen(peg->src));
    print_cursor(peg->src, peg->err->pos);
} 

// -- debug trace op codes display ---------------------------------

void show_exp(Env *pen, Node *exp, char* out, int len) {
    int tag = exp->tag;
    char *name = peg_names[tag];
    int n = strlen(name);
    if (n+2 > len) panic("show_exp overflow...");
    strcpy(out, name);
    out += n; // strlen(name);
    *out++ = ' ';
    if (tag == ID || tag == SQ || tag == CHS) {
        out = node_quote(pen->grammar, exp, out, len-(n+3));
    }
    if (tag == PRE) {
        *out++ = exp->data.opx.sign;
    }
    if (tag == REP) {
        int min = exp->data.opx.min;
        int max = exp->data.opx.max;
        if (min==0 && max==0) *out++ = '*';
        if (min==1 && max==0) *out++ = '+';
        if (min==0 && max==1) *out++ = '?';
    }
    *out = '\0';
}

void debug_trace(Env *pen, Node* exp) {
    char c = pen->input[pen->pos];
    if (c == '\n') c = ' ';
    printf("%d: %c\t", pen->pos, c);
    char show[100];
    show_exp(pen, exp, show, 99);
    printf("%s\n", show);
}

// -- rule trace ---------------------------------------------

void trace_quote(char* str, int start, int end) {
    printf("\"");
    for (int i=start; i<end; i++) {
        char c = str[i];
        if (c == '\n') printf("\\n");
        else if (c == '\"') printf("\\\"");
        else printf("%c", str[i]);
    }
    printf("\"");   
}

void print_tag_name(Env* pen, int tag) {
    if (!pen->tree) { // boot grammar...
        printf("%s", peg_names[tag]);
        return;
    }
    Node* node = pen->tree->nodes[tag]->nodes[0]; // ID i..j
    for (int i=node->start; i<node->end; i++) {
        printf("%c", pen->grammar[i]);
    }
}

void rule_trace_open(Env *pen, int tag) {
    if (pen->pos > pen->trace_pos) {
        printf("\n");
        for (int i=0; i<pen->trace_depth; i++) printf("| ");
        trace_quote(pen->input, pen->trace_pos, pen->pos);
        pen->trace_pos = pen->pos;
    }
    printf("\n");
    for (int i=0; i<pen->trace_depth; i++) printf("| ");
    print_tag_name(pen, tag);
    pen->trace_depth++;
    pen->trace_open = true; 
}

void rule_trace_close(Env *pen, int tag, bool result) {
    if (pen->pos < pen->trace_pos) {
        printf(" <= !");
    }
    bool txt = pen->trace_pos < pen->pos;
    if (!pen->trace_open) {
        if (txt || !result) {
            printf("\n");
            for (int i=0; i<pen->trace_depth; i++) printf("| ");
        }
    }
    if (result) {
        if (txt) trace_quote(pen->input, pen->trace_pos, pen->pos);
    } else {
        printf("!");
    }
    pen->trace_pos = pen->pos;
    pen->trace_depth--;
    pen->trace_open = false;
}

// -- implicit rules -----------------------------------------------

int implicit_def(char* name) {
    int n = sizeof(implicit_names)/sizeof(char *); // names count;
    for (int i=2; i<n; i+=1) {
        if (strcmp(implicit_names[i], name) == 0) 
            return i; // index of defined rule
    }
    return _NULL; // not defined as an implicit rule
}

int implicit_rule(char* name, int len) {
    if (name[0] != '_') return _NULL;
    if (len == 1) return implicit_def(name); // _UNDERSCORE
    bool range = false; // range example: _123-FFF 
    for (int i=1; i<len; i+=1) { // validate as a char code...
        char c = name[i];        // [0-9A-F]+ ('-' [0-9A-F]+)?
        if ((c >= 'A' && c <= 'F') || (c>='0' && c<='9')) continue;
        if (c == '-') { // only one '-' 
            if (i > 0 && i < len-1 && !range) {
                range = true; // with at least one digit either side..
                continue;
            }
        } else { // Not a char code ......
            return implicit_def(name); // implicit definition or _NULL
        }
    } // rule name IS a char code  
    return _CHAR; // implicit character code 
}

bool implicit_range(Node* exp, int min, int max) {
    exp->data.range.min = min;
    exp->data.range.max = max;
    exp->data_use = RANGE_DATA;
    return true;
}

bool implicit_builtin(Node* exp, int code) {
    exp->data.opx.builtin = code;
    exp->data_use = BUILTIN;
    return true;
}

bool resolve_implicit(Node* exp, char* name, int len) {
    int imp = implicit_rule(name, len); // validate hex digits..
    if (imp == _NULL) return false;
    if (imp == _CHAR) {
        int min = 0, max = 0;
        for (int i=1; i<len; i+=1) {
            int c = (unsigned char)name[i];
            if (c == '-') { 
                for (int j=i+1; j<len; j+=1) {
                    int c = (unsigned char)name[j];
                    max = (max<<4) + c-(c>='0'&&c<='9'? '0' : 55); // 'A'=65
                }
                break;
            };
            min = (min<<4) + c-(c>='0'&&c<='9'? '0' : 55); // 'A'=65
        }
        if (max < min) max = min;
        return implicit_range(exp, min, max);
    }
    // if (imp == _WS) return implicit_range(exp, 0x9, 0x20); // => BUILTIN
    if (imp == _TAB) return implicit_range(exp, 0x9, 0x9);
    if (imp == _LF) return implicit_range(exp, 0xA, 0xA);
    if (imp == _CR) return implicit_range(exp, 0xD, 0xD);
    if (imp == _BS) return implicit_range(exp, 0x5C, 0x5C); // back slash
    if (imp == _SQ) return implicit_range(exp, 0x27, 0x27);
    if (imp == _DQ) return implicit_range(exp, 0x22, 0x22);
    if (imp == _BT) return implicit_range(exp, 0x60, 0x60); // back tick `
    if (imp == _EOL) return implicit_range(exp, 0xA, 0xD);
    if (imp == _ANY) return implicit_range(exp, 0x0, 0x10FFFF);
    
    if (imp == _EOF) return implicit_builtin(exp, _EOF); // _9-D / ' '
    if (imp == _NL) return implicit_builtin(exp, _NL);   // _LF / _CR _LF?
    if (imp == _WS) return implicit_builtin(exp, _WS);   // _9-D / ' '
    if (imp == _UNDERSCORE) return implicit_builtin(exp, _UNDERSCORE); // _WS*

    return false; // undefined implicit rule name
}

// -- resolve opx data slot extensions -------------------

void resolve_id(Env* pen, Node* exp) {
    char name[50];
    char *p = node_text(pen->grammar, exp, name, 50);
    *p = '\0';
    int len = p-name;
    for (int i=0; i<pen->tree->count; i++) {
        Node* id = pen->tree->nodes[i]->nodes[0];
        int n = id->end-id->start;
        if (n != len) continue;
        bool match = true;
        for (int j=0; j<len; j++) {
            if (pen->grammar[id->start+j] != name[j]) {
                match = false;
                break;
            }
        }
        if (!match) continue;
        exp->data.opx.idx = i;
        exp->data_use = DATA_VALS;
        return;
    }
    if (!resolve_implicit(exp, name, len)) {
        char msg[50];
        sprintf(msg, "*** Undefined rule: %s\n", name); // TODO improve this..
        panic(msg);
    }
}

unsigned char rep_num(char* src, int start, int end) {
    int num = 0;        
    for (int i = start; i < end; i++) {
        num = num*10 + src[i]-'0';
    }
    if (num > 255) {
        char msg[50];
        sprintf(msg, "Repeat * %d is too big, limit 255\n", num);
        panic(msg);
    }
    return (unsigned char)num;
}

void resolve_rep(Env* pen, Node* exp) {
    unsigned char max=0, min=0;
    Node* sfx = exp->nodes[1];
    if (sfx->tag == SFX) {
        char sign = pen->grammar[sfx->start];
        if (sign == '+') min = 1;
        if (sign == '?') max = 1;
    } else if (sfx->tag == NUM) {
        min = rep_num(pen->grammar, sfx->start, sfx->end);
        max = min;
    } else if (sfx->tag == RANGE) {
        Node* num1 = sfx->nodes[0];
        min = rep_num(pen->grammar, num1->start, num1->end);
        Node* num2 = sfx->nodes[1];
        max = rep_num(pen->grammar, num2->start, num2->end);
    } else panic("woops..");
    exp->data.opx.min = min;
    exp->data.opx.max = max;
    exp->data_use = DATA_VALS;
}

void resolve_pre(Env* pen, Node* exp) {
    Node* pfx = exp->nodes[0];
    char sign = pen->grammar[pfx->start];
    exp->data.opx.sign = sign;
    exp->data_use = DATA_VALS;
}

int hex(int n, char *src) {
    int code = 0;
    for (int i=0; i<n; i++) {
        char c = src[i];
        if (c >= '0' && c <= '9') {
            code = (code<<4) + (c-'0');
        } else if (c >= 'A' && c <= 'F') {
            code = (code<<4) + (c-'A') + 10;
        } else if (c >= 'a' && c <= 'f') {
            code = (code<<4) + (c-'a') + 10;
        } else return 0;
    }
    return code;
}

int node_ints(Env* pen, Node* exp, int *out) {
    int *start = out;
    int len = exp->end - exp->start;
    char *src = pen->grammar + exp->start;
    for (int i=0; i<len; i++) { // copy UTF-8 bytes....
        unsigned char c = src[i];
        if (c == '\\' && i<len-1) {
            unsigned char x = src[i+1];
            int code = -1;
            if (x == 't') code = 9;  // TAB
            if (x == 'n') code = 10; // LF
            if (x == 'r') code = 13; // CR
            if (x == 'u' && i < len-5) code = hex(4, src+i+2);
            if (x == 'U' && i < len-9) code = hex(8, src+i+2);
            if (code == -1) { // no escape code
                *out++ = '\\';
            } else { // escape => code
                int n = 1; // esc src len e.g. \n = 1
                if (x == 'u') n = 5;
                if (x == 'U') n = 9;
                i += n; // skip over escape format
                *out++ = code;
            }
        } else {
            *out++ = c;
        }
    }
    return out-start; // length
}

void resolve_sq(Env* pen, Node* exp) {
    int codes[128];
    int len = node_ints(pen, exp, codes);
    char *str = malloc(len+1); // [0]=len
    if (str == NULL) panic("malloc");
    str[0] = len;
    char *res = str+1;
    for (int i=0; i<=len; i++) {
        res += utf8_write(res, codes[i]); // res += utf8_size
    }
    exp->data.str.chars = str;
    exp->data_use = HEAP_STR;
}

void resolve_chs(Env* pen, Node* exp) {
    int codes[128];
    int len = node_ints(pen, exp, codes);
    int *vals = malloc(sizeof(int)*(len+1)); // [0]=len
    if (vals == NULL) panic("malloc");
    vals[0] = len;
    for (int i=0; i<=len; i++) {
        vals[i+1] = codes[i];
    }
    exp->data.arr.ints = vals;
    exp->data_use = HEAP_ARR;
}

char *extn_names[] = {
    "do", "id", "eq", "lt", "gt", "le", "ge"
};

enum extn_tag {
    EXT_do, EXT_id,
    EXT_eq, EXT_lt, EXT_gt, EXT_le, EXT_ge
};

bool ext_do(Env *, Node *); // <do id1 id2>
bool ext_do_ids(Env *, Node *, Node *); // id1 -> id2
bool ext_compare(Env *, Node* , int);

void resolve_extn(Env* pen, Node* exp) {
    int tag = -1; // EXT_undefined
    if (exp->count > 0) { // <cmd ... >
        Node* cmd = exp->nodes[0];
        int cmd_len = cmd->end-cmd->start; 
        int n = sizeof(extn_names)/sizeof(char *);
        for (int i=0; i<n; i+=1) {
            char *key = extn_names[i];
            int k = 0;
            for (int j=cmd->start; j<cmd->end; j++) {
                 if (key[k] != pen->grammar[j]) break;
                 k += 1;
            }
            if (k == cmd_len) {
                tag = i;
                break;
            }
        } // ... ext_names[i]
        for (int i=1; i<exp->count; i++) {
            Node* arg = exp->nodes[i];
            resolve_id(pen, arg); // <cmd id-args...>
        }
    }
    exp->data.opx.idx = tag;
    exp->data_use = DATA_VALS;
}

// == bootstrap peg_code constructors =================================

Node *ops(int tag, int n, ...) {
    Node *nd = (Node *)malloc(sizeof(Node)+n*sizeof(Node *));
    if (nd == NULL) return NULL;
    nd->tag = tag; // rule name index
    nd->count = n;
    va_list argp;
    va_start(argp, n);
    for (int i=0; i<n; i+=1) {
        nd->nodes[i] = va_arg(argp, void*);
    }
    va_end(argp);
    return nd;
}

int findstr(char *str) { // index of str span peg_grammar
    char* src = peg_grammar;
    char* p = strstr(src, str);
    if (p) return p - src;
    char msg[100];
    sprintf(msg, "Boot: '%s' not found in peg grammar...\n", str);
    panic(msg);
    return 0;
}

int rule_index(const char *name) { // index of name in peg_names
    int n = sizeof(peg_names)/sizeof(char *); // rule_count;
    for (int i=0; i<n; i+=1) {
        if (strcmp(peg_names[i], name) == 0) return i;
    }
    char msg[100];
    sprintf(msg, "Boot: '%s' is not a peg grammar rule name... \n", name);
    panic(msg);
    return 0;
}

Node *op(int tag, char *str) { // tag: ID, SQ, or CHS
    Node *nd = (Node *)malloc(sizeof(Node));
    if (nd == NULL) return NULL;
    nd->tag = tag;
    nd->count = 0;
    int i = findstr(str); // index in peg_grammar text
    nd->start = i;
    nd->end = i + strlen(str);
    if (tag == ID) {
        if (peg_grammar[i] == '_') {   // chk for implicit char rule...
            char c = peg_grammar[i+1]; // let run() resolve it...
            if (c>='0' && c<='9' || c>='A' && c<='F') return nd;
            if (strcmp(str, "_WS") == 0) return nd;
            if (strcmp(str, "_EOL") == 0) return nd;
        } 
        nd->data.opx.idx = rule_index(str);
        nd->data_use = DATA_VALS;
    }
    return nd;
}

Node *opREP(Node *opx, char* sfx) { // rep(op, sfx)
    Node *nd = (Node *)malloc(sizeof(Node)+2*sizeof(Node *));
    if (nd == NULL) return NULL;
    nd->tag = REP;
    nd->count = 2;
    nd->nodes[0] = opx;
    nd->nodes[1] = op(SFX, sfx);
    unsigned char min = 0, max = 0;
    if (sfx[0] == '+') min = 1;
    if (sfx[0] == '?') max = 1;
    nd->data.opx.min = min;
    nd->data.opx.max = max;
    nd->data_use = DATA_VALS;
    return nd;
}

Node *opPRE(char* pfx, Node* opx) {
    Node *nd = (Node *)malloc(sizeof(Node)+2*sizeof(Node *));
    if (nd == NULL) return NULL;
    nd->tag = PRE;
    nd->count = 2;
    nd->nodes[0] = op(PFX, pfx);
    nd->nodes[1] = opx;
    nd->data.opx.sign = pfx[0];
    nd->data_use = DATA_VALS;
    return nd;
}

// -- bootstrap peg grammar parse tree --------------------

Node *boot_code() {
    return ops(PEG, 22,
    //   Peg   = _ (rule _)+
    ops(RULE, 2, op(ID, "Peg"),
        ops(SEQ, 2, op(ID, "_"),
            opREP(ops(SEQ, 2, op(ID, "rule"),
                        op(ID, "_")), "+"))),               
    //   rule  = id _ '=' _ alt
    ops(RULE, 2, op(ID, "rule"),
        ops(SEQ, 5, op(ID, "id"), op(ID, "_"), op(SQ, "="),
                op(ID, "_"), op(ID, "alt"))),           

    //  alt   = seq ('/'_ seq)*
    ops(RULE, 2, op(ID, "alt"),
        ops(SEQ, 2, op(ID, "seq"),
            opREP(ops(SEQ, 3, op(SQ, "/"), op(ID, "_"),
                    op(ID, "seq")), "*"))),        
    //  seq   = rep*
    ops(RULE, 2, op(ID, "seq"),
        opREP(op(ID, "rep"), "*")),
    //   rep   = pre sfx? _
    ops(RULE, 2, op(ID, "rep"),
        ops(SEQ, 3, op(ID, "pre"),
            opREP(op(ID, "sfx"), "?"), op(ID, "_"))),                   
    //   pre   = pfx? term                  
    ops(RULE, 2, op(ID, "pre"),
        ops(SEQ, 2, opREP(op(ID, "pfx"), "?"),
            op(ID, "term"))),                   
    //   term  = call / quote / chars / group
    ops(RULE, 2, op(ID, "term"),
        ops(ALT, 5, op(ID, "call"), op(ID, "quote"), 
            op(ID, "chars"), op(ID, "group"), op(ID, "extn"))),
    //   group = '(' _ alt _ ')'
    ops(RULE, 2, op(ID, "group"),
        ops(SEQ, 5, op(SQ, "("), op(ID, "_"), op(ID, "alt"),
            op(ID, "_"), op(SQ, ")"))),            

    //   call  = at? id _ multi? !'='
    ops(RULE, 2, op(ID, "call"),
        ops(SEQ, 5, opREP(op(ID, "at"), "?"),
            op(ID, "id"), op(ID, "_"), opREP(op(ID, "multi"), "?"),
            opPRE("!", op(SQ, "=")))),
    //   at    = '@'
    ops(RULE, 2, op(ID, "at"), op(SQ, "@")),                      
    //   multi = '->' _ id _
    ops(RULE, 2, op(ID, "multi"),
        ops(SEQ, 4, op(SQ, "->"), op(ID, "_"), op(ID, "id"), op(ID, "_"))),                      

    //   id    = [a-zA-Z_] [a-zA-Z0-9_-]*
    ops(RULE, 2, op(ID, "id"),
        ops(SEQ, 2, op(CHS, "a-zA-Z_"), 
            opREP(op(CHS, "a-zA-Z0-9_-"), "*"))),

    //   pfx   = [&!~]
    ops(RULE, 2, op(ID, "pfx"), op(CHS, "~!&")),                      
    //   sfx   = [+?] / '*' range?                      
    ops(RULE, 2, op(ID, "sfx"),
        ops(ALT, 2, op(CHS, "+?"),
            ops(SEQ, 2, op(SQ, "*"),
                opREP(op(ID, "range"), "?")))),
    // range = num ('..' num)?
    ops(RULE, 2, op(ID, "range"),
        ops(SEQ, 2, op(ID, "num"),
            opREP(ops(SEQ, 2, op(SQ, ".."), op(ID, "num")), "?"))),
    // num = [0-9]+
    ops(RULE, 2, op(ID, "num"), opREP(op(CHS, "0-9"), "+")),

    //   quote = ['] sq ['] 'i'?
    ops(RULE, 2, op(ID, "quote"),
        ops(SEQ, 4, op(SQ, "'"), op(ID, "sq"), op(SQ, "'"),
                opREP(op(SQ, "i"), "?"))),                
    //   sq    = ~[']*
    ops(RULE, 2, op(ID, "sq"),
        opREP(opPRE("~", op(SQ, "'")), "*")),                      
    //   chars = '[' chs ']'                
    ops(RULE, 2, op(ID, "chars"),
        ops(SEQ, 3, op(SQ, "["), op(ID, "chs"), op(SQ, "]"))),                
    //   chs   =  ~']'*                     
    ops(RULE, 2, op(ID, "chs"),
        opREP(opPRE("~", op(SQ, "]")), "*")),
    //   extn = '<' (id ' '*)* ~'>'* '>'
    ops(RULE, 2, op(ID, "extn"),
        ops(SEQ, 4, op(SQ, "<"),
            opREP(ops(SEQ, 2, op(ID, "id"),
                    opREP(op(SQ, " "), "*")), "*"),
            opREP(opPRE("~", op(SQ, ">")), "*"),
            op(SQ, ">"))),                   
    //   _  = (_WS+ / '#' ~_EOL*)*
    ops(RULE, 2, op(ID, "_"),
        opREP(ops(ALT, 2, opREP(op(ID, "_WS"), "+"),
            ops(SEQ, 2, op(SQ, "#"),
                opREP(opPRE("~", op(ID, "_EOL")), "*"))), "*"))              
    );
}

// == Bootstrap ======================================

Peg* BOOT = NULL;

// BOOT = { peg_grammar, boot_code(), NULL};
void bootstrap() {
    Peg* peg = (Peg *)malloc(sizeof(Peg));
    if (!peg) panic("boot malloc..");
    peg->src = peg_grammar;
    peg->tree = boot_code();
    peg->peg = NULL;
    peg->err = NULL;
    BOOT = peg;
}

// == parser machine engine ==============================

bool run(Env *pen, Node *exp) {
    if (pen->flags == 1) debug_trace(pen, exp);
    switch (exp->tag) {
    case ID: {
        if (exp->data_use == NO_DATA) resolve_id(pen, exp);
        if (exp->data_use == RANGE_DATA) { // implicit char rule name
            if (pen->pos >= pen->end) return false;
            int size = 1; // char bytes...
            int c = (unsigned char)pen->input[pen->pos];
            if (c > 127) {
                c = utf8_read(pen->input);
                size = utf8_size(c);
            }
            if (c < exp->data.range.min) return false;
            if (c > exp->data.range.max) return false;
            pen->pos += size;
            return true;
        }
        if (exp->data_use == BUILTIN) { // implicit rule...
            switch (exp->data.opx.builtin) {
                case _WS: { // _WS = _9-D / ' '
                    if (pen->pos >= pen->end) return false;
                    char c = pen->input[pen->pos];
                    if (c == 0x20 || (c <= 0xD && c>= 0x9)) {
                        pen->pos++;
                        return true;
                    }
                    return false;              
                }
                case _UNDERSCORE: { // _ = _WS*
                    while (pen->pos < pen->end) {
                        char c = pen->input[pen->pos];
                        if (c == 0x20 || (c <= 0xD && c>= 0x9)) {
                            pen->pos++;
                        } else break;
                    }
                    return true;
                }
                case _NL: { // _NL = _LF / CR LF?
                    if (pen->pos >= pen->end) return false;
                    char c = pen->input[pen->pos];
                    if (c=='\n') {
                        pen->pos++;
                        return true;
                    }
                    if (c=='\r') {
                        pen->pos++;
                        if (pen->input[pen->pos] == '\n') pen->pos++;
                        return true;                       
                    }
                    return false;                   
                }
                case _EOF: { // !_ANY
                    if (pen->pos >= pen->end) return true;
                    return false;
                }
                default: panic("undefined builtin...");
            }
        }
        int tag = exp->data.opx.idx;
        int start = pen->pos;
        int stack = pen->stack;
        Node* rule = pen->tree->nodes[tag]->nodes[1];
        if (pen->flags == 2) rule_trace_open(pen, tag);
        if (pen->depth++ > MAX_STACK)
            panic("call recursion > MAX_STACK ....");
        bool result = run(pen, rule);
        pen->depth--;
        if (pen->flags == 2) rule_trace_close(pen, tag, result);
        if (pen->fail_rule < 0) pen->fail_rule = tag;
        if (result) {
            char first = pen->grammar[exp->start]; // tag name char
            if (first == '_') return true;
            int n = pen->stack-stack; // nodes count
            if (n == 1 && first > 'Z') return true;
            Node *nd = newNode(tag, start, pen->pos, n);
            for (int i=0; i<n; i++) {
                nd->nodes[i] = pen->results[stack+i];
            };
            pen->results[stack] = nd;
            pen->stack = stack+1;
            if (pen->stack >= MAX_STACK) { // TODO elastic stack...
                panic("MAX_STACK exceeded...");
            }
        }
        return result;
    }
    case SEQ: {
        int n = exp->count;
        for (int i=0; i<n; i+=1) {
            Node *op = exp->nodes[i];
            if (!run(pen, op)) {
                if (i>0 && pen->pos > pen->fail) {
                    pen->fail = pen->pos;
                    pen->fail_rule = -1; // flag for ID
                    pen->expected = op;
                }
                return false;
            }
        }
        return true;
    }
    case ALT: {
        int n = exp->count;
        int pos = pen->pos;
        int stack = pen->stack;
        for (int i=0; i<n; i+=1) {
            Node *op = exp->nodes[i];
            if (run(pen, op)) return true;
            if (pen->pos != pos) {
                pen->pos = pos;
            }
            if (pen->stack > stack) {
                for (int i=stack; i<stack; i++) {
                    drop(pen->results[i]);
                }
                pen->stack = stack;
            }
        }
        return false;
    }
    case REP: {
        if (exp->data_use == NO_DATA) resolve_rep(pen, exp);
        Node *op = exp->nodes[0];
        int min = exp->data.opx.min;
        int max = exp->data.opx.max;
        int pos = pen->pos;
        int stack = pen->stack;
        int count = 0;
        do {
            bool result = run(pen, op);
            if (!result) break;
            if (pen->pos == pos) break; // no porgress
            pos = pen->pos;
            stack = pen->stack;
            count += 1;
        } while (count != max);
        if (count < min) return false;
        if (pen->pos > pos) {
            pen->pos = pos; // reset last run failure
        }
        if (pen->stack > stack) {
            for (int i=stack; i<stack; i++) {
                drop(pen->results[i]);
            }
            pen->stack = stack;
        }
        return true;
    }
    case PRE: {
        if (exp->data_use == NO_DATA) resolve_pre(pen, exp);
        Node *op = exp->nodes[1];
        char sign = exp->data.opx.sign;
        int pos = pen->pos;
        int stack = pen->stack;
        bool result = run(pen, op);
        pen->pos = pos; // reset
        if (pen->stack > stack) {
            for (int i=stack; i<stack; i++) {
                drop(pen->results[i]);
            }
            pen->stack = stack;
        }
        if (sign == '~') {
            if (result) return false;
            if (pos < pen->end) {
                pen->pos = pos+1; // UTF-8 continues ...
                while ((pen->input[pen->pos] & 0xC0) == 0x80) pen->pos++;
                return true;
            }
            return false;
        }
        if (sign == '!') return !result;
        return result;
    }
    case SQ: { 
        if (pen->pos+exp->end-exp->start > pen->end) return false;
        if (exp->data_use == NO_DATA) resolve_sq(pen, exp);
        unsigned char len = exp->data.str.chars[0];
        if (pen->grammar[exp->end+1] != 'i') { // normal case sensitive...
            for (int i=1; i<=len; i+=1) {
                if (pen->input[pen->pos] != exp->data.str.chars[i]) return false;
                pen->pos += 1; // any bytes, so UTF-8 ok
            }
            return true;
        } // 'xyz'i ASCII only case insensitive...  TODO Unicodes
        for (int i=1; i<=len; i+=1) {
            unsigned char c1 = pen->input[pen->pos];
            if (c1 >= 'a' && c1 <= 'z') c1 = c1-32;
            unsigned char c2 = exp->data.str.chars[i]; // TODO compile time..
            if (c2 >= 'a' && c2 <= 'z') c2 = c2-32;
            if (c1 != c2) return false;
            pen->pos += 1;
        }
        return true;
    }
    case CHS: {
        if (pen->pos >= pen->end) return false;       
        if (exp->data_use == NO_DATA) resolve_chs(pen, exp);
        int len = exp->data.arr.ints[0];
        int c = (unsigned char)pen->input[pen->pos];
        int n = 1; // char size
        if (c > 127) {
            c = utf8_read(pen->input);
            n = utf8_size(c);
        }
        for (int i=1; i<=len; i++) { // 1..len
            int code = exp->data.arr.ints[i];
            if (i<len-1 && exp->data.arr.ints[i+1] == '-') {
                int max = exp->data.arr.ints[i+2];
                i += 2;
                if (c < code || c > max) continue;
                pen->pos += n;
                return true;
            } else if (c == code) {
                pen->pos += n;
                return true;
            }
        }
        return false;
    }
    case CALL: { // call = at? id _ ("->" id _)?
        if (exp->count == 2 && exp->nodes[0]->tag == AT) { // @id
            Node* id = exp->nodes[1];
            if (id->data_use == NO_DATA) resolve_id(pen, id);
            return ext_compare(pen, exp, EXT_id);
        }
        if (exp->count == 2 && exp->nodes[0]->tag == ID) { // id1 -> id2
            Node* id1 = exp->nodes[0];
            if (id1->data_use == NO_DATA) resolve_id(pen, id1);
            Node* id2 = exp->nodes[1];
            if (id2->data_use == NO_DATA) resolve_id(pen, id2);
            return ext_do_ids(pen, id1, id2);
        }
        printf("*** Not implemented: "); // TODO improve err reporting
        print_text(pen->grammar, exp);
        printf("\n");
        return false;
    }
    case EXTN: {
        if (pen->pos >= pen->end) return false;       
        if (exp->data_use == NO_DATA) resolve_extn(pen, exp);
        int tag = exp->data.opx.idx;
        switch (tag) {
            case EXT_do: return ext_do(pen, exp);
            case EXT_id: return ext_compare(pen, exp, EXT_id);
            case EXT_eq: return ext_compare(pen, exp, EXT_eq);
            case EXT_lt: return ext_compare(pen, exp, EXT_lt);
            case EXT_gt: return ext_compare(pen, exp, EXT_gt);
            case EXT_le: return ext_compare(pen, exp, EXT_le);
            case EXT_ge: return ext_compare(pen, exp, EXT_ge);
            default: { // TODO better err reporting...
                printf("**** Undefined extn: ");
                print_text(pen->grammar, exp);
                printf("\n");
                return false;
            }
        }
        return false;        
    }

    default: {
        char msg[50];
        sprintf(msg, "woops: undefined op: %d\n", exp->tag);
        panic(msg);
        return false;
      }
    } // switch
} // run

// -- Extensions -------------------------------------------------

bool ext_do(Env* pen, Node* exp) { // <do x y>
    if (exp->count != 3) return false;
    return ext_do_ids(pen, exp->nodes[1], exp->nodes[2]);
}

bool ext_do_ids(Env *pen, Node *id1, Node *id2) { // id1 -> id2
    int stack = pen->stack;
    bool result = run(pen, id1);
    if (result && pen->stack > stack) {
        pen->multi++;
        Node* node = pen->results[stack];
        node->data_use = DATA_VALS;
        node->data.opx.is_multi = 1;
        node->data.opx.multi = id2->data.opx.idx;
    }
    return result;
}

Node *find_prior(Env *pen, int tag) {    
    int k = pen->stack;
    while (k-- > 0) { // seach prior parse tree sibling nodes...
        Node* node = pen->results[k];
        if (node->tag == tag) return node;
    }
    return NULL;
}

bool ext_compare(Env* pen, Node* exp, int key) {
    if (exp->count < 2) return false;
    Node *id = exp->nodes[1];
    int tag = id->data.opx.idx;
    Node *prior = find_prior(pen, tag);
    int len = 0; // prior match length
    if (prior != NULL) len = prior->end-prior->start;           
    int start = pen->pos;
    bool result = run(pen, id);
    if (!result) return false;
    int size = pen->pos-start;
    if (key == EXT_eq) return size == len? true : false;
    if (key == EXT_lt) return size < len? true : false;
    if (key == EXT_gt) return size > len? true : false;
    if (key == EXT_le) return size <= len? true : false;
    if (key == EXT_ge) return size >= len? true : false;
    if (key ==  EXT_id) {
        if (size != len) return false;
        for (int i=0; i<len; i++) {
            int pos = prior->start; // len > 0
            if (pen->input[start+i] != pen->input[pos+i]) return false;
        }
        return true;
    }
    printf("woops, ext_compare key=%d\n", key);
    return false;
}

// -- Transform X -> Y ---------------------------------------------

void multi_transform(Env* pen, Node* parent) {
    for (int i=0; i<parent->count; i++) {
        Node* node = parent->nodes[i]; 
        if (node->data_use == DATA_VALS && node->data.opx.is_multi) {
            int tag = node->data.opx.multi;
            pen->multi--;
            pen->pos = node->start;
            pen->end = node->end;
            Node* rule = pen->tree->nodes[tag]->nodes[0];
            int stack = pen->stack;
            bool result = run(pen, rule);
            if (result && pen->pos == node->end) {
                drop(parent->nodes[i]);
                parent->nodes[i] = pen->results[stack--];
            }
        } else {
            multi_transform(pen, node);
        }
    }
}

// ==  Parser  ============================================

Peg* peg_parser(Peg* peg, char* input, int start, int end, int flags) {
    if (!peg) { // peg_compile(BOOT, ...)  TODO flags mask for raw+trace...
        if (!BOOT) bootstrap();
        peg = BOOT;
    }
    if (peg->err) {
        fault_report(peg);
        panic("grammar error...");
    }
    Env pen;
    pen.grammar = peg->src;
    pen.tree = peg->tree;
    pen.input = input;
    pen.pos = start;
    pen.end = end; //strlen(input);
    pen.depth = 0;
    pen.stack = 0;
    pen.multi = 0;
    pen.flags = flags;
    pen.trace_pos = 0;
    pen.trace_depth = 0;
    pen.trace_open = true;
    pen.fail = 0;
    pen.fail_rule = 0;
    pen.expected = NULL;

    Node* begin = pen.tree->nodes[0]->nodes[0]; // op(ID, <rule.0>)

    bool result = run(&pen, begin);

    if (pen.flags) printf("\n"); // end of debug trace

    if (result) {
        Peg* new_peg = newPeg(input, pen.results[0], peg, NULL);
        if (pen.pos == pen.end) { // OK ...
            while (pen.multi) {
                multi_transform(&pen, pen.results[0]);
            }
            return new_peg;
        }
        new_peg->err = newErr(PEG_FELL_SHORT, pen.pos > pen.fail? pen.pos : pen.fail);
        return new_peg;
    }

    Err* err = newErr(PEG_FAILED, pen.pos > pen.fail? pen.pos : pen.fail);
    err->fail_rule = pen.fail_rule;
    err->expected = pen.expected;
    Peg* bad_peg = newPeg(input, pen.results[0], peg, err);

    return bad_peg;
}

// ==  API  ============================================

// returns a ptr to a parser for the grammar
extern Peg* peg_compile(char* grammar) {
    return peg_parser(BOOT, grammar, 0, strlen(grammar), 0);
}

// parse input string using peg parser..
extern Peg* peg_parse(Peg* peg, char* input) {
    return peg_parser(peg, input, 0, strlen(input), 0);
}

// parse a slice of input from start to end..
extern Peg* peg_parse_text(Peg* peg, char* input, int start, int end) {
    return peg_parser(peg, input, start, end, 0);
}

// display the parse tree or error report
extern void peg_print(Peg* peg) {
    if (peg->err) fault_report(peg);
    else print_ptree(peg);
}

// true if the `peg` encountered an error
extern bool peg_err(Peg* peg) {
    return peg->err? true : false;
}

// print out a trace of the parse...
extern Peg* peg_trace(Peg* peg, char* input) {
    return peg_parser(peg, input, 0, strlen(input), 2);
}

// print out a low level trace of the parser instructions..
extern Peg* peg_debug(Peg* peg, char* input) {
    return peg_parser(peg, input, 0, strlen(input), 1);
}

// Peg* access ..........


// returns ptr to the parse tree root node..
extern Node* peg_tree(Peg* peg) {
    return peg->tree;
}

// copies node text span into `text` string arg, with a 0 end,
// limit of max length, truncated if necessary.
extern void peg_text(Peg* peg, Node* node, char* text, int max) {
    int len = node->end - node->start;
    int limit = len < max? len : max;
    char* ptr = peg->src + node->start;
    memcpy(text, ptr, limit);
    *(text+limit) = '\0';    
}

// copies node rule name into `name` string arg, with a 0 end,
// limit of max length, truncated if necessary.
extern void peg_name(Peg* peg, Node* node, char* name, int max) {
    Node* id = peg->peg->tree->nodes[node->tag]->nodes[0];
    char* ptr = peg->peg->src + id->start;
    int len = id->end - id->start;
    int limit = len < max? len : max;
    memcpy(name, ptr, limit);
    *(name+limit) = '\0';
}

// Node* access ........


// returns index of rule name
extern int peg_tag(Node* node) {
    return node->tag;
}
// returns count of children nodes
extern int peg_count(Node* node) {
    return node->count;
}
// returns ptr to the ith child node
extern Node* peg_nodes(Node* node, int i) {
    return node->nodes[i];
}
