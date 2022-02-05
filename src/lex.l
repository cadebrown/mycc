%{
#include <mycc.h>
#include <gram.h>
%}

%option nounput

%%
\/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+\/  {}
\/\/.*                                       {}


sizeof                       return SIZEOF;
typeof                       return TYPEOF;
typedef                      return TYPEDEF;
if                           return IF;
else                         return ELSE;
while                        return WHILE;
do                           return DO;
for                          return FOR;
break                        return BREAK;
continue                     return CONTINUE;
goto                         return GOTO;
return                       return RETURN;
extern                       return EXTERN;
[a-zA-Z_][a-zA-Z_0-9]*       yylval.str = strdup(yytext); if (types.find((string)yytext) != types.end()) { return TYPENAME; } else { return NAME; }
,                            return COM;
:                            return COL;
;                            return SEMI;

[0-9]+\.[0-9]+               yylval.val = const_float(yytext); return FLOAT;
[0-9]+                       yylval.val = const_int(yytext); return INT;
\"(\\.|[^"\\])*\"            yylval.val = const_strlit(yytext); return STRING;

\|\|                         return OROR;
\&\&                         return ANDAND;

\^                           return XOR;
\|                           return OR;
\&                           return AND;
\<\<                         return LSHIFT;
\>\>                         return RSHIFT;
\+                           return ADD;
\-                           return SUB;
\*                           return MUL;
\/                           return DIV;
\%                           return MOD;

\^\=                         return SETXOR;
\|\=                         return SETOR;
\&\=                         return SETAND;
\<\<\=                       return SETLSHIFT;
\>\>\=                       return SETRSHIFT;
\+\=                         return SETADD;
\-\=                         return SETSUB;
\*\=                         return SETMUL;
\/\=                         return SETDIV;
\%\=                         return SETMOD;

\<\=                         return LE;
\>\=                         return GE;
\<                           return LT;
\>                           return GT;
\=\=                         return EQ;
\!\=                         return NE;

\~                           return SQIG;
\!                           return NOT;


\=                           return SET;

\(                           return LPAR;
\)                           return RPAR;
\{                           return LBRC;
\}                           return RBRC;
\[                           return LBRK;
\]                           return RBRK;

\n                           yylineno++;
[ \t]+                       ;
.                            yyerror("bad token");
%%
