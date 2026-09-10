/* ==========================================================================
 * Programming Assignment 1  --  Question 1
 * PSNE and Very Weakly Dominant Strategies for an n-player game.
 *
 * Solved by brute force  ->  implemented in C (as required by the handout).
 * Build:   gcc -O2 -std=c11 -o pa1_q1 pa1_q1.c
 * Run:     ./pa1_q1 < input.txt
 *
 * Group split
 *   Member 1  (this scaffold) : Architecture, memory, I/O
 *       - parse n and the strategy-space sizes |S_i|
 *       - dynamically allocate the payoff array (malloc, no big stack objects)
 *       - profile <-> 1D index mapping for the NFG layout
 *       - strict output formatting for PSNE and VWDS
 *   Member 2 : find_all_psne()      -- pure-strategy Nash equilibria
 *   Member 3 : find_vwds()          -- very weakly dominant strategies
 *
 * Both solver routines are built on best_responses(), so the three parts
 * share one well-defined interface (the Game struct + the BR table).
 * ==========================================================================
 *
 * INPUT  (as per the handout)
 *   line 1 : n                          number of players
 *   line 2 : |S_1| |S_2| ... |S_n|      size of each player's strategy space
 *   rest   : payoffs in Gambit NFG "payoff" layout, i.e. a flat whitespace-
 *            separated list.  For every strategy profile (contingency) the
 *            n player payoffs are listed together; profiles are ordered with
 *            player 1's strategy index varying fastest, then player 2, etc.
 *            Numbers may be integers or rationals of the form  p/q.
 *
 * OUTPUT
 *   line 1        : npsne
 *   next npsne    : n space-separated strategy indices (1-based), one PSNE per
 *                   line, in lexicographic order of (s_1, ..., s_n)
 *   next n lines  : for player i, the count of very weakly dominant strategies
 *                   followed by their (1-based, ascending) indices
 * ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Output strategy indices are 1-based (change to 0 for 0-based). */
#define OUT_BASE 1

/* Tolerance for "is a best response" comparisons.  All PSNE / very-weak
 * conditions are non-strict (>=), so a small slack here is safe and also
 * absorbs the rounding error from rational (p/q) payoffs. */
#define EPS 1e-9

/* ------------------------------------------------------------------ *
 * Member 1 : data model
 * ------------------------------------------------------------------ */
typedef struct {
    int          n;        /* number of players                            */
    int         *size;     /* size[i]   = |S_i|                            */
    long long   *stride;   /* stride[i] = |S_0| * ... * |S_{i-1}|          */
    long long    NC;       /* number of contingencies = prod_i |S_i|      */
    double      *P;        /* payoffs, length NC * n  (row-major on c, p)  */
} Game;

/* ------------------------------------------------------------------ *
 * Member 1 : allocation helpers (report and abort on OOM)
 * ------------------------------------------------------------------ */
static void *xmalloc(size_t bytes)
{
    void *p = malloc(bytes ? bytes : 1);
    if (!p) { fprintf(stderr, "out of memory (%zu bytes)\n", bytes); exit(1); }
    return p;
}

static void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count ? count : 1, size ? size : 1);
    if (!p) { fprintf(stderr, "out of memory (%zu x %zu)\n", count, size); exit(1); }
    return p;
}

/* ------------------------------------------------------------------ *
 * Member 1 : index mapping between a strategy profile and the flat
 *            NFG contingency index.
 *
 *   contingency(s) = sum_i  s[i] * stride[i]        (player 0 varies fastest)
 *   payoff of player p at contingency c  =  P[c * n + p]
 * ------------------------------------------------------------------ */
static long long profile_to_index(const int *s, const long long *stride, int n)
{
    long long idx = 0;
    for (int i = 0; i < n; i++)
        idx += (long long)s[i] * stride[i];
    return idx;
}

/* strategy of player p inside contingency c */
static inline long long strat_of(long long c, const Game *g, int p)
{
    return (c / g->stride[p]) % g->size[p];
}

/* opponent key: contingency c with player p's strategy forced to 0.
 * Unique per opponent profile and always in [0, NC). */
static inline long long opp_key(long long c, const Game *g, int p)
{
    return c - strat_of(c, g, p) * g->stride[p];
}

/* Advance the profile odometer; player n-1 changes fastest so the overall
 * enumeration is lexicographic in (s_0, s_1, ..., s_{n-1}).
 * Returns 0 once the profile wraps back to all zeros. */
