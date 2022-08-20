
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#define MAX_STACK 255

// -- peg grammar --------------------------------

// TODO update grammar..... seq, rop, group, dq, ...

char* peg_grammar = 
"    Peg   = _ (rule _)+                          \n"
"    rule  = id _ '=' _ alt                       \n"
"                                                 \n"
"    alt   = seq ('/' _ seq)*                     \n"
"    seq   = rep _ (rep _)*                       \n"
"    rep   = pre sfx?                             \n"
"    pre   = pfx? term                            \n"
"    term  = call / quote / chars / group / extn  \n"
"    group = '(' _ alt _ ')'                      \n"
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
// corresponding peg rule names. i.e AST tags == op-codes.

// BOOT calls boot_code() which must agree with these op codes. 

enum op {
    PEG, RULE,
    ALT, SEQ, REP, PRE, TERM, GROUP,
    CALL, AT, MULTI,
    ID, PFX, SFX, RANGE, NUM,
    QUOTE, SQ, CHARS, CHS, EXTN, WS
};

// parser machine instruction op-codes:
// ALT, SEQ, REP, PRE, ID, SQ, CHS
// plus from resolving CALL ...
// AT, MULTI

// -- parse tree node: slot for application data ------

typedef union {
    struct { // opx used for parser op code data...
        char idx;       // max 256 grammar rules
        char min;       // repeat
        char max;       // max 256 numeric repeat
        char sign;      // ~ ! &
        char is_multi;  // bool flag multi useage
        char multi;     // B idx for A::B (idx may be 0)
        char c1,c2;     // reseve 8 bytes   
    } opx;
    struct { // or HEAP_DATA pointer for application data
        void* p;
    } ptr;
} Slot;

// -- parse tree node ------------------------------

typedef struct Node Node;

enum DATA_USE { NO_DATA, DATA_VALS, HEAP_DATA };

struct Node {
    char tag;      // rule name index
    char data_use; // enum DATE_USE
    int start;     // start string slice
    int end;       // last+1 string slice
    int count;     // nodes count
    Slot data;     // application data -- op codes
    Node* nodes[]; // chidren node pointers
};

static Node *newNode(int tag, int i, int j, int n) {
    Node *nd = (Node *)malloc(sizeof(Node) + n*sizeof(Node *));
    if (nd == NULL) abort(); // TODO or exit()? or...?
    nd->tag = tag;
    nd->data_use = NO_DATA;
    nd->start = i;
    nd->end = j;
    nd->count = n; // number of children nodes
    return nd;
};

static void drop(Node* node) {
    if (node->data_use == HEAP_DATA) {
        free(node->data.ptr.p);
    }
    for (int i=0; i<node->count; i+=1) {
        drop(node->nodes[i]);
    }
    free(node);
}

enum err { PEG_OK, PEG_FELL_SHORT, PEG_FAILED };

char* peg_err_msg[] = {
    "ok",
    "Parse fell short",
    "Parse failed"
};

typedef struct Err Err;

struct Err {
    int err; 
    int fail_rule;
    int pos;
    Node* expected;
};

Err* newErr(int code, int pos) {
    Err* err = malloc(sizeof(Err));
    if (!err) abort(); // TODO what should this do, exit()?
    err->err = code;
    err->pos = pos;
    return err;
}

// == Peg AST ===================================================

typedef struct Peg Peg;

struct Peg { // parse tree.........
    char* src;   // input string
    Node* tree;  // parse tree indexes into src string
    Peg* peg;    // grammar parse tree
    Err* err;    // error info
};

// == parser machine ========================================

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


void debug_trace(Env *pen, Node *exp);
void rule_trace_open(Env *pen, int tag);
void rule_trace_close(Env *pen, int tag, bool result);
void resolve_id(Env* pen, Node* exp);
void resolve_rep(Env* pen, Node* exp);
void resolve_pre(Env* pen, Node* exp);
void print_line_num(char* p, int pos);
void print_text(char* str, Node* node);

/* 
    UTF-8
    0-7F            0xxx xxxx
    80-7FF          110x xxxx 10xx xxxx
    800-7FFF        1110 xxxx 10xx xxxx 10xx xxxx
    10000-10FFFF    1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx
*/

int utf8_size(int x) {
    if (x < 128) return 1;
    if (x < 0x800) return 2;
    if (x < 0x10000) return 3;
    return 4;
}

int utf8_len(char* p) {
    unsigned int c = (unsigned char)p[0];
    if (c < 127) return 1;            
    int x = c<<1, i = 2;
    while ((x <<= 1) & 0x80) i++;
    return i;
}

