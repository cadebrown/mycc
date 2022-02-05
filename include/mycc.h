/* mycc.h - custom C compiler implementation, for
 *
 * 
 * @author: Cade Brown <cade@utk.edu>
 */

#pragma once
#ifndef MYCC_H__
#define MYCC_H__

/* If defined, use ORC for JIT compilation */
#define USE_ORC

/* C std */
#include <stdio.h>
#include <stdlib.h>

/* C++ std */
#include <string>
#include <unordered_map>

/* LLVM API */
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/APFloat.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/PassManager.h"

//#include "llvm/Passes/PassBuilder.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/Analysis/CallGraphSCCPass.h"

#include "llvm/Target/TargetMachine.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"


/* LLVM ORC API */
#ifdef USE_ORC

#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/JITSymbol.h"

#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"

using namespace llvm::orc;

#endif

using namespace llvm;
using namespace std;

/* Scope - 
 */
struct Scope {
    /* Local Variables, which are always */
    unordered_map<string, AllocaInst*> vars;

};

/* LoopCFs - Loop Control Flows
 *
 * Holds a list of 'break' and 'continue' statements
 *   for a a loop scope. There is a stack of these in 'sem.cc',
 *   and 'for','while', and 'do...while' push on an empty one,
 *   and when they are finished, pop it off and back patch them
 * 
 */
struct LoopCFs {

    vector<BasicBlock*> cf_breaks;
    vector<BasicBlock*> cf_continues;

};


/* Global Variables (see 'sem.cc' for descriptions) */
extern unique_ptr<LLVMContext> _TheContext;
#define TheContext (*_TheContext)
extern unique_ptr<Module> _TheModule;
#define TheModule (*_TheModule)

extern IRBuilder<> Builder;
extern unordered_map<string, Type*> types;
extern unordered_map<string, Function*> funcs;

/* Get a constant integer value */
Value* const_int(int val);
Value* const_int(const char* valstr);

/* Get a constant floating-point value */
Value* const_float(double val);
Value* const_float(const char* valstr);

/* Get a constant string value */
Value* const_str(string val);
Value* const_str(const char* val);

/* Get a constant string value, which is a literal (i.e. begins with '"', has not escaped itself) */
Value* const_strlit(string val);
Value* const_strlit(const char* val);


/* Initialize IR generation */
void init_gen();

/* Finalize IR generation */
void fini_gen();

#ifdef USE_ORC

/* Run JIT main */
int run_jit(int argc, char** argv);

#endif

/* Signal an error */
extern void yyerror (const char *msg);



/* Utilities */
BasicBlock* newbb(const string& name="bb");
AllocaInst* decl(string name, Type* tp, Value* initial=NULL);
void doglobal(string name, Type* tp);
Function* newfunc(string name, vector<Type*> types, vector<string> names, Type* rtype, bool is_extern=false, bool is_vararg=false);
void endfunc();

void newloop();
void endloop(BasicBlock* break_to, BasicBlock* continue_to);

/* Expression/Value Routines */
Value* dovar(string name, bool do_load=true);
Value* doset(string op, Value* aL, Value* R);
Value* dobop(string op, Value* L, Value* R);
Value* douop(string op, Value* L);
Value* docond(Value* L);
Value* docast(Value* L, Type* to);
Value* docall(Value* L, vector<Value*> args);
Value* doindex(Value* L, Value* idx, bool do_load=true);
PHINode* docor(BasicBlock* bbL, Value* L, BasicBlock* bbR, Value* R);
PHINode* docand(BasicBlock* bbL, Value* L, BasicBlock* bbR, Value* R);

/* Statement/ControlFlow Routines */
void dobr(BasicBlock* to);
void docondbr(Value* cond, BasicBlock* bb_T, BasicBlock* bb_F);
void dobreak();
void docontinue();
void dogoto(string name);
BasicBlock* dolabel(string name);
void doret(Value* L);
void doif(BasicBlock* before, Value* cond, BasicBlock* bb_T, BasicBlock* bb_Tend, BasicBlock* bb_F, BasicBlock* bb_Fend, BasicBlock* after);
void dowhile(BasicBlock* before, BasicBlock* bb_cond, Value* cond, BasicBlock* body, BasicBlock* body_end, BasicBlock* after);
void dodo(BasicBlock* before, BasicBlock* body, BasicBlock* bb_cond, Value* cond, BasicBlock* after);
void dofor(BasicBlock* before, BasicBlock* bb_cond, Value* cond, BasicBlock* bb_inc, BasicBlock* body, BasicBlock* body_end, BasicBlock* after);


#endif /* MYCC_H__ */

