%{
#include "parser.tab.h"
#include <string.h>
%}

%%

"states"    { return STATES; }
"alphabet"  { return ALPHABET; }
"start"     { return START; }
"final"     { return FINAL; }

":"         { return COLON; }

\r\n        { return NEWLINE; }
\n          { return NEWLINE; }
\r          { return NEWLINE; }

[a-zA-Z0-9_]+  { yylval.str = strdup(yytext); return ID; }

[ \t]+      ;   /* ignore whitespace */

.           { return yytext[0]; }

%%

int yywrap() { return 1; }
