%{
/* Grammar for a small subset of C
 *
 */

#include <mycc.h>

int yylex();

extern int yylineno;
/*
 * yyerror - issue error message
 */
void
yyerror (const char *msg)
{
   fprintf(stderr, " %s.  Line %d\n", msg, yylineno);
   exit(1);
}
/* This is weird, because some C++ standards don't allow allocating pairs with 'new'... 
 * This structure is given to
 */
struct FuncPars {
    vector<Type*> types; 
    vector<string> names;
};

%}
%union {
    /* NOTE: we must use pointers, since unions cannot handle complicated destructors */
    llvm::BasicBlock* bb;
    llvm::Type* tp;
    llvm::Value* val;

    vector<llvm::Value*>* vals;

    struct FuncPars* funcpars;

    const char* str;
}

%type  <sr> blkend

%type  <bb> bb curbb

%type  <tp> type dhead

%type  <val> expr atom
%type  <val> expro E0 E1 E2 E3 E4 E5 E6 E7
%token <val> INT FLOAT STRING

%type  <val> cond

%type  <vals> exprs

%token <str> NAME TYPENAME

%type  <funcpars> fpars


%token ADD SUB MUL DIV MOD NOT NEG SQIG SET LT LE GT GE EQ NE OROR ANDAND OR AND XOR LSHIFT RSHIFT
%token SETADD SETSUB SETMUL SETDIV SETMOD SETOR SETAND SETXOR SETLSHIFT SETRSHIFT
%token COM COL SEMI
%token LPAR RPAR LBRC RBRC LBRK RBRK
%token EXTERN
%token SIZEOF TYPEOF TYPEDEF
%token RETURN IF ELSE WHILE FOR BREAK CONTINUE GOTO DO

%%
program : program program
        | func
        | efunc
        | TYPEDEF type NAME SEMI { types[(string)$3] = $2; }
        | dhead SEMI
        |
        ;

efunc   : EXTERN type NAME LPAR fpars RPAR SEMI { newfunc($3, $5->types, $5->names, $2, true); }
        | EXTERN type NAME LPAR RPAR SEMI { newfunc($3, {}, {}, $2, true); }
        ;

func    : fhead RPAR stmt        { endfunc(); }
        ;

fhead   : type NAME LPAR fpars   { newfunc($2, $4->types, $4->names, $1); }
        | type NAME LPAR         { newfunc($2, {}, {}, $1); }
        ;

fpars   : fpars COM type NAME    { $$->types.push_back($3); $$->names.push_back((string)$4); }
        | type NAME              { $$ = new FuncPars; $$->types.push_back($1); $$->names.push_back((string)$2); }
        ;

blkend  : stmt blkend            {}
        | RBRC                   {}
        ;

curbb   :                        { $$ = Builder.GetInsertBlock(); }     
        ;

nl      :                        { newloop(); }
        ;

bb      :                        { $$ = newbb(); }
        ;

cond    : expr                   { $$ = docond($1); }
        ;

dhead   : dhead COM NAME         { $$ = $1; decl($3, $1, NULL); }
        | dhead COM NAME SET expr{ $$ = $1; decl($3, $1, $5); }
        | dhead COM NAME LBRK INT RBRK { $$ = $1; decl($3, ArrayType::get($1, dyn_cast<ConstantInt>($5)->getSExtValue()), NULL); }
        | type NAME LBRK INT RBRK{ $$ = $1; decl($2, ArrayType::get($1, dyn_cast<ConstantInt>($4)->getSExtValue()), NULL); }
        | type NAME              { $$ = $1; decl($2, $1, NULL); }
        | type NAME SET expr     { $$ = $1; decl($2, $1, $4); }
        ;

stmt    : SEMI                   {  }
        | dhead SEMI             {  }
        | RETURN expro SEMI      { doret($2); }
        | BREAK SEMI             { dobreak(); }
        | CONTINUE SEMI          { docontinue(); }
        | GOTO NAME SEMI         { dogoto((string)$2); }
        | NAME COL               { dolabel((string)$1); }
        | expr SEMI              {}
        | LBRC blkend            {}
        | IF LPAR cond curbb  RPAR bb stmt curbb bb  { doif($4, $3, $6, $8, NULL, NULL, $9); }
        | IF LPAR cond curbb  RPAR bb stmt curbb ELSE bb stmt curbb bb  { doif($4, $3, $6, $8, $10, $12, $13); }
        | nl curbb WHILE LPAR bb cond RPAR bb stmt curbb bb  { dowhile($2, $5, $6, $8, $10, $11); }
        | nl curbb DO bb stmt WHILE LPAR curbb cond RPAR bb  { dodo($2, $4, $8, $9, $11); }
        | nl curbb FOR LPAR expro SEMI bb expro SEMI bb expro RPAR bb stmt curbb bb  { dofor($2, $7, $8, $10, $13, $15, $16); }
        ;

