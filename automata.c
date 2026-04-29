#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "automata.h"

/* ══════════════════════════════════════════════════════════════════
   NFA data
   ══════════════════════════════════════════════════════════════════ */
static char nfa_states[MAX_STATES][MAX_NAME];
static int  nfa_state_count = 0;

static char nfa_symbols[MAX_SYMBOLS][MAX_NAME];
static int  nfa_sym_count = 0;

static char nfa_start[MAX_NAME];
static char nfa_finals[MAX_STATES][MAX_NAME];
static int  nfa_final_count = 0;

typedef struct { char from[MAX_NAME]; char sym[MAX_NAME]; char to[MAX_NAME]; } NfaTrans;
static NfaTrans nfa_trans[MAX_TRANSITIONS];
static int nfa_trans_count = 0;

/* ══════════════════════════════════════════════════════════════════
   DFA data (subset construction)
   ══════════════════════════════════════════════════════════════════ */
/* Each DFA state is a bitmask over NFA states */
typedef unsigned long long Mask;

typedef struct {
    Mask     mask;
    char     name[MAX_NAME];
    int      is_final;
    /* transitions: indexed by symbol index → DFA state index (-1 = dead) */
    int      next[MAX_SYMBOLS];
} DfaState;

static DfaState dfa[MAX_DFA_STATES];
static int      dfa_count = 0;

/* ══════════════════════════════════════════════════════════════════
   Parser section tracking
   ══════════════════════════════════════════════════════════════════ */
typedef enum { SEC_NONE, SEC_STATES, SEC_ALPHABET, SEC_FINALS } Section;
static Section cur_section = SEC_NONE;

/* pending "from" and "symbol" for transition lines */
static char pending_from[MAX_NAME];
static char pending_sym[MAX_NAME];
static int  pending_stage = 0;   /* 0=from, 1=sym, 2=to */

