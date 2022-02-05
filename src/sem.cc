/* sem.cc - Code generation routines
 *
 * This file should be filled out by students
 *
 *
 * @author: Cade Brown <cade@utk.edu>
 */
#include <mycc.h>

/* TheContext: This is needed for all LLVM APIs, think of it like a catch-all
*   of informations and settings. We could have multiple if we needed multiple
*   projects in one binary. But, we just need a global because we are compiling
*   a single file for a single target.
*/
unique_ptr<LLVMContext> _TheContext = make_unique<LLVMContext>();

/* TheModule: contains the functions and globals we are generating */
unique_ptr<Module> _TheModule = make_unique<Module>("<stdin>", TheContext);
/* Types defines (NOTE: pointers are not included here), 'name -> LLVM Type' */
unordered_map<string, Type*> types;
/* Functions defined (NOTE: may be extern, like printf), 'name -> LLVM Function' */
unordered_map<string, Function*> funcs;

/* Utility class to emit instructions, makes generating IR easier */
IRBuilder<> Builder(TheContext);

/* Array of global variables */
static unordered_map<string, GlobalVariable*> globals;

/* Current stack of scopes within the function */
vector<Scope> scopes;

/* Current stack of loop control flows */
vector<LoopCFs> cfs;

/* Holds 'name -> block' of the labels defined within CurrentFunction */
static unordered_map<string, BasicBlock*> labels;

/* Holds 'block_from -> name' of gotos */
static unordered_map<BasicBlock*, string> gotos;

static Function* CurrentFunction = NULL;

/* Integer literal of '0' */
static Value *v0 = NULL;


void init_gen() {
    /* define standard types */
    types["char"] = Type::getInt8Ty(TheContext);
    types["int"] = Type::getInt32Ty(TheContext);
    types["long"] = Type::getInt64Ty(TheContext);
    types["size_t"] = Type::getInt64Ty(TheContext);
    types["double"] = Type::getDoubleTy(TheContext);
    types["void"] = types["char"];

    /* builtin functions */
    newfunc("putchar", { types["int"] }, { "c" }, types["int"], true, false);
    newfunc("puts",    { types["char"]->getPointerTo() }, { "msg" }, types["int"], true, false);
    newfunc("printf",  { types["char"]->getPointerTo() }, { "fmt" }, types["int"], true, true);

    /* initialize constants */
    v0 = ConstantInt::get(types["int"], 0);
}

void fini_gen() {
    /* LLVM devs being LLVM devs, of course there is no documentation or examples on
     *   something so core to the infrastructure like "How do I run optimizations?"
     * The official tutorials/examples all say to just use legacy:: everywhere, so that's
     *   what we do
     */

    /* Verify construction, or print to 'errs' */
    verifyModule(TheModule, &errs());

    /* Optimize IR 
     *
     * Don't uncomment this until your code is working -- if you generate incorrect code
     *   it might actually throw away your code! (if it is dead, for example) or
     *   change the instructions and make it harder to debug, so don't start with
     *   the optimizer
     */

    /*
    legacy::PassManager pm;
    pm.add(createInstructionCombiningPass());
    pm.add(createReassociatePass());
    pm.add(createGVNPass());
    pm.add(createCFGSimplificationPass());
    pm.add(createConstantPropagationPass());
    pm.add(createLoopUnrollAndJamPass());
    pm.add(createFunctionInliningPass());

    for (Function& func : TheModule.functions()) {
        //func.addAttribute(0, Attribute::AlwaysInline);
    }

    int ct = 0;
    while (ct < 6 && pm.run(TheModule)) ct++;

    */

}


/** Student Section **/

/*** Utility Macros ***/

/* Determine kind of type a given llvm::Value* is */
#define IS_BOOL(_val) (IS_INT(_val) && ((IntegerType*)(_val)->getType())->getBitWidth() == 1)
#define IS_INT(_val) ((_val)->getType()->isIntegerTy())
#define IS_PTR(_val) ((_val)->getType()->isPointerTy())
#define IS_FLT(_val) ((_val)->getType()->isFloatingPointTy())

/* Return references to constant integers */
Value* const_int(int val) {
    return ConstantInt::get(types["int"], val);
}
Value* const_int(const char* valstr) {
    return ConstantInt::get(types["int"], stoi((string)valstr));
}

/* Get a constant floating-point value */
Value* const_float(double val) {
    return ConstantFP::get(types["double"], val);
}
Value* const_float(const char* valstr) {
    return ConstantFP::get(types["double"], valstr);
}

/* Get a constant string value 
 * NOTE: This does not escape the string, so directly passing in a token is no good --
 *   the string will have literal quotation marks at the beginning and end, and \n will 
 *   not be a newline. Use 'conststrlit' for those
 */
Value* const_str(string val) {
    return Builder.CreateGlobalStringPtr(val);
}
Value* const_str(const char* val) {
    return const_str((string)val);
}

