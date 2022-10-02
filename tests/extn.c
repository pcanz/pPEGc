#include "test-kit.c"

int main(void) {
    printf("Test pPEG <id x> or @x and <do x y> or X -> Y multi ...\n");

    char* s = 
    "str = tag raw tag  \n"
    "raw = ~<eq tag>*   \n"
    "tag = '#'+         \n";
     
    test_show(s, "###abc##def###");

    char* s1 = 
    "str  = tag raw tag  \n"
    "raw  = ~@tag*       \n"
    "tag  = '#'+         \n";
    
    test_show(s1, "###abc##def###");

    char* s2 = 
    "str  = tag raw tag  \n"
    "raw  = ~<ge tag>*   \n"
    "tag  = '#'+         \n";
    
    test_show(s2, "###abc##def#####");

    // palindromes ==========================================

    char* p = 
    "P = '0' <do M P> '0' / '1' <do M P> '1' / [01]?   \n"
    "M = ([01] &[01])+                                 \n";
    
    test_ok(p, "010");
    test_ok(p, "01010");
    test_ok(p, "010101010");
    test_ok(p, "000010000");
    test_show(p, "11001110011");
    test_ok(p, "1100010011");

    char* p1 = 
    "P = '0' M -> P '0' / '1' M -> P '1' / [01]?   \n"
    "M = ([01] &[01])+                             \n";
    
    test_ok(p1, "010");
    test_ok(p1, "01010");
    test_ok(p1, "010101010");
    test_ok(p1, "000010000");
    test_show(p1, "11001110011");
    test_ok(p1, "1100010011");

    char* pn = 
    "P = x <do M P> <id x> / x?   \n"
    "M = (x &x)+                  \n"
    "x = [a-z]                    \n";

    test_show(pn, "racecar");
    test_show(pn, "xxxxxxx");

    char* pn1 = 
    "P = x M -> P @x / x?  \n"
    "M = (x &x)+           \n"
    "x = [a-z]             \n";

    test_show(pn1, "racecar");
    test_show(pn1, "xxxxxxx");

}