/* ── Helpers ─────────────────────────────────────────────────────── */
static int nfa_state_idx(const char *name) {
    for (int i = 0; i < nfa_state_count; i++)
        if (strcmp(nfa_states[i], name) == 0) return i;
    return -1;
}
static int nfa_sym_idx(const char *sym) {
    for (int i = 0; i < nfa_sym_count; i++)
        if (strcmp(nfa_symbols[i], sym) == 0) return i;
    return -1;
}
static int nfa_is_final(const char *name) {
    for (int i = 0; i < nfa_final_count; i++)
        if (strcmp(nfa_finals[i], name) == 0) return 1;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
   Build interface (called by parser actions)
   ══════════════════════════════════════════════════════════════════ */
void automata_begin_states(void)   { cur_section = SEC_STATES;   pending_stage = 0; }
void automata_begin_alphabet(void) { cur_section = SEC_ALPHABET; pending_stage = 0; }
void automata_begin_finals(void)   { cur_section = SEC_FINALS;   pending_stage = 0; }

void parser_add_identifier(const char *name) {
    switch (cur_section) {
    case SEC_STATES:
        if (nfa_state_count < MAX_STATES)
            strncpy(nfa_states[nfa_state_count++], name, MAX_NAME-1);
        break;
    case SEC_ALPHABET:
        if (nfa_sym_count < MAX_SYMBOLS)
            strncpy(nfa_symbols[nfa_sym_count++], name, MAX_NAME-1);
        break;
    case SEC_FINALS:
        if (nfa_final_count < MAX_STATES)
            strncpy(nfa_finals[nfa_final_count++], name, MAX_NAME-1);
        break;
    case SEC_NONE:
        /* transition token: from → sym → to */
        if (pending_stage == 0) {
            strncpy(pending_from, name, MAX_NAME-1);
            pending_stage = 1;
        } else if (pending_stage == 1) {
            strncpy(pending_sym, name, MAX_NAME-1);
            pending_stage = 2;
        } else {
            add_transition(pending_from, pending_sym, name);
            pending_stage = 0;
        }
        break;
    }
}

void set_start_state(const char *name) {
    strncpy(nfa_start, name, MAX_NAME-1);
    cur_section = SEC_NONE;
}

void add_transition(const char *from, const char *sym, const char *to) {
    if (nfa_trans_count >= MAX_TRANSITIONS) { fprintf(stderr,"too many transitions\n"); return; }
    strncpy(nfa_trans[nfa_trans_count].from, from, MAX_NAME-1);
    strncpy(nfa_trans[nfa_trans_count].sym,  sym,  MAX_NAME-1);
    strncpy(nfa_trans[nfa_trans_count].to,   to,   MAX_NAME-1);
    nfa_trans_count++;
}

/* ══════════════════════════════════════════════════════════════════
   Print NFA
   ══════════════════════════════════════════════════════════════════ */
void print_nfa(void) {
    printf("=== NFA ===\n");
    printf("States  : ");
    for (int i = 0; i < nfa_state_count; i++) printf("%s ", nfa_states[i]);
    printf("\nAlphabet: ");
    for (int i = 0; i < nfa_sym_count; i++) printf("%s ", nfa_symbols[i]);
    printf("\nStart   : %s\n", nfa_start);
    printf("Finals  : ");
    for (int i = 0; i < nfa_final_count; i++) printf("%s ", nfa_finals[i]);
    printf("\nTransitions:\n");
    for (int i = 0; i < nfa_trans_count; i++)
        printf("  %s --%s--> %s\n", nfa_trans[i].from, nfa_trans[i].sym, nfa_trans[i].to);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════
   Subset construction: NFA → DFA
   ══════════════════════════════════════════════════════════════════ */

/* Compute move(mask, sym_idx): set of NFA states reachable from mask on symbol */
static Mask move_set(Mask mask, int si) {
    Mask result = 0;
    for (int i = 0; i < nfa_state_count; i++) {
        if (!(mask & (1ULL << i))) continue;
        for (int t = 0; t < nfa_trans_count; t++) {
            if (strcmp(nfa_trans[t].from, nfa_states[i]) == 0 &&
                strcmp(nfa_trans[t].sym,  nfa_symbols[si]) == 0) {
                int j = nfa_state_idx(nfa_trans[t].to);
                if (j >= 0) result |= (1ULL << j);
            }
        }
    }
    return result;
}

static int find_dfa_state(Mask mask) {
    for (int i = 0; i < dfa_count; i++)
        if (dfa[i].mask == mask) return i;
    return -1;
}

/* Build a readable name like "{q0,q1}" for a DFA state */
static void mask_to_name(Mask mask, char *buf, int buflen) {
    buf[0] = '\0';
    strncat(buf, "{", buflen-1);
    int first = 1;
    for (int i = 0; i < nfa_state_count; i++) {
        if (mask & (1ULL << i)) {
            if (!first) strncat(buf, ",", buflen-1);
            strncat(buf, nfa_states[i], buflen-1);
            first = 0;
        }
    }
    strncat(buf, "}", buflen-1);
}

void convert_nfa_to_dfa(void) {
    /* workqueue of unprocessed DFA state indices */
    int queue[MAX_DFA_STATES];
    int head = 0, tail = 0;

    /* start state */
    int si = nfa_state_idx(nfa_start);
    if (si < 0) { fprintf(stderr, "Start state not found\n"); return; }
    Mask start_mask = (1ULL << si);

    /* create initial DFA state */
    dfa[0].mask     = start_mask;
    dfa[0].is_final = nfa_is_final(nfa_start);
    mask_to_name(start_mask, dfa[0].name, MAX_NAME);
    for (int s = 0; s < nfa_sym_count; s++) dfa[0].next[s] = -1;
    dfa_count = 1;
    queue[tail++] = 0;

    while (head < tail) {
        int cur = queue[head++];
        for (int s = 0; s < nfa_sym_count; s++) {
            Mask next_mask = move_set(dfa[cur].mask, s);
            if (next_mask == 0) { dfa[cur].next[s] = -1; continue; }

            int idx = find_dfa_state(next_mask);
            if (idx < 0) {
                /* new DFA state */
                idx = dfa_count++;
                dfa[idx].mask = next_mask;
                for (int ss = 0; ss < nfa_sym_count; ss++) dfa[idx].next[ss] = -1;
                mask_to_name(next_mask, dfa[idx].name, MAX_NAME);
                /* check if any NFA state in mask is a final state */
                dfa[idx].is_final = 0;
                for (int i = 0; i < nfa_state_count; i++)
                    if ((next_mask & (1ULL << i)) && nfa_is_final(nfa_states[i]))
                        dfa[idx].is_final = 1;
                queue[tail++] = idx;
            }
            dfa[cur].next[s] = idx;
        }
    }
}

/* ══════════════════════════════════════════════════════════════════
   Print DFA
   ══════════════════════════════════════════════════════════════════ */
void print_dfa(void) {
    printf("=== DFA (subset construction) ===\n");
    printf("States:\n");
    for (int i = 0; i < dfa_count; i++)
        printf("  %s%s%s\n", dfa[i].name,
               i == 0 ? " [start]" : "",
               dfa[i].is_final ? " [final]" : "");
    printf("Transitions:\n");
    for (int i = 0; i < dfa_count; i++)
        for (int s = 0; s < nfa_sym_count; s++)
            if (dfa[i].next[s] >= 0)
                printf("  %s --%s--> %s\n",
                       dfa[i].name, nfa_symbols[s], dfa[dfa[i].next[s]].name);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════
   Simulation
   ══════════════════════════════════════════════════════════════════ */
void simulate_nfa(const char *input) {
    /* BFS/bitset simulation */
    Mask cur = 0;
    int si = nfa_state_idx(nfa_start);
    if (si >= 0) cur = (1ULL << si);

    for (int i = 0; input[i]; i++) {
        char sym[2] = { input[i], '\0' };
        int s = nfa_sym_idx(sym);
        if (s < 0) { printf("NFA: Rejected (unknown symbol '%c')\n", input[i]); return; }
        cur = move_set(cur, s);
        if (cur == 0) { printf("NFA: Rejected\n"); return; }
    }
    /* accept if any current state is final */
    for (int i = 0; i < nfa_state_count; i++)
        if ((cur & (1ULL << i)) && nfa_is_final(nfa_states[i]))
            { printf("NFA: Accepted\n"); return; }
    printf("NFA: Rejected\n");
}

void simulate_dfa(const char *input) {
    if (dfa_count == 0) { printf("DFA: not built\n"); return; }
    int cur = 0;
    for (int i = 0; input[i]; i++) {
        char sym[2] = { input[i], '\0' };
        int s = nfa_sym_idx(sym);
        if (s < 0) { printf("DFA: Rejected (unknown symbol '%c')\n", input[i]); return; }
        cur = dfa[cur].next[s];
        if (cur < 0) { printf("DFA: Rejected\n"); return; }
    }
    printf("DFA: %s\n", dfa[cur].is_final ? "Accepted" : "Rejected");
}