/* Get a constant string value, which is a literal (i.e. begins with '"', has not escaped itself) */
Value* const_strlit(string val) {
    string b = "";
    /* Work with inner contents */
    for (size_t i = 1; i < val.size() - 1; ++i) {
        char c = val[i];
        if (c == '\\') {
            i++;
            c = val[i];
            /**/ if (c == 'n') b += '\n';
            else if (c == 't') b += '\t';
            else {
                yyerror("unknown string literal escape code");
            }

        } else {
            b += c;
        }
    }
    return const_str(b);
}
Value* const_strlit(const char* val) {
    return const_strlit((string)val);
}

/* Create a new llvm::BasicBlock, and set it as the insertion point 
 *
 * | BasicBlock::Create(LLVMContext&, string&, Function*)
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 */
BasicBlock* newbb(const string& name) {
    BasicBlock* res = BasicBlock::Create(TheContext, name, CurrentFunction);
    Builder.SetInsertPoint(res);
    return res;
}

/* Add a new global variable
 *
 */
void doglobal(string name, Type* tp) {
    if (globals.find(name) != globals.end()) yyerror("defined global twice");

    TheModule.getOrInsertGlobal(name, tp);
    GlobalVariable* var = TheModule.getNamedGlobal(name);
    globals[name] = var;
    var->setInitializer(ConstantAggregateZero::get(tp));
}

/* Declare a new variable in the top scope with a type and (optional) value
 *
 * Use 'docast()'
 * 
 * | IRBuilder::CreateAlloca(Type*, (NULL), string& name)
 * | IRBuilder::CreateStore(Value*, Value*)
 */
AllocaInst* decl(string name, Type* tp, Value* initial) {
    if (scopes.size() == 0) {
        doglobal(name, tp);
        return NULL;
    }
    Scope& s = scopes[scopes.size() - 1];
    if (s.vars.find(name) != s.vars.end()) yyerror("defined variable twice");

    AllocaInst* res = Builder.CreateAlloca(tp, NULL, name);
    s.vars[name] = res;

    if (initial) {
        /* Assign initial value */
        Builder.CreateStore(docast(initial, tp), res);
    }
    return res;
}

/* Create a new function with a given name, parameter types, parameter names, and return type.
 * If 'is_extern', the function is just declared, but not defined. If 'is_vararg', the function is
 *   variadic (like printf)
 *
 * You should use 'Function::ExternalLinkage' for extern and functions being defined within
 *   the module.
 * 
 * You should also clear your labels/gotos (since they are per function)
 * 
 * | FunctionType::get(Type*, vector<Types*>&, bool)
 * | Function::Create(FunctionType*, (Function::ExternalLinkage), string&, Module*) 
 */
Function* newfunc(string name, vector<Type*> types, vector<string> names, Type* rtype, bool is_extern, bool is_vararg) {
    if (funcs.find(name) != funcs.end()) yyerror("defined function twice");

    FunctionType* FT = FunctionType::get(rtype, types, is_vararg);
    Function* F = Function::Create(FT, Function::ExternalLinkage, name, TheModule);

    labels.clear();
    gotos.begin();

    if (!is_extern) {
        CurrentFunction = F;
        /* Append a new scope for this function */
        scopes.push_back(Scope());

        /* Create a body for it if it is not extern */
        BasicBlock* bb = newbb("entry");

        /* Declare arguments as locals, so they can be stored to (and so they
         *   have names)
         */
        size_t i = 0;
        for (Argument& arg : F->args()) {
            decl(names[i], arg.getType(), &arg);
            i++;
        }
    }

    /* Add to list of functions defined */
    funcs.insert({ name, F });

    return F;
}

/* End the current function being generated
 *
 * You should iterate through 'labels' and 'gotos', and back patch them, similar to 'endloop()'
 * 
 * You should store labels and goto locally, then clear them
 * 
 * NOTE: This should only be called after a defined function (not after an 'extern' declaration)
 */
void endfunc() {
    auto mygotos = gotos;
    gotos.clear();

    for (auto& kv : mygotos) {
        Builder.SetInsertPoint(kv.first);
        dobr(labels[kv.second]);
    }

    scopes.pop_back();
    CurrentFunction = NULL;
}

/* Enter a loop scope
 *
 * Just push on a 'LoopCFs' object to 'cfs' stack
 */
void newloop() {
    cfs.push_back(LoopCFs());
}

/* Finalize a loop and backpatch 'break' and 'continue' statements
 *
 * The *first* thing you should do is pop off a 'LoopCFs' from 'cfs'. Otherwise,
 *   using 'dobr()' will not work, since it will check all of 'cfs', and prevent
 *   you from generating the code.
 * 
 * Then, loop through the breaks and continues, make sure to set your insertion
 *   point to the blocks in which they are, and then branch to either 'break_to'
 *   or 'continue_to' respectively. (use 'dobr()')
 * 
 * | IRBuilder::SetInsertPoint(BasicBlock* bb);
 */
