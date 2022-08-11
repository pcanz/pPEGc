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

    char* s5 = "s5 = 'abc'i \n";
    test_ok(s5, "aBc");

    printf("OK, UTF-8 tests done...\n");
}