static int profile_next(int *s, const int *size, int n)
{
    for (int i = n - 1; i >= 0; i--) {
        if (++s[i] < size[i]) return 1;
        s[i] = 0;
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Member 1 : input
 * ------------------------------------------------------------------ */
static double parse_number(const char *tok)
{
    const char *slash = strchr(tok, '/');
    if (slash) {                       /* rational  p/q  */
        char buf[64];
        size_t k = (size_t)(slash - tok);
        if (k >= sizeof buf) k = sizeof buf - 1;
        memcpy(buf, tok, k);
        buf[k] = '\0';
        double num = strtod(buf, NULL);
        double den = strtod(slash + 1, NULL);
        return den != 0.0 ? num / den : num;
    }
    return strtod(tok, NULL);
}

static void read_game(Game *g)
{
    if (scanf("%d", &g->n) != 1 || g->n <= 0) {
        fprintf(stderr, "bad input: number of players\n");
        exit(1);
    }

    g->size   = xmalloc((size_t)g->n * sizeof *g->size);
    g->stride = xmalloc((size_t)g->n * sizeof *g->stride);

    g->NC = 1;
    for (int i = 0; i < g->n; i++) {
        if (scanf("%d", &g->size[i]) != 1 || g->size[i] <= 0) {
            fprintf(stderr, "bad input: |S_%d|\n", i + 1);
            exit(1);
        }
        g->stride[i] = g->NC;
        g->NC       *= g->size[i];
    }

    long long total = g->NC * g->n;                 /* <= 10^6 by constraints */
    g->P = xmalloc((size_t)total * sizeof *g->P);

    for (long long k = 0; k < total; k++) {
        char tok[64];
        if (scanf("%63s", tok) != 1) {
            fprintf(stderr, "bad input: expected %lld payoffs, got %lld\n",
                    total, k);
            exit(1);
        }
        g->P[k] = parse_number(tok);
    }
}

static void free_game(Game *g)
{
    free(g->size); free(g->stride); free(g->P);
}

/* ------------------------------------------------------------------ *
 * Shared kernel : best-response table for one player.
 *
 * isBR[c] = 1  iff  player p's strategy in contingency c earns him the
 * maximum payoff available against that same opponent profile.
 *
 * best_buf is caller-provided scratch of length >= NC.
 * Cost: O(NC) per player  ->  O(n * NC) <= O(10^6) overall.
 * ------------------------------------------------------------------ */
static void best_responses(const Game *g, int p, char *isBR, double *best_buf)
{
    const int       n  = g->n;
    const long long NC = g->NC;

    for (long long c = 0; c < NC; c++) best_buf[c] = -INFINITY;

    for (long long c = 0; c < NC; c++) {
        long long key = opp_key(c, g, p);
        double    v   = g->P[c * n + p];
        if (v > best_buf[key]) best_buf[key] = v;
    }

    for (long long c = 0; c < NC; c++) {
        long long key = opp_key(c, g, p);
        isBR[c] = (g->P[c * n + p] >= best_buf[key] - EPS) ? 1 : 0;
    }
}

/* ------------------------------------------------------------------ *
 * Member 2 : all pure-strategy Nash equilibria.
 *
 * A contingency is a PSNE iff every player is simultaneously best-
 * responding.  Results are written as *count profiles of n ints each,
 * in lexicographic order of (s_1, ..., s_n).  Caller frees *profiles.
 * ------------------------------------------------------------------ */
static void find_all_psne(const Game *g, int **profiles, long long *count)
{
    const int       n  = g->n;
    const long long NC = g->NC;

    char   *isBR     = xmalloc((size_t)NC);
    double *best_buf = xmalloc((size_t)NC * sizeof *best_buf);
    int    *brcount  = xcalloc((size_t)NC, sizeof *brcount);

    for (int p = 0; p < n; p++) {
        best_responses(g, p, isBR, best_buf);
        for (long long c = 0; c < NC; c++) brcount[c] += isBR[c];
    }

    /* first pass: how many equilibria */
    long long npsne = 0;
    for (long long c = 0; c < NC; c++)
        if (brcount[c] == n) npsne++;

    long long slots = npsne * n;
    int *out = xmalloc((size_t)(slots > 0 ? slots : 1) * sizeof *out);

    /* second pass: emit profiles in lexicographic order */
    int *s = xcalloc((size_t)n, sizeof *s);
    long long w = 0;
    do {
        long long c = profile_to_index(s, g->stride, n);
        if (brcount[c] == n)
            for (int i = 0; i < n; i++) out[w++] = s[i];
    } while (profile_next(s, g->size, n));

    free(s); free(isBR); free(best_buf); free(brcount);

    *profiles = out;
    *count    = npsne;
}

/* ------------------------------------------------------------------ *
 * Member 3 : very weakly dominant strategies for one player.
 *
 * Strategy k of player p is very weakly dominant iff it is a best
 * response to *every* opponent profile, i.e. u_p(k, a) >= u_p(t, a)
 * for all t and all a_{-p}.  Caller frees *idx.
 * ------------------------------------------------------------------ */
static void find_vwds(const Game *g, int p, int **idx, int *count)
{
    const long long NC = g->NC;

    char   *isBR     = xmalloc((size_t)NC);
    double *best_buf = xmalloc((size_t)NC * sizeof *best_buf);
    best_responses(g, p, isBR, best_buf);

    long long  opp  = NC / g->size[p];                 /* # opponent profiles */
    long long *hits = xcalloc((size_t)g->size[p], sizeof *hits);

    for (long long c = 0; c < NC; c++)
        hits[strat_of(c, g, p)] += isBR[c];

    int *res = xmalloc((size_t)g->size[p] * sizeof *res);
    int  m   = 0;
    for (int k = 0; k < g->size[p]; k++)
        if (hits[k] == opp) res[m++] = k;              /* BR everywhere */

    free(hits); free(isBR); free(best_buf);

    *idx   = res;
    *count = m;
}

/* ------------------------------------------------------------------ *
 * Member 1 : output
 * ------------------------------------------------------------------ */
static void write_results(const Game *g,
                          const int *psne, long long npsne)
{
    printf("%lld\n", npsne);
    for (long long e = 0; e < npsne; e++) {
        for (int i = 0; i < g->n; i++)
            printf("%s%d", i ? " " : "", psne[e * g->n + i] + OUT_BASE);
        putchar('\n');
    }

    for (int p = 0; p < g->n; p++) {
        int *idx, cnt;
        find_vwds(g, p, &idx, &cnt);
        printf("%d", cnt);
        for (int k = 0; k < cnt; k++)
            printf(" %d", idx[k] + OUT_BASE);
        putchar('\n');
        free(idx);
    }
}

/* ------------------------------------------------------------------ */
int main(void)
{
    Game g;
    read_game(&g);

    int      *psne;
    long long npsne;
    find_all_psne(&g, &psne, &npsne);

    write_results(&g, psne, npsne);

    free(psne);
    free_game(&g);
    return 0;
}