void endloop(BasicBlock* break_to, BasicBlock* continue_to) {
    LoopCFs loop = cfs.back();
    cfs.pop_back();
    
    BasicBlock* after = Builder.GetInsertBlock();

    /*  */
    for (BasicBlock* bb : loop.cf_breaks) {
        Builder.SetInsertPoint(bb);
        dobr(break_to);
    }
    for (BasicBlock* bb : loop.cf_continues) {
        Builder.SetInsertPoint(bb);
        dobr(continue_to);
    }

    
    /* Restore */
    Builder.SetInsertPoint(after);
}


/** Expression/Value **/

/* Look up a var in our stack of scopes, returning (by default) a load
 *   representing the value at this point in time, or if given 'do_load==false',
 *   return the address as an AllocaInst
 *
 * There's a special case (which is specific to C), that arrays decay into pointers.
 *   We just always decay them into pointers -- we 'bitcast' a pointer to an array, to a pointer
 *   of the elements. 
 *  
 * Essentially, once a variable has been located, get the type (remember -- all elements
 *   in our scope will be an alloca instruction, which means they will be a pointer to the actual type,
 *   i.e. 'int x;' will store 'x' as a 'int*' internally, use 'getPointerElementType()' to get 'int' from
 *   'int*').
 * 
 * Then, check whether that type (declared type, not internal type) is an array type (isArrayTy()), and if so,
 *   replace the return value with itself casted (CreateBitCast()) to a pointer to the element
 *   type (getArrayElementType()) of the array. So, you should get the array element type, and then call
 *   'getPointerTo()' on that.
 * 
 * You'll also need to check 'globals' if all that fails
 * 
 * | Value::getType()
 * | Type::getPointerElementType()
 * | Type::isArrayType()
 * | Type::getArrayElementType()
 * | Type::getPointerTo()
 * | Builder::CreateBitCast(Value*, Type*)
 * | Builder::CreateLoad(Value*)
 * | Module::getFunction(string&)
 */
Value* dovar(string name, bool do_load) {
    for (int i = scopes.size() - 1; i >= 0; i--) {
        if (scopes[i].vars.find(name) != scopes[i].vars.end()) {
            /* Found in the scope, so get it */
            Value* res = scopes[i].vars[name];
            Type* tp = res->getType()->getPointerElementType();
            if (tp->isArrayTy()) {
                /* Bit cast to a pointer */
                res = Builder.CreateBitCast(res, tp->getArrayElementType()->getPointerTo());
            } else if (do_load) {
                /* By default, we should load it to actually get the value 
                 *   but, you can ask for no load if that works as well
                 */
                res = Builder.CreateLoad(res);
            }
            return res;
        }
    }

    /* Attempt to find it as a function */
    Function* res_func = TheModule.getFunction(name);
    if (res_func) return res_func;
    
    /* Attempt to find it as a global */
    if (globals.find(name) != globals.end()) { 
        Value* res = globals[name]; 
        Type* tp = res->getType()->getPointerElementType();

        if (tp->isArrayTy()) {
            /* Bit cast to a pointer */
            res = Builder.CreateBitCast(res, tp->getArrayElementType()->getPointerTo());
        } else if (do_load) {
            /* By default, we should load it to actually get the value 
                *   but, you can ask for no load if that works as well
                */
            res = Builder.CreateLoad(res);
        }
        return res;
    }

    yyerror("use of unknown variable");
    return NULL;
}

/* Perform a binary set operator (i.e. assignment) operator,
 *   where 'aL' is the *address* of the left operand, and 'R'
 *   is the *value* of the right operand.
 *   
 * When 'op' is '=', it is just normal assignment
 * 
 * I would suggest using the 'dobop()' method dynamically, and doing basically:
 * 'L = load(aL); res = doset("=", aL, dobop(op[:op.size()-1], L, R))'
 * 
 * | Type::getPointerElementType()
 * | Builder::CreateStore(Value*, Value*)
 */
Value* doset(string op, Value* aL, Value* R) {
    Value* res = NULL;

    /**/ if (op == "=") {
        Type* tp = aL->getType()->getPointerElementType();
        return Builder.CreateStore(docast(R, tp), aL);
    } else {
        Value* L = Builder.CreateLoad(aL);
        res = doset("=", aL, dobop(op.substr(0, op.size()-1), L, R));
    }
    
    if (!res) yyerror("bad binary operator (assignment)");
    return res;
}

