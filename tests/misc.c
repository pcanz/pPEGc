#include "test-kit.c"

int main(void) {
    printf("Test pPEG misc ...\n");

    char* s1 = "s1 = 'foo' !'=' 'bar'  \n";
    test_ok(s1, "foobar");
    // test_ok(s1, "foo=bar");

    char* date = 
    "date  = year '-' month '-' day   \n"
    "year  = [0-9]*4                  \n"
    "month = [0-9]*2                  \n"
    "day   = [0-9]*2                  \n";
    test_trace(date, "2022-03-04");

    printf("OK, misc tests done...\n");
}

