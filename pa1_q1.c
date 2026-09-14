/*
 * PA1 Q1 -- PSNE and very weakly dominant strategies for an n-player game.
 * Brute force in C, per the handout.
 *
 * Build: gcc -O2 -std=c11 -o pa1_q1 pa1_q1.c
 * Run:   ./pa1_q1 < input.txt
 *
 * Poojitha - architecture, I/O, output formatting
 * Akshith  - find_all_psne() (PSNE)
 * Kris     - find_vwds()    (very weakly dominant strategies)
 *
 * Input:  n / |S_1..n| / flat payoffs, Gambit NFG order (player 1's index
 *         varies fastest), each token an integer or p/q rational.
 * Output: npsne, then that many PSNE profiles (1-based, lexicographic in
 *         (s_1,...,s_n)), then per player a line: count + ascending
 *         1-based VWDS indices.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define OUT_BASE 1   /* output indices are 1-based; set to 0 for 0-based */

/* Payoffs are kept as exact reduced fractions (den > 0) and compared by
 * cross-multiplying in __int128, so we never fall into the trap of a
 * `double` silently losing precision on large or close-together payoffs. */
typedef struct { long long num, den; } Rat;

/* stand-in for -infinity, used to seed a running max */
static const Rat RAT_NEG_INF = { LLONG_MIN / 4, 1 };

static long long gcd_ll(long long a, long long b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { long long t = a % b; a = b; b = t; }
    return a == 0 ? 1 : a;
}

static Rat make_rat(long long num, long long den)
{
    if (den < 0) { den = -den; num = -num; }
    long long d = gcd_ll(num, den);
    Rat r; r.num = num / d; r.den = den / d;
    return r;
}

/* cross-multiply to compare exactly; __int128 gives plenty of headroom
 * since num/den never exceed ~9.2e18 in magnitude */
static int rat_cmp(Rat a, Rat b)
{
    __int128 lhs = (__int128)a.num * b.den;
    __int128 rhs = (__int128)b.num * a.den;
    return (lhs > rhs) - (lhs < rhs);
}

typedef struct {
    int          n;        /* number of players */
    int         *size;     /* size[i]   = |S_i| */
    long long   *stride;   /* stride[i] = |S_0| * ... * |S_{i-1}| */
    long long    NC;       /* number of contingencies = prod_i |S_i| */
    Rat         *P;        /* payoffs, length NC * n */
} Game;

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

/* contingency(s) = sum_i s[i] * stride[i]  (player 0 varies fastest);
 * payoff of player p at contingency c is P[c * n + p] */
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

/* contingency c with player p's strategy forced to 0 -- a unique,
 * in-range key for "the opponents' profile" */
static inline long long opp_key(long long c, const Game *g, int p)
{
    return c - strat_of(c, g, p) * g->stride[p];
}

/* odometer: player n-1 increments fastest, so this walks every profile in
 * lexicographic order of (s_0, ..., s_{n-1}). Note this is a *different*
 * order than the payoff array's contingency layout above. */
static int profile_next(int *s, const int *size, int n)
{
    for (int i = n - 1; i >= 0; i--) {
        if (++s[i] < size[i]) return 1;
        s[i] = 0;
    }
    return 0;
}

