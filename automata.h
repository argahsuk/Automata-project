#ifndef AUTOMATA_H
#define AUTOMATA_H

#define MAX_STATES       64
#define MAX_SYMBOLS      32
#define MAX_TRANSITIONS  256
#define MAX_DFA_STATES   128
#define MAX_NAME         32

/* ── Build functions (called by parser) ──────────────────────────── */
void automata_begin_states(void);
void automata_begin_alphabet(void);
void automata_begin_finals(void);
void parser_add_identifier(const char *name);
void set_start_state(const char *name);
void add_transition(const char *from, const char *sym, const char *to);

/* ── Print ────────────────────────────────────────────────────────── */
void print_nfa(void);
void print_dfa(void);

/* ── Conversion & simulation ──────────────────────────────────────── */
void convert_nfa_to_dfa(void);
void simulate_nfa(const char *input);
void simulate_dfa(const char *input);

#endif /* AUTOMATA_H */
