/* main.cc - entry point for the compiler
 *
 * @author: Cade Brown <cade@utk.edu>
 */

#include <mycc.h>
#include <gram.h>

extern void yyerror (const char *msg);
extern int yyparse();
extern int yylex();

int main(int argc, char** argv) {


    /* Initialize LLVM */
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    /* Initialize IR generator */
    init_gen();

    if (yyparse()) {
        /* Syntax Error While Parsing */
        yyerror("syntax error");
    } else {
        /* Successfully Parsed */
        fini_gen();

        TheModule.print(outs(), NULL);
        outs().write('\n');

        #ifdef USE_ORC
        printf("JIT output:\n");
        return run_jit(argc, argv);
        #endif
    }
    return 0;
}

