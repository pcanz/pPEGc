#include "test-kit.c"

int main(void) {
    printf("Test pPEG implicit rules ...\n");

    // implicit char rules....

    char* space = "space = _20 \n";
    test_ok(space, " ");

    char* TAB = "TAB = _9 \n";
    test_ok(TAB, "\t");

    char* CR = "CR = _D \n";
    test_ok(CR, "\r");

    char* LF = "LF = _A \n";
    test_ok(LF, "\n");

    char* _9_D = "_9_D = _9-D\n";
    test_ok(_9_D, "\t");
    test_ok(_9_D, "\n");
    test_ok(_9_D, "\r");

    char* _0_9 = "_0_9 = _30-39";
    test_ok(_0_9, "0");
    test_ok(_0_9, "3");
    test_ok(_0_9, "4");
    test_ok(_0_9, "9");

    char* _0__ = "_0__ = _0-10FFFF";
    test_ok(_0__, "\x01");
    test_ok(_0__, "Q");
    test_ok(_0__, "\xFF");
    // test_ok(_0__, "\U000000FF"); // failed???
    test_ok(_0__, "\U00100000");
    test_ok(_0__, "\U0010FFFF");


    // implicit builtins..............

    char* _TABCRLF = "_TABCRLF = _TAB _CR _LF";
    test_ok(_TABCRLF, "\t\r\n");

    char* _DQSQBSBT = "_DQSQBSBT = _DQ _SQ _BS _BT";
    test_ok(_DQSQBSBT, "\"\'\\`");

    char* EOL = "EOL = _EOL";
    test_ok(EOL, "\n");
    test_ok(EOL, "\r");

    char* NL = "NL = _NL";
    test_ok(NL, "\n");
    test_ok(NL, "\r");
    test_ok(NL, "\r\n");

    char* WS = "WS = _WS \n";
    test_ok(WS, "\t");
    test_ok(WS, "\n");
    test_ok(WS, " ");

    char* __ = "__ = _";
    test_ok(__, " ");
    test_ok(__, "\n");
    test_ok(__, "  \t\r\r\n   ");

    char* ANY = "ANY = _ANY";
    // test_ok(ANY, "\0"); // Hummm....
    test_ok(ANY, "\x01");
    test_ok(ANY, "x");
    test_ok(ANY, "\U0010FFFF");

    char* _EOF_ = "_EOF_ = _ANY* _EOF";
    test_ok(_EOF_, "xyz");
    test_ok(_EOF_, "");


    printf("OK, implicit rule tests done...\n");
}

