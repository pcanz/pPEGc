#include "test-kit.c"

int main(void) {
    printf("Test pPEG UTF-8 ...\n");

    char* s0 = "s0 = 'x'  \n";
    test_ok(s0, "x");

    char* s1 = "s1 = '«»'  \n";
    test_ok(s1, "«»");
 
    char* s2 = "s2 = [x]\n";
    test_ok(s2, "x");

    char* s3 = "s3 = [«]\n";
    test_ok(s3, "«");

    char* s4 = "s4 = [«x»]+\n";
    test_ok(s4, "x«x»xx");

    // case-insensitive...

    char* s5 = "s5 = 'abc'i \n";
    test_ok(s5, "aBc");

    // implicit char rules....

    char* s6 = "s6 = _20 \n";
    test_ok(s6, " ");

    char* s7 = "s7 = _9-20  # comment..\n";
    test_ok(s7, "\n");

    char* s8 = "s8 = _WS \n";
    test_ok(s8, "\t");
    test_ok(s8, "\n");
    test_ok(s8, " ");

    char* s9 = "s9 = _";
    test_ok(s9, " ");
    test_ok(s9, "\n");
    test_ok(s9, "  \t\r\r\n   ");

    printf("OK, UTF-8 tests done...\n");
}