int utf8(char* p) {
    unsigned int c = (unsigned char)p[0];
    if (c < 127) return c;            
    int x = c<<1, i = 2;
    while ((x <<= 1) & 0x80) i++;
    x = (x & 0xFF) >> i;
    for (int n=1; n<i; n++) x = (x<<6)+((unsigned char)p[n] & 0x3F);
    return x;
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

// == parser machine engine ==============================

bool run(Env *pen, Node *exp) {
    if (pen->flags == 1) debug_trace(pen, exp);
    switch (exp->tag) {
    case ID: {
        if (exp->data_use == NO_DATA) resolve_id(pen, exp);
        int tag = exp->data.opx.idx;
        int start = pen->pos;
        int stack = pen->stack;
        Node* rule = pen->tree->nodes[tag]->nodes[1];
        if (pen->flags == 2) rule_trace_open(pen, tag);
        if (pen->depth++ > MAX_STACK) {
            printf(".... call reciursion > MAX_STACK ....");
            abort(); // TODO exit? better error msg...
        }
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
            if (pen->stack >= MAX_STACK) {
                printf("MAX_STACK %d exceeded...\n", MAX_STACK);
                abort(); // TODO exit? elastic results stack?
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
        if (sign == '~') {
            if (result) return false;
            if (pos < pen->end) {
                pen->pos = pos+1; // UTF-8 continues ...
                while ((pen->input[pen->pos] & 0xC0) == 0x80) pen->pos++;
                return true;
            }
            return false;
        }
        pen->pos = pos; // reset
        if (pen->stack > stack) {
            for (int i=stack; i<stack; i++) {
                drop(pen->results[i]);
            }
            pen->stack = stack;
        }
        if (sign == '!') return !result;
        return result;
    }
    case SQ: { 
        if (pen->pos+exp->end-exp->start > pen->end) return false;
        // TODO if raw && !resolved raw -> escaped string....
        if (pen->grammar[exp->end+1] == 'i') { // 'xyz'i ASCII only..
            for (int i=exp->start; i<exp->end; i+=1) {
                unsigned char c1 = pen->input[pen->pos];
                if (c1 >= 'a' && c1 <= 'z') c1 = c1-32;
                unsigned char c2 = pen->grammar[i]; // TODO compile time..
                if (c2 >= 'a' && c2 <= 'z') c2 = c2-32;
                if (c1 != c2) return false;
                pen->pos += 1;
            }
            return true;
        }
        for (int i=exp->start; i<exp->end; i+=1) {
            if (pen->input[pen->pos] != pen->grammar[i]) return false;
            pen->pos += 1; // any bytes, so UTF-8 ok
        }
        return true;
    }
    case CHS: {
        // TODO if raw && !resolved ....
        if (pen->pos >= pen->end) return false;
        int c = pen->input[pen->pos];
        if (c > 127 || c < 0) c = utf8(pen->input);
        int x_size = 1;
        for (int i=exp->start; i<exp->end; i+=x_size) {
            int x = pen->grammar[i]; // TODO raw new escaped string
            x_size = 1;
            if (x > 127 || x < 0) {
                x = utf8(pen->grammar+i); // TODO
                x_size = utf8_size(x);
            }
            if (i+x_size+1 < exp->end && pen->grammar[i+x_size] == '-') {
                int y = pen->grammar[i+x_size+1]; // TODO
                int y_size = 1;
                if (y > 127 || y < 0) {
                    y = utf8(pen->grammar+i); // TODO
                    y_size =utf8_size(y);
                }
                i += 1+y_size; // skip range: "x-y"
                if (c < x) continue;
                if (c > y) continue;
                pen->pos += x_size;
                return true;
            } else if (x == c) {
                pen->pos += x_size;
                return true;
            }
        }
        return false;
    }
    case CALL: { // at? id (":" id)?
        if (exp->count != 2) { // both operators:  @X:Y
            printf("*** Not implemented: "); // TODO improve err reporting
            print_text(pen->grammar, exp);
            printf("\n");
            return false;
        }
        if (exp->nodes[0]->tag == AT) { // at id
            Node* id = exp->nodes[1];
            if (id->data_use == NO_DATA) resolve_id(pen, id);
            int tag = id->data.opx.idx;
            int k = pen->stack;
            while (k-- > 0) {
                Node* node = pen->results[k];
                if (node->tag == tag) {             
                    int start = pen->pos;
                    int pos = start;
                    for (int i=node->start; i<node->end; i++) {
                        if (pen->input[i] != pen->input[pos++]) {
                            return false;
                        }
                    }
                    Node* nd = newNode(tag, start, pos, 0);
                    pen->results[pen->stack++] = nd;
                    pen->pos = pos; // TODO + parse tree nodes??
                    return true;
                }
            }
            return true; // no prior sibling, match ''
        }
        if (exp->nodes[1]->tag == ID) { // multi id1 : id2
            // int start = pen->pos;
            int stack = pen->stack;
            Node* id1 = exp->nodes[0];
            if (id1->data_use == NO_DATA) resolve_id(pen, id1);
            Node* id2 = exp->nodes[1];
            if (id2->data_use == NO_DATA) resolve_id(pen, id2);
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
    }
    case EXTN: {
        int key = 0; // TODO assumes 32 bits, 4 char max key
        for (int i=exp->start+1; i<exp->start+4; i++) {
            unsigned char c = pen->grammar[i];
            if (c == ' ' || c == '>') break;
            key = (key<<8)+c;
        } // TODO compile time key, extern linkage?
        switch (key) {
            case 0: return false; // <>
            case 0x00666F6F: { // foo
                printf("foo....");
                return true;
            }
            default: { // TODO better err reporting...
                printf("**** Undefined extn: ");
                print_text(pen->grammar, exp);
                printf("\n");
                return false;
            }
        }
        return false;
    }
    default:
        printf("woops: undefined op: %d\n", exp->tag);
        abort();
    }
} // run

// -- node utils ---------------------------------

char* node_text(char* str, Node* nd, char* out, int len) {
    if (len < 1) return out;
    int span = nd->end - nd->start; // text len
    int mid = len >> 1;
    int end = nd->end;
    int skip = end; // don't skip ... truncate to len if necessary
    int rest = end; // not used unless skip ...
    if (len < span) { // trucate or skip ...
        if (len > 11) { // skip ...
            skip = nd->start + mid - 3; // " ... " in approx middle
            rest = nd->end - mid + 2 ;  // " ... " 5 chars
        } else { // truncate
            end = nd->start+len;
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

void print_text(char* str, Node* node) {
    char line[80];
    char* p = &line[0];
    p = node_text(str, node, p, 79);
    *p = '\0';
    printf("%s", line);
}

// -- resolve opx data slot extensions -------------------

void resolve_id(Env* pen, Node* exp) {
    char name[50];
    char* p = &name[0];
    p = node_text(pen->grammar, exp, p, 49);
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
    printf("*** Undefined rule: %s\n", name);
    abort();
    return;
}

unsigned char numeric(char* src, int start, int end) {
    int num = 0;        
    for (int i = start; i < end; i++) {
        num = num*10 + src[i]-'0';
    }
    if (num > 255) {
        printf("Repeat * %d is too big, limit 255\n", num);
        abort();
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
        min = numeric(pen->grammar, sfx->start, sfx->end);
        max = min;
    } else if (sfx->tag == RANGE) {
        Node* num1 = sfx->nodes[0];
        min = numeric(pen->grammar, num1->start, num1->end);
        Node* num2 = sfx->nodes[1];
        max = numeric(pen->grammar, num2->start, num2->end);
    } else abort();
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

// char* escape(char* str, int start, int end, char* out) {
//     for (int i=start; i<end; i+=1) {
//         char c = str[i];
//         if (c == '\\') {
//             char x = str[++i];
//             if (x == 'u') {
//                 code = utf8_endcode(str, out);
//                 i += ut8_size(code)-1;
//                 continue;
//             }
//             if (x == 'n') c = '\n';
//             if (x == 'r') c = '\r';
//             if (x == 't') c = '\t';
//         }
//         *out++ = c;
//     }
//     return out;
// }

// -- debug trace op codes display ---------------------------------

char* node_quote(char* str, Node* nd, char* out, int len) {
    *out++ = '"';
    out = node_text(str, nd, out, len-2);
    *out++ = '"';
    return out;
}    

void show_exp(Env *pen, Node *exp, char* out, int len) {
    int tag = exp->tag;
    char *name = peg_names[tag];
    int n = strlen(name);
    if (n+2 < len) abort();
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

// -- fault reporting ----------------------------------------

void print_cursor(char* p, int pos) {
    int i = pos;
    while ((unsigned char)p[i] >= ' ' && i > 0) i--;
    if (i>0) i++; // after line break
    int j = pos;
    while ((unsigned char)p[j] >= ' ') j++; // \n or \0
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
    // while (i > 0 && p[i] != '\n' && p[i] != '\r') i--;
    int col = pos-i;
    do {
        if (p[i] == '\n') lf++;
        if (p[i] == '\r') cr++;
    } while (i-- > 0);
    int ln = lf > cr? lf : cr;
    printf("%d.%d", ln+1, col+1);
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
    printf("Boot: '%s' not found in peg grammar...\n", str);
    abort();
}

int rule_index(const char *name) { // index of name in peg_names
    int n = sizeof(peg_names)/sizeof(char *); // rule_count;
    for (int i=0; i<n; i+=1) {
        if (strcmp(peg_names[i], name) == 0) return i;
    }
    printf("Boot: '%s' is not a peg grammar rule name... \n", name);
    abort();
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
    return ops(PEG, 23,  // 20 before CALL, AT, MULTI
    //   Peg   = _ (rule _)+
    ops(RULE, 2, op(ID, "Peg"),
        ops(SEQ, 2, op(ID, "_"),
            opREP(ops(SEQ, 2, op(ID, "rule"),
                        op(ID, "_")), "+"))),               
    //   rule  = id _ '=' _ alt
    ops(RULE, 2, op(ID, "rule"),
        ops(SEQ, 5, op(ID, "id"), op(ID, "_"), op(SQ, "="),
                op(ID, "_"), op(ID, "alt"))),           

    //   alt   = seq (_ '/' _ seq)*
    // ops(RULE, 2, op(ID, "alt"),
    //     ops(SEQ, 2, op(ID, "seq"),
    //         opREP(ops(SEQ, 4, op(ID, "_"), op(SQ, "/"), op(ID, "_"),
    //                 op(ID, "seq")), "*"))),
    //  alt   = seq ('/' _ seq)*
    ops(RULE, 2, op(ID, "alt"),
        ops(SEQ, 2, op(ID, "seq"),
            opREP(ops(SEQ, 3, op(SQ, "/"), op(ID, "_"),
                    op(ID, "seq")), "*"))),        
    //   seq   = rep (' '+ rep)*            
    // ops(RULE, 2, op(ID, "seq"),
    //     ops(SEQ, 2, op(ID, "rep"),
    //         opREP(ops(SEQ, 2, opREP(op(SQ, " "), "+"),
    //                         op(ID, "rep")), "*"))),        
    //  seq   = rep _ (rep _)*
    ops(RULE, 2, op(ID, "seq"),
        ops(SEQ, 3, op(ID, "rep"), op(ID, "_"),
            opREP(ops(SEQ, 2, op(ID, "rep"), op(ID, "_")), "*"))),
    //   rep   = pre sfx?
    ops(RULE, 2, op(ID, "rep"),
        ops(SEQ, 2, op(ID, "pre"),
            opREP(op(ID, "sfx"), "?"))),                   
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

    //   call  = at? id multi? _ !'='
    ops(RULE, 2, op(ID, "call"),
        ops(SEQ, 5, opREP(op(ID, "at"), "?"),
            op(ID, "id"), opREP(op(ID, "multi"), "?"),
            op(ID, "_"), opPRE("!", op(SQ, "=")))),
    //   at    = '@'
    ops(RULE, 2, op(ID, "at"), op(SQ, "@")),                      
    //   mulit = ':' id
    ops(RULE, 2, op(ID, "multi"),
        ops(SEQ, 2, op(SQ, ":"), op(ID, "id"))),                      

    //   id    = [a-zA-Z_] [a-zA-Z0-9_]*
    ops(RULE, 2, op(ID, "id"),
        ops(SEQ, 2, op(CHS, "a-zA-Z_"), 
            opREP(op(CHS, "a-zA-Z0-9_"), "*"))),

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
    //   extn = '<' ~'>'* '>'
    ops(RULE, 2, op(ID, "extn"),
        ops(SEQ, 3, op(SQ, "<"),
            opREP(opPRE("~", op(SQ, ">")), "*"),
            op(SQ, ">"))),                   
    //   _     = ([ \r\n\t]+ / '#' ~[\n\r]*)*
    ops(RULE, 2, op(ID, "_"),
        opREP(ops(ALT, 2, opREP(op(CHS, " \n\r\t"), "+"),
            ops(SEQ, 2, op(SQ, "#"),
                opREP(opPRE("~", op(CHS, "\n\r")), "*"))), "*"))              
    );
}

// -- AST print display ---------------------------

void print_tag(Peg* ast, int tag) {
    if (!ast->peg) { // boot grammar...
        printf("%s", peg_names[tag]);
        return;
    }
    char* src = ast->peg->src;
    Node* peg = ast->peg->tree;
    Node* nd = peg->nodes[tag]->nodes[0]; // ID i..j
    print_text(src, nd);
}

void print_tree(Peg* ast, Node* nd, int inset, int last) {
    for (int j=0; j<inset; j++) {
        if (j == inset-1) { // max inset ...
            if (last&(0x1<<inset)) printf("%s", "\u2514\u2500");  // `-
                else printf("%s", "\u251C\u2500");   // |-
        }
        else if (last&(0x1<<(j+1))) printf("%s", "  ");
            else printf("%s", "\u2502 ");  // |
    }
    if (!nd) { printf("NULL\n"); return; }
    if (nd->count > 0) {
        print_tag(ast, nd->tag);
        printf("\n");
        for (int i=0; i<nd->count; i++) {
            if (i == nd->count-1) last = last|(0x1<<(inset+1));
            print_tree(ast, nd->nodes[i], inset+1, last);
        };
        return;
    }
    // leaf node...
    print_tag(ast, nd->tag);
    char txt[50];
    char* t = &txt[0];
    t = node_quote(ast->src, nd, t, 49);
    *t = '\0';       
    printf(" %s\n", txt);
}

void print_ast(Peg* ast) {
    if (ast && ast->tree) {
        print_tree(ast, ast->tree, 0, 0);
    } else {
        printf("Empty AST...\n");
    }
}

// -- fault report -----------------------------------------

void fault_report(Peg* ast) {
    printf("%s in rule: ", peg_err_msg[ast->err->err]);
    print_tag(ast, ast->err->fail_rule);
    printf("\n");
    if (ast->err->expected) {
        char open = ' ';
        char close = ' ';
        if (ast->err->expected->tag == SQ) { open = '\''; close = '\''; }
        if (ast->err->expected->tag == CHS) { open = '['; close = ']'; }
        printf("expected: %c", open);
        print_text(ast->peg->src, ast->err->expected);
        printf("%c\n", close);
    }
    printf("on line: ");
    print_line_num(ast->src, ast->err->pos);
    printf(" at: %d of %lu\n", ast->err->pos, strlen(ast->src));
    print_cursor(ast->src, ast->err->pos);
} 

// -- Transform ---------------------------------------------

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

// == Bootstrap ======================================

Peg* BOOT = NULL;

// BOOT = { peg_grammar, boot_code(), NULL};
void bootstrap() {
    Peg* peg = (Peg *)malloc(sizeof(Peg));
    if (!peg) abort();
    peg->src = peg_grammar;
    peg->tree = boot_code();
    peg->peg = NULL;
    peg->err = NULL;
    BOOT = peg;
}

// ==  Parser  ============================================

void peg_print(Peg* peg);

Peg peg_parser(Peg* peg, char* input, int start, int end, int flags) {
    if (!peg) { // peg_compile(BOOT, ...)    TODO flags mask for raw+trace...
        if (!BOOT) bootstrap();
        peg = BOOT;
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
        Peg ast = {input, pen.results[0], peg, NULL};
        if (pen.pos == pen.end) { // OK ...
            while (pen.multi) {
                multi_transform(&pen, pen.results[0]);
            }
            return ast;
        }
        ast.err = newErr(PEG_FELL_SHORT, pen.pos);
        return ast;
    }

    Err* err = newErr(PEG_FAILED, pen.pos > pen.fail? pen.pos : pen.fail);
    Peg ast = {input, NULL, peg, err};
    err->fail_rule = pen.fail_rule;
    err->expected = pen.expected;

    return ast;
}

// ==  API  ============================================

Peg peg_compile(char* grammar) {
    return peg_parser(BOOT, grammar, 0, strlen(grammar), 0);
}

Peg peg_compile_text(char* grammar, int start, int end) {
    return peg_parser(BOOT, grammar, start, end, 0);
}

Peg peg_compile_raw(char* grammar, int start, int end) {
    return peg_parser(BOOT, grammar, start, end, 0);
}

Peg peg_parse(Peg* peg, char* input) {
    return peg_parser(peg, input, 0, strlen(input), 0);
}

Peg peg_parse_text(Peg* peg, char* input, int start, int end) {
    return peg_parser(peg, input, start, end, 0);
}

void peg_print(Peg* peg) {
    if (peg->err) fault_report(peg);
    else print_ast(peg);
}

bool peg_err(Peg* peg) {
    return peg->err? true : false;
}

Peg peg_trace(Peg* peg, char* input) {
    return peg_parser(peg, input, 0, strlen(input), 2);
}

Peg peg_debug(Peg* peg, char* input) {
    return peg_parser(peg, input, 0, strlen(input), 1);
}
