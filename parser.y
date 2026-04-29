%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "automata.h"

extern int yylex(void);
void yyerror(const char *s);
%}

%union {
    char *str;
}

%token <str> ID
%token STATES ALPHABET START FINAL COLON NEWLINE

%%

program
    : sections transitions
    | sections
    ;

sections
    : sections section
    | section
    ;

section
    : STATES   COLON { automata_begin_states();   } id_list newline_or_eof
    | ALPHABET COLON { automata_begin_alphabet(); } id_list newline_or_eof
    | START    COLON ID newline_or_eof  { set_start_state($3); free($3); }
    | FINAL    COLON { automata_begin_finals();   } id_list newline_or_eof
    | NEWLINE
    ;

newline_or_eof
    : NEWLINE
    | /* empty (EOF) */
    ;

id_list
    : id_list ID  { parser_add_identifier($2); free($2); }
    | ID          { parser_add_identifier($1); free($1); }
    ;

transitions
    : transitions transition
    | transition
    ;

transition
    : ID ID ID newline_or_eof {
        add_transition($1, $2, $3);
        free($1); free($2); free($3);
    }
    | NEWLINE
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file> [test_strings...]\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }

    extern FILE *yyin;
    yyin = f;
    yyparse();
    fclose(f);

    print_nfa();
    convert_nfa_to_dfa();
    print_dfa();

    for (int i = 2; i < argc; i++) {
        printf("Input: \"%s\"\n", argv[i]);
        simulate_nfa(argv[i]);
        simulate_dfa(argv[i]);
        printf("\n");
    }

    return 0;
}