type    : TYPENAME               { $$ = types[(string)$1]; }
        | type MUL               { $$ = PointerType::get($1, 0/* addrspace */); }
        | LPAR type RPAR         { $$ = $2; }
        | TYPEOF E7              { $$ = $2->getType(); }
        ;

exprs   : exprs COM expr         { $$ = $1; $$->push_back($3); }
        | expr                   { $$ = new vector<llvm::Value*>({ $1 }); }
        ;

expro   :                        { $$ = NULL; }
        | expr                   {}
        ;

expr    : E0
        | NAME SET expr          { $$ = doset("=", dovar($1, false), $3); }
        | NAME SETADD expr       { $$ = doset("+=", dovar($1, false), $3); }
        | NAME SETSUB expr       { $$ = doset("-=", dovar($1, false), $3); }
        | NAME SETMUL expr       { $$ = doset("*=", dovar($1, false), $3); }
        | NAME SETDIV expr       { $$ = doset("/=", dovar($1, false), $3); }
        | NAME SETMOD expr       { $$ = doset("%=", dovar($1, false), $3); }
        | NAME SETOR expr        { $$ = doset("|=", dovar($1, false), $3); }
        | NAME SETXOR expr       { $$ = doset("^=", dovar($1, false), $3); }
        | NAME SETAND expr       { $$ = doset("&=", dovar($1, false), $3); }
        | NAME SETLSHIFT expr    { $$ = doset("<<=", dovar($1, false), $3); }
        | NAME SETRSHIFT expr    { $$ = doset(">>=", dovar($1, false), $3); }
        ;

E0      : E0 curbb OROR   bb E1  { $$ = docor($2, docond($1), $4, docond($5)); }
        | E0 curbb ANDAND bb E1  { $$ = docand($2, docond($1), $4, docond($5)); }
        | E1                     {}
        ;

E1      : E1 LT E2               { $$ = dobop("<", $1, $3); }
        | E1 LE E2               { $$ = dobop("<=", $1, $3); }
        | E1 GT E2               { $$ = dobop(">", $1, $3); }
        | E1 GE E2               { $$ = dobop(">=", $1, $3); }
        | E1 EQ E2               { $$ = dobop("==", $1, $3); }
        | E1 NE E2               { $$ = dobop("!=", $1, $3); }
        | E2                     {}
        ;

E2      : E2 OR E3               { $$ = dobop("|", $1, $3); }
        | E2 AND E3              { $$ = dobop("&", $1, $3); }
        | E2 XOR E3              { $$ = dobop("^", $1, $3); }
        | E3                     {}
        ;

E3      : E3 LSHIFT E4           { $$ = dobop("<<", $1, $3); }
        | E3 RSHIFT E4           { $$ = dobop(">>", $1, $3); }
        | E4                     {}
        ;

E4      : E4 ADD E4              { $$ = dobop("+", $1, $3); }
        | E4 SUB E4              { $$ = dobop("-", $1, $3); }
        | E5                     {}
        ;

E5      : E5 MUL E5              { $$ = dobop("*", $1, $3); }
        | E5 DIV E5              { $$ = dobop("/", $1, $3); }
        | E5 MOD E5              { $$ = dobop("%", $1, $3); }
        | E6                     {}
        ;

E6      : NOT E6                 { $$ = douop("!", $2); }
        | SUB E6                 { $$ = douop("-", $2); }
        | AND NAME               { $$ = dovar($2, false/* do_load */); }
        | MUL E6                 { $$ = douop("*", $2); }
        | SIZEOF type            { $$ = const_int(($2->getPrimitiveSizeInBits() + 7) / 8); }
        | E7
        ;

E7      : LPAR expr RPAR         { $$ = $2; }
        | E7 LPAR RPAR           { $$ = docall($1, {}); }
        | E7 LPAR exprs RPAR     { $$ = docall($1, *$3); }
        | E7 LBRK expr RBRK      { $$ = doindex($1, $3); }
        | E7 LBRK expr RBRK SET expr       { $$ = doset("=" , doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETADD expr    { $$ = doset("+=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETSUB expr    { $$ = doset("-=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETMUL expr    { $$ = doset("*=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETDIV expr    { $$ = doset("/=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETMOD expr    { $$ = doset("%=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETOR expr     { $$ = doset("|=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETXOR expr    { $$ = doset("^=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETAND expr    { $$ = doset("&=", doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETLSHIFT expr { $$ = doset("<<=",doindex($1, $3, false), $6); }
        | E7 LBRK expr RBRK SETRSHIFT expr { $$ = doset(">>=",doindex($1, $3, false), $6); }
        | atom                   {}
        ;


atom    : INT                    {}
        | FLOAT                  {}
        | STRING                 {}
        | NAME                   { $$ = dovar($1); }
        ;

%%