/* Perform a binary operator on two sides of the equation
 * 
 * | IRBuilder::CreateAdd(Value*, Value*)
 * | IRBuilder::CreateFAdd(Value*, Value*)
 * | IRBuilder::CreateSub(Value*, Value*)
 * | IRBuilder::CreateFSub(Value*, Value*)
 * | IRBuilder::CreateMul(Value*, Value*)
 * | IRBuilder::CreateFMul(Value*, Value*)
 * | IRBuilder::CreateSDiv(Value*, Value*)
 * | IRBuilder::CreateFDiv(Value*, Value*)
 *
 * | IRBuilder::CreateAnd(Value*, Value*)
 * | IRBuilder::CreateOr(Value*, Value*)
 * | IRBuilder::CreateXor(Value*, Value*)
 * 
 * | IRBuilder::CreateShl(Value*, Value*)
 * | IRBuilder::CreateAShr(Value*, Value*)
 *
 * | IRBuilder::CreateICmpSLT(Value*, Value*)
 * | IRBuilder::CreateFCmpOLT(Value*, Value*)
 * | ...
 */
Value* dobop(string op, Value* L, Value* R) {
    Value* res = NULL;
    /**/ if (op == "+") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateAdd(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFAdd(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "-") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateSub(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFSub(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "*") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateMul(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFMul(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "/") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateSDiv(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFDiv(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "&") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateAnd(L, R);
        }
    } else if (op == "|") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateOr(L, R);
        }
    } else if (op == "^") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateXor(L, R);
        }
    } else if (op == "<<") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateShl(L, R);
        }
    } else if (op == ">>") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateAShr(L, R);
        }
    } else if (op == "<") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateICmpSLT(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFCmpOLT(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "<=") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateICmpSLE(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFCmpOLE(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == ">") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateICmpSGT(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFCmpOGT(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == ">=") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateICmpSGE(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFCmpOGE(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "==") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateICmpEQ(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFCmpOEQ(docast(L, types["double"]), docast(R, types["double"]));
        }
    } else if (op == "!=") {
        if (IS_INT(L) && IS_INT(R)) {
            res = Builder.CreateICmpNE(L, R);
        } else if (IS_FLT(L) || IS_FLT(R)) {
            res = Builder.CreateFCmpONE(docast(L, types["double"]), docast(R, types["double"]));
        }
    }
    
    if (!res){
         yyerror("bad binary operator");
    }

    return res;
}

/* Perform a unary operator on a value
 *
 * For '!L', you'll want to create a comparison to 0
 * 
 * | IRBuilder::CreateNeg(Value*)
 * | IRBuilder::CreateFNeg(Value*)
 * | IRBuilder::CreateICmpEQ(Value*, Value*)
 * | IRBuilder::CreateNot(Value*, Value*)
 * | IRBuilder::CreateLoad(Value*)
 */
Value* douop(string op, Value* L) {
    Value* res = NULL;
    /**/ if (op == "+") {
        res = L;
    } else if (op == "-") {
        /**/ if (IS_INT(L)) {
            res = Builder.CreateNeg(L);
        } else if (IS_FLT(L)) {
            res = Builder.CreateFNeg(L);
        }
    } else if (op == "!") {
        /**/ if (IS_INT(L)) {
            res = Builder.CreateICmpEQ(L, docast(v0, L->getType()));
        } else if (IS_PTR(L)) {
            res = douop(op, Builder.CreatePtrToInt(L, types["long"]));
        }
    } else if (op == "~") {
        /**/ if (IS_INT(L)) {
            res = Builder.CreateNot(L);
        }
    } else if (op == "*") {
        res = Builder.CreateLoad(L);
    }
    
    if (!res) yyerror("bad operator");
    return res;
}

/* Perform a cast to a given type
 *
 * You should check whether the type (getType()) is already correct --
 *   just return 'L' in that case. Otherwise, check if the type you're
 *   casting to is floating point. If so, the result should be created with
 *   'CreateFPCast()'. If that doesn't match, just let it throw the 'bad cast'
 *   error.
 * 
 * You can use 'CreatePointerCast' to cast between pointer types 
 * 
 * For 'int' -> 'int' you should use 'SExt' (sign extension)
 * 
 * | Value::getType()
 * | Type::isPointerTy()
 * | Type::isFloatingPointTy()
 * | Type::isIntegerTy()
 * | IRBuilder::CreatePointerCast()
 * | IRBuilder::CreateSExt()
 * | IRBuilder::CreateSIToFP(Value*, Type*)
 * | IRBuilder::CreateFPToSI(Value*, Type*)
 */
Value* docast(Value* L, Type* to) {
    Value* res = NULL;
    /**/ if (L->getType() == to) {
        res = L;
    } else if (to->isPointerTy() && L->getType()->isPointerTy()) {
        res = Builder.CreatePointerCast(L, to);
    } else if (to->isIntegerTy() && L->getType()->isIntegerTy()) {
        res = Builder.CreateSExt(L, to);
    } else if (to->isFloatingPointTy() && L->getType()->isIntegerTy()) {
        res = Builder.CreateSIToFP(L, to);
    } else if (to->isIntegerTy() && L->getType()->isFloatingPointTy()) {
        res = Builder.CreateFPToSI(L, to);
    }

    if (!res) yyerror("bad cast");
    return res;
}

/* Convert 'L' into a conditional (if it isn't already one)
 *
 * | IRBuilder::CreateICmpNE(Value*, Value*)
 * | IRBuilder::CreateFCmpONE(Value*, Value*)
 * | IRBuilder::GetInsertBlock()
 */
Value* docond(Value* L) {
    if (IS_BOOL(L)) {
        /* do nothing */
    } else if (IS_INT(L)) {
        L = Builder.CreateICmpNE(docast(L, types["int"]), docast(v0, types["int"]));
    } else if (IS_FLT(L)) {
        L = Builder.CreateFCmpONE(L, docast(v0, L->getType()));
    } else {
        yyerror("invalid type for conditional");
    }
    return L;
}

/* Perform a function call like: L(args[0], ... args[N-1])
 *
 * This one is pretty simple -- if the function is variadic, just
 *   emit a 'CreateCall'. Otherwise, generate a new vector
 *   of casted values.
 * 
 * You should use 'getFunctionParamType' on 'L''s type to see what to cast to
 * 
 * One odd thing is that the type of L is actually a pointer to a function, so use
 *   getPointerElementType() before checking if it is a function
 * 
 * | Type::getPointerElementType()
 * | Type::isFunctionTy()
 * | Type::isFunctionVarArg()
 * | Type::getFunctionParamType(int)
 * | IRBuilder::CreateCall(Value*, vector<Value*>& args)
 */
Value* docall(Value* L, vector<Value*> args) {
    Value* res = NULL;

    Type* tp = L->getType()->getPointerElementType();
    if (tp->isFunctionTy() && !tp->isFunctionVarArg()) {
        vector<Value*> casted;
        int i = 0;
        for (Value* arg : args) {
            casted.push_back(docast(arg, tp->getFunctionParamType(i)));
            i++;
        }
        res = Builder.CreateCall((FunctionType*)tp, L, casted);

    } else {
        res = Builder.CreateCall((FunctionType*)tp, L, args);
    }

    return res;
}

/* Perform an indexing (i.e. subscripting with '[]') operation
 *
 * LLVM uses the 'GetElementPointer' (GEP) instruction to compute an item.
 * I've linked to a page that covers this instruction, but essentially,
 *   it returns the address of an indexed element -- not the element itself.
 * 
 * Sometimes, we want the address (like when assigning a value to it), but by default,
 *   we want the value at the address. Therefore, to make it a reusable function,
 *   if 'do_load' is given, we also emit a load instruction. Otherwise, we just 
 *   return the address (which can be passed to 'doset()', for example)
 * 
 * You may be frustrated if you try and use this with an array type; this is why in 
 *   'dovar()' you are instructed to decay the array to a pointer via a bitcast. Once it
 *   is bitcasted to a pointer, this function should work fine just using 'GEP'
 * 
 * SEE: https://llvm.org/docs/GetElementPtr.html
 *
 * | Builder::CreateGEP(Value*, Value*)
 * | Builder::CreateLoad(Value*)
 */
Value* doindex(Value* L, Value* idx, bool do_load) {
    Value* res = NULL;
    res = Builder.CreateGEP(L, idx);

    if (do_load) {
        res = Builder.CreateLoad(res);
    }

    if (!res) yyerror("doindex() not implemented");
    return res;
}

/* Perform a short-circuit or (i.e. '||') on two operands
 * 
 * Keep in mind that 'L' and 'R' are in different basic blocks, and you should
 *   only execute 'bbR' if 'L' was NOT true. You will need to keep them in their
 * 
 * You will need to create a new block that will hold the result (as a PHI node).
 * 
 * Then, go back to bbR (use IRBuilder::SetInsertPoint), and in it, convert 'R' into
 *   a conditional via 'docond()'. Then, use the 'dobr()' to create a break from bbR
 *   into the 'after' BasicBlock, in which the PHI node is.
 * 
 * Now, you go back to 'bbL', and you'll want to use 'docondbr()' to branch to 'bbR' if 'L' is false,
 *   (use IRBuilder::CreateNot to invert the value). Otherwise, you should branch to 'after' (use
 *   this as the last argument to 'docondbr'()) directly.
 * 
 * Finally, go back to your 'after' basic block, and create a PHI node (CreatePHI()),
 *   and reserve '2' spots (since there are two possible values it can have). To fill in the values,
 *   use 'PHINode::addIncoming()', where you'll give it the value (L and R as conditionals), as well
 *   as where they would come from (bbL and bbR as BasicBlock)
 * 
 * This function should return the PHINode
 *
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 * | IRBuilder::CreateNot(Value*)
 * | IRBuilder::CreatePHI(Type*, int=(2))
 * | PHINode::addIncoming(Value*, BasicBlock*)
 */
PHINode* docor(BasicBlock* bbL, Value* L, BasicBlock* bbR, Value* R) {
    PHINode* res = NULL;

    BasicBlock* after = newbb();

    Builder.SetInsertPoint(bbR);
    R = docond(R);
    dobr(after);

    Builder.SetInsertPoint(bbL);
    docondbr(Builder.CreateNot(docond(L)), bbR, after);
    dobr(after);

    Builder.SetInsertPoint(after);
    res = Builder.CreatePHI(L->getType(), 2);
    res->addIncoming(L, bbL);
    res->addIncoming(R, bbR);

    if (!res) yyerror("docor() not implemented");
    return res;
}

/* Perform a short-circuit and (i.e. '&&') on two operands
 *
 * This is very similar to 'docor()', except you should go to 'bbR' if 
 *   'L' was truthy, and if 'L' wasn't, then skip ahead to 'after'
 * 
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 * | IRBuilder::CreatePHI(Type*, int=(2))
 * | PHINode::addIncoming(Value*, BasicBlock*)
 */
PHINode* docand(BasicBlock* bbL, Value* L, BasicBlock* bbR, Value* R) {
    PHINode* res = NULL;

    BasicBlock* after = newbb();

    Builder.SetInsertPoint(bbR);
    R = docond(R);
    dobr(after);

    Builder.SetInsertPoint(bbL);
    docondbr(L, bbR, after);
    dobr(after);

    Builder.SetInsertPoint(after);
    res = Builder.CreatePHI(L->getType(), 2);
    res->addIncoming(L, bbL);
    res->addIncoming(R, bbR);

    if (!res) yyerror("docand() not implemented");
    return res;
}

/** Statements/ControlFlow **/

/* Generate code to move to another block from the current block, if
 *   there is not already a terminating instruction already.
 * 
 * This is done so that you do not generate code that contains multiple terminators --
 *   in LLVM, only one branching instruction (including ret/br/br conditional) may be present
 *   in a basic block, and it must be at the end. For example, if you have 'return 5;' within
 *   the body of an 'if', the code should not break out of the loop afterwards, because it already
 *   has jumped out. That would be malformed IR
 * 
 * Therefore, you should check the current block and see if it has a terminator. If it does,
 *   then nothing else should be done. Otherwise, create a simple branch statement.
 * 
 * To accomodate 'break' and 'continue', you should also check if any of the loops in 'cfs' contain
 *   the current block (GetInsertBlock()) in their 'cf_breaks' or 'cf_continues'. If any of them do,
 *   then you shouldn't do a 'br' here, since the actual 'break'/'continue' will be backpatched later.
 *  
 * You should do the same with 'gotos'
 * 
 * | IRBuilder::GetInsertBlock()
 * | BasicBlock::getTerminator() // returns NULL if there was no terminator for the block
 * | IRBuilder::CreateBr(BasicBlock*)
 */
void dobr(BasicBlock* to) {
    BasicBlock* from = Builder.GetInsertBlock();
    if (from->getTerminator() == NULL) {
        for (LoopCFs& loop : cfs) {
            if (find(loop.cf_breaks, from) != loop.cf_breaks.end() || find(loop.cf_continues, from) != loop.cf_continues.end()) {
                return;
            }
        }
        if (gotos.find(from) != gotos.end()) return;
        
        Builder.CreateBr(to);
    }
    // yyerror("dobr() not implemented");
}

/* The same as 'dobr', except you will be using 'CreateCondBr'
 *
 * Also, use the 'docond()' function to ensure 'cond' is a valid conditional for LLVM
 * 
 * This code should cause the current block to jump to 'bb_T' if 'cond' was truthy, and
 *   'bb_F' otherwise. Both should be non-NULL.
 * 
 * This is an important function -- your implementations of short circuit operators, if/while/do/for,
 *   should use this function
 * 
 * | IRBuilder::GetInsertBlock()
 * | BasicBlock::getTerminator() // returns NULL if there was no terminator for the block
 * | IRBuilder::CreateCondBr(Value*, BasicBlock*, BasicBlock*)
 */
void docondbr(Value* cond, BasicBlock* bb_T, BasicBlock* bb_F) {
    BasicBlock* from = Builder.GetInsertBlock();
    if (from->getTerminator() == NULL) {
        for (LoopCFs& loop : cfs) {
            if (find(loop.cf_breaks, from) != loop.cf_breaks.end() || find(loop.cf_continues, from) != loop.cf_continues.end()) {
                return;
            }
        }
        if (gotos.find(from) != gotos.end()) return;

        cond = docond(cond);
        BranchInst* br = Builder.CreateCondBr(cond, bb_T, bb_F);
    }

    // yyerror("docondbr() not implemented");
}

/* Perform a 'return' statement, with the provided value
 *
 * If 'val' is NULL, you should return void
 * 
 * | IRBuilder::CreateRet(Value*)
 * | IRBuilder::CreateRetVoid()
 */
void doret(Value* val) {
    if (val) {
        Builder.CreateRet(val);
    } else {
        Builder.CreateRetVoid();
    }
}

/* Perform a 'break' statement
 *
 * Here you should just record the statemnt, by taking the top of the 'cfs' stack
 *   and adding the current block to 'cf_breaks' -- you actually do the backpatching in 'endloop()'
 *
 * | IRBuilder::GetInsertBlock();
 */
void dobreak() {
    LoopCFs& loop = cfs.back();
    loop.cf_breaks.push_back(Builder.GetInsertBlock());
}

/* Perform a 'continue' statement (analogous to 'dobreak()')
 *
 * | IRBuilder::GetInsertBlock();
 */
void docontinue() {
    LoopCFs& loop = cfs.back();
    loop.cf_continues.push_back(Builder.GetInsertBlock());
}

/* Perform a 'goto' statement
 *
 * Since labels can be defined after the 'goto', we cannot for-sure link it at this time.
 * Therefore, the linking happens in 'endloop()'
 *
 * You'll just want to 'insert' the current block and name it's goto-ing into the 'gotos' map
 * 
 * You'll also need to create a 'newbb()' after you insert it, since it is going to terminate
 *   the current block
 */
void dogoto(string name) {
    gotos.insert({Builder.GetInsertBlock(), name});
    newbb();

    //yyerror("dogoto() not implemented");
}

/* Perform a '<label>:' statement
 *
 * Firstly, you'll want to record where you are currently via 'GetInsertBlock().
 * 
 * Then, you'll want to create a 'newbb()' that the label will refer to. Then, go
 *   ahead and insert that into your labels.
 * 
 * Remember -- by default statements should 'flow through' to their label. Therefore,
 *   you should rewind back to where you came from and call 'dobr()' to link the block
 *   you started on to the new label.
 * 
 * Then, since we need to generate code, make sure to resume at the new label's block
 * 
 * | IRBuilder::GetInsertBlock()
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 */
BasicBlock* dolabel(string name) {
    BasicBlock* res = NULL;
    BasicBlock* from = Builder.GetInsertBlock();
    
    res = newbb();
    labels.insert({name, res});

    Builder.SetInsertPoint(from);
    dobr(res);
    Builder.SetInsertPoint(res);

    if (!res) yyerror("dolabel() not implemented");
    return res;
}


/* Generate code for an 'if' construct, which should include conditional branches
 *
 * bb_T: the block to run if the condition was true
 * bb_T_end: the last block in the 'true' CFG node, that should be backpatched
 * bb_F: the block to run if the conditional was false
 *       NOTE: this will be NULL if you have 'if (...) {...}', it is only valid
 *               when there is an 'else' branch
 * bb_F_end: the last block in the 'false' CFG node, that should be backpatched
 * after: the block that should be ran after the true/false branches (i.e. where
 *   they merge back together)
 * 
 * As of this point, 'bb_T' and 'bb_F' have already been fully generated.
 * 
 * So, use the functions 'docondbr' and 'dobr' to backpatch the code to jump
 *   correctly -- remember that 'bb_F' may be NULL if there was no 'else'.
 * 
 * Make sure to restore the code generation point (SetInsertPoint) to 'after',
 *   after you have patched the conditionals.
 * 
 * CFG:
 * +-------+
 * |before |
 * +-------+
 *  |
 * +-------+  F  +-------+
 * | cond  |-----| bb_F  |
 * +-------+     +-------+
 *  |T           |bb_Fend|
 * +-------+     +-------+
 * | bb_T  |      |
 * +-------+      |
 * |bb_Tend|-----+-------+
 * +-------+     | after |
 *               +-------+           
 *                 
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 */
void doif(BasicBlock* before, Value* cond, BasicBlock* bb_T, BasicBlock* bb_Tend, BasicBlock* bb_F, BasicBlock* bb_Fend, BasicBlock* after) {
    Builder.SetInsertPoint(before);
    BranchInst* br = NULL;

    if (bb_F) {
        docondbr(cond, bb_T, bb_F);
    } else {
        docondbr(cond, bb_T, after);
    }

    Builder.SetInsertPoint(bb_Tend);
    dobr(after);

    if (bb_F) {
        Builder.SetInsertPoint(bb_Fend);
        dobr(after);
    }

    /* done, now resume normally */
    Builder.SetInsertPoint(after);

    //yyerror("doif() not implemented");
}

/* Generate code for a 'while'
 *
 * You should use other functions you've created to make this easier; the
 *   only LLVM call should be to insert the current block.
 * 
 * Use 'dobr' to connect the nodes below, and 'docondbr' for when there are multiple
 *   edges out from a single node. This is mainly just connecting the nodes in the CFG below.
 *
 * CFG:
 *  | 
 * +-------+
 * |before |
 * +-------+
 *  |
 * +-------+  F  +-------+
 * | cond  |-----| after |
 * +-------+<-+  +-------+
 *  |T        |
 * +-------+  |
 * | body  |  |
 * +-------+  |
 * |bodyend|--+
 * +-------+
 * 
 * 
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 */
void dowhile(BasicBlock* before, BasicBlock* bb_cond, Value* cond, BasicBlock* body, BasicBlock* body_end, BasicBlock* after) {
    endloop(after, bb_cond);

    /* before -> cond */
    Builder.SetInsertPoint(before);
    dobr(bb_cond);

    /* from (cond=T)-> body */
    /* from (cond=F)-> after */
    Builder.SetInsertPoint(bb_cond);
    docondbr(cond, body, after);

    /* body_end -> cond */
    Builder.SetInsertPoint(body_end);
    dobr(bb_cond);

    /* done, now resume normally */
    Builder.SetInsertPoint(after);

    //yyerror("dowhile() not implemented");
}

/* Generate code for a 'do...while'
 *
 * You should use other functions you've created to make this easier; the
 *   only LLVM call should be to insert the current block.
 * 
 * This is very similar to 'while', except maybe a bit easier -- if you look
 *   at the CFG you will notice that 'body' and 'cond' will always be concatenated
 *   and already joined. This means you do not need to patch them together, and
 *   therefore do not need the bb in which 'cond' was generated. You can think
 *   of them conceptually as one monolothic body
 *
 * CFG:
 *  |
 * +-------+
 * |before |
 * +-------+
 *  |
 * +-------+
 * | body  |
 * +-------+  F  +-------+
 * | cond  |-----| after |
 * +-------+     +-------+
 *  | |
 *  +-+
 * 
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 */
void dodo(BasicBlock* before, BasicBlock* body, BasicBlock* bb_cond, Value* cond, BasicBlock* after) {
    endloop(after, body);

    /* before -> body */
    Builder.SetInsertPoint(before);
    dobr(body);

    /* bb_cond (cond=T)-> body */
    /* bb_cond (cond=F)-> after */
    Builder.SetInsertPoint(bb_cond);
    docondbr(cond, body, after);

    /* done, now resume normally */
    Builder.SetInsertPoint(after);

    //yyerror("dodo() not implemented");
}

/* Generate code for a 'for (init; cond; inc) {body}' construct
 * 
 * Conceptually, this is similar to the other loops -- just a little more complicated.
 * 
 * Similar to 'while', some parameters are not even needed, because they are not relevant
 *   to the CFG, or already concatenated via nature of their position in the grammar
 * 
 * Therefore, the parameters are:
 *   before: 'bb' before the construct, which the init is concatenated to
 *   bb_cond: 'bb' containing the conditional 'cond'
 *   cond: the conditional expression (may be NULL -- see below)
 *   bb_inc: 'bb' containing the 'increment' expression. even though this bb
 *             will be generated before 'body', it should be appended to 'body',
 *             unconditionally
 *   body: the start of the body
 *   body_end: the last part of the body -- this is what should be patched to 'inc'
 *   after: the sync point after the loop
 * 
 * If you look in the grammar, each of the 'init,cond,inc' are 'expro' which is optional
 *   expression. They may be NULL. This will only affect the 'cond' in this function, the bbs
 *   passed in will be non-NULL (although they may be empty -- they are not invalid).
 * 
 * Therefore, if 'cond' is NULL (i.e. empty), you should default it to 'true', such that it
 *   makes a forever loop. You can use 'getTrue()' to generate such a value
 * 
 * CFG:
 *  |
 * +-------+
 * |before |
 * +-------+
 * | init  |
 * +-------+
 *  |
 * +-------+  F  +-------+
 * | cond  |-----| after |
 * |       |<-+  |       |
 * +-------+  |  +-------+
 *  |T        |
 * +-------+  |
 * | body  |  |
 * +-------+  |
 * |bodyend|  |
 * +-------+  |
 *  |         |
 * +-------+  |
 * | inc   |--+
 * +-------+
 * 
 * | IRBuilder::SetInsertPoint(BasicBlock*)
 * | IRBuilder::getTrue()
 */
void dofor(BasicBlock* before, BasicBlock* bb_cond, Value* cond, BasicBlock* bb_inc, BasicBlock* body, BasicBlock* body_end, BasicBlock* after) {
    endloop(after, bb_cond);

    /* before -> cond */
    Builder.SetInsertPoint(before);
    dobr(bb_cond);

    /* cond (cond=T)-> body */
    /* cond (cond=F)-> after */
    Builder.SetInsertPoint(bb_cond);
    if (!cond) cond = Builder.getTrue();
    docondbr(cond, body, after);

    /* body_end -> inc */
    Builder.SetInsertPoint(body_end);
    dobr(bb_inc);

    /* inc -> cond */
    Builder.SetInsertPoint(bb_inc);
    dobr(bb_cond);

    /* done, now resume normally */
    Builder.SetInsertPoint(after);

    //yyerror("dofor() not implemented");
}
