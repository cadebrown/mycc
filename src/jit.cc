/* jit.cc - run 'TheModule' as a JIT-compiled source
 *
 * @author: Cade Brown <cade@cade.site>
 */
#include <mycc.h>

#ifdef USE_ORC


/* JIT Compiler */
struct OrcJIT {
    
    /* Internal JIT from ORC API */
    unique_ptr<LLJIT> TheLLJIT;

    /* Arguments to main() */
    int argc;
    char** argv;

    /* Result code */
    int result;

};

/* Entry point for a JIT'd module */
typedef int (*jitmain_f)(int argc, char** argv);


int run_jit(int argc, char** argv) {

    /* Create a JIT */
    auto req = LLJITBuilder().create();
    if (auto err = req.takeError()) logAllUnhandledErrors(move(err), errs(), "[JIT] ");
    unique_ptr<LLJIT> TheJIT = move(req.get());

    /* Set the data layout, which is important. Normally, this is done at a lower level.
     * But, since we are doing the compilation now, we need to have exact data layout for a target machine 
     */
    TheModule.setDataLayout(TheJIT->getDataLayout());
    auto& JD = TheJIT->getMainJITDylib();

    /* Define exported symbols from libc that we have */
    MangleAndInterner Mangle(TheJIT->getExecutionSession(), TheJIT->getDataLayout());

    /* Load current process permanently */
    sys::DynamicLibrary::LoadLibraryPermanently(NULL);

    /* Resolve external functions */
    for (auto func : funcs) {
        if (func.second->getBasicBlockList().size() == 0) {
            uintptr_t addr = SectionMemoryManager::getSymbolAddressInProcess(func.first);
            JD.define(absoluteSymbols({
                {Mangle(func.first), JITEvaluatedSymbol(addr, JITSymbolFlags::FlagNames::Absolute)},
            }));
        }
    }

    /* Add the IR we generated (i.e. the functions and globals) */
    ThreadSafeContext TSC = ThreadSafeContext(move(_TheContext));
    TheJIT->addIRModule(ThreadSafeModule(move(_TheModule), TSC));

    /* Look up the entry function */
    auto sym_req = TheJIT->lookup("main");
    if (auto err = sym_req.takeError()) logAllUnhandledErrors(move(err), errs(), "[JIT] ");

    /* Cast and call as a function */
    auto sym = sym_req.get();
    JITTargetAddress x = sym.getAddress();

    return (*(jitmain_f*)&x)(argc, argv);
}

#endif