static Rat parse_number(const char *tok)
{
    const char *slash = strchr(tok, '/');
    if (slash) {
        char buf[64];
        size_t k = (size_t)(slash - tok);
        if (k >= sizeof buf) k = sizeof buf - 1;
        memcpy(buf, tok, k);
        buf[k] = '\0';
        long long num = strtoll(buf, NULL, 10);
        long long den = strtoll(slash + 1, NULL, 10);
        if (den == 0) {
            fprintf(stderr, "bad input: zero denominator in \"%s\"\n", tok);
            exit(1);
        }
        return make_rat(num, den);
    }
    return make_rat(strtoll(tok, NULL, 10), 1);
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

    long long total = g->NC * g->n;   /* <= 10^6 by constraints */
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

/* isBR[c] = 1 iff player p's strategy in contingency c earns him the
 * (exactly) maximum payoff available against that same opponent profile.
 * best_buf is caller-provided scratch of length >= NC.
 * Cost: O(NC) per player -> O(n * NC) <= O(10^6) overall. */
static void best_responses(const Game *g, int p, char *isBR, Rat *best_buf)
{
    const int       n  = g->n;
    const long long NC = g->NC;

    for (long long c = 0; c < NC; c++) best_buf[c] = RAT_NEG_INF;

    for (long long c = 0; c < NC; c++) {
        long long key = opp_key(c, g, p);
        Rat       v   = g->P[c * n + p];
        if (rat_cmp(v, best_buf[key]) > 0) best_buf[key] = v;
    }

    for (long long c = 0; c < NC; c++) {
        long long key = opp_key(c, g, p);
        isBR[c] = (rat_cmp(g->P[c * n + p], best_buf[key]) >= 0) ? 1 : 0;
    }
}

/* compute every player's best-response table once; player p's table is
 * the row at isBR_all + p * NC. Caller frees the returned pointer. */
static char *compute_all_isBR(const Game *g)
{
    const int       n  = g->n;
    const long long NC = g->NC;

    char *isBR_all  = xmalloc((size_t)n * (size_t)NC);
    Rat  *best_buf  = xmalloc((size_t)NC * sizeof *best_buf);

    for (int p = 0; p < n; p++)
        best_responses(g, p, isBR_all + (size_t)p * (size_t)NC, best_buf);

    free(best_buf);
    return isBR_all;
}

/* PSNE: a contingency where every player's isBR bit is set. Written out
 * as *count profiles of n ints each, in lexicographic order. Caller
 * frees *profiles. */
static void find_all_psne(const Game *g, const char *isBR_all,
                           int **profiles, long long *count)
{
    const int       n  = g->n;
    const long long NC = g->NC;

    int *brcount = xcalloc((size_t)NC, sizeof *brcount);
    for (int p = 0; p < n; p++) {
        const char *isBR = isBR_all + (size_t)p * (size_t)NC;
        for (long long c = 0; c < NC; c++) brcount[c] += isBR[c];
    }

    long long npsne = 0;
    for (long long c = 0; c < NC; c++)
        if (brcount[c] == n) npsne++;

    long long slots = npsne * n;
    int *out = xmalloc((size_t)(slots > 0 ? slots : 1) * sizeof *out);

    int *s = xcalloc((size_t)n, sizeof *s);
    long long w = 0;
    do {
        long long c = profile_to_index(s, g->stride, n);
        if (brcount[c] == n)
            for (int i = 0; i < n; i++) out[w++] = s[i];
    } while (profile_next(s, g->size, n));

    free(s); free(brcount);

    *profiles = out;
    *count    = npsne;
}

/* VWD: strategy k of player p is very weakly dominant iff it's a best
 * response to *every* opponent profile, i.e. hits[k] == opp. Takes
 * player p's precomputed isBR row instead of recomputing it. Caller
 * frees *idx. */
static void find_vwds(const Game *g, int p, const char *isBR, int **idx, int *count)
{
    const long long NC = g->NC;

    long long  opp  = NC / g->size[p];   /* number of opponent profiles */
    long long *hits = xcalloc((size_t)g->size[p], sizeof *hits);

    for (long long c = 0; c < NC; c++)
        hits[strat_of(c, g, p)] += isBR[c];

    int *res = xmalloc((size_t)g->size[p] * sizeof *res);
    int  m   = 0;
    for (int k = 0; k < g->size[p]; k++)
        if (hits[k] == opp) res[m++] = k;

    free(hits);

    *idx   = res;
    *count = m;
}

static void write_results(const Game *g, const char *isBR_all,
                          const int *psne, long long npsne)
{
    const long long NC = g->NC;

    printf("%lld\n", npsne);
    for (long long e = 0; e < npsne; e++) {
        for (int i = 0; i < g->n; i++)
            printf("%s%d", i ? " " : "", psne[e * g->n + i] + OUT_BASE);
        putchar('\n');
    }

    for (int p = 0; p < g->n; p++) {
        int *idx, cnt;
        find_vwds(g, p, isBR_all + (size_t)p * (size_t)NC, &idx, &cnt);
        printf("%d", cnt);
        for (int k = 0; k < cnt; k++)
            printf(" %d", idx[k] + OUT_BASE);
        putchar('\n');
        free(idx);
    }
}

int main(void)
{
    Game g;
    read_game(&g);

    char *isBR_all = compute_all_isBR(&g);

    int      *psne;
    long long npsne;
    find_all_psne(&g, isBR_all, &psne, &npsne);

    write_results(&g, isBR_all, psne, npsne);

    free(isBR_all);
    free(psne);
    free_game(&g);
    return 0;
}
