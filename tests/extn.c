#include "test-kit.c"

// experimental...
// - should multi rule syntax use X::Y rather than X:Y
// - or is this premature, stick to: <and X Y> (and <eq rule>)

int main(void) {
    printf("Test pPEG same @x and X:Y multi ...\n");

    char* s = 
    "s       = tag content @tag   \n"
    "content = ~@tag*             \n"
    "tag     = '#'+               \n";
    
    test_show(s, "###abcdef###");

    char* p = 
    "P = '0' M:P '0' / '1' M:P '1' / [01]?   \n"
    "M = ([01] &[01])+                       \n";
    
    test_ok(p, "010");
    test_ok(p, "01010");
    test_ok(p, "010101010");
    test_ok(p, "000010000");
    test_show(p, "11001110011");
    test_ok(p, "1100010011");

    char* pn = 
    "P = x M:P @x / x?    \n"
    "M = (x &x)+          \n"
    "x = [a-z]            \n";

    test_show(pn, "racecar");
    test_show(pn, "xxxxxxx");

}

