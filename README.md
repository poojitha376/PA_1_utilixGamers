# PA_1_utilixGamers — Programming Assignment 1 (IGT, Monsoon 2026)

**Question chosen: Q1 —** given an *n*-player game in Gambit NFG payoff format, list
**all pure-strategy Nash equilibria (PSNE)** and the **very weakly dominant
strategies (VWDS)** for every player.

Solved by **brute force**, so per the handout the whole program is written in **C**.

---

## Build & run

```sh
gcc -O2 -std=c11 -o pa1_q1 pa1_q1.c
./pa1_q1 < input.txt
```

## Input format

```
line 1 : n                              number of players
line 2 : |S_1| |S_2| ... |S_n|          size of each player's strategy space
rest   : payoffs as a flat whitespace-separated list, in Gambit NFG order:
         - for every strategy profile (contingency) the n player payoffs
           are listed together
         - profiles are ordered with player 1's strategy index varying
           fastest, then player 2, and so on
         - numbers are integers or rationals of the form  p/q
```

## Output format

```
line 1       : npsne
next npsne   : n space-separated 1-based strategy indices, one PSNE per line,
               in lexicographic order of (s_1, ..., s_n)
next n lines : for player i, the count of very weakly dominant strategies
               followed by their 1-based indices in ascending order
```

## Worked example — Prisoner's Dilemma

Input:
```
2
2 2
3 3  5 0  0 5  1 1
```
Output:
```
1
2 2
1 2
1 2
```
(The only PSNE is (Defect, Defect) = profile `2 2`; Defect = strategy `2` is very
weakly dominant for both players.)

## Constraints handled

`n · ∏|S_i| ≤ 10^6`, 60 s per test, 1 GB memory.
Core algorithm is `O(n · ∏|S_i|)` — the max-size input (~10^6 payoff entries)
runs in well under a tenth of a second.

---

## Group split

| Member | Responsibility | Status |
|--------|----------------|--------|
| **Poojitha** | Architecture, memory, I/O, profile↔index mapping, output formatting | ✅ Done |
| **Akshith** | PSNE computation — `find_all_psne()` | ✅ Done |
| **Kris** | Very weakly dominant strategies — `find_vwds()` | ✅ Done |

---

## What Poojitha did (Member 1 — Architecture, Memory, I/O)

- **Set up the repository** and the single-file C project `pa1_q1.c`, plus this
  README and a `.gitignore` (keeps the compiled `pa1_q1` binary and build
  artifacts from the Lean/Python verification tooling out of git).
- **Designed the data model** — the `Game` struct that everything hangs off:
  - `n` — number of players
  - `size[i]` — `|S_i|`
  - `stride[i]` — `|S_0| · |S_1| · … · |S_{i-1}|`, precomputed once
  - `NC` — number of contingencies = `∏_i |S_i|`
  - `P` — the payoff array, length `NC · n`
- **Input parsing** (`read_game()`):
  - reads `n` and validates it is positive
  - reads the `n` strategy-space sizes, and builds `stride[]` and `NC` in the
    same pass
  - reads exactly `NC · n` payoff tokens; errors out clearly if the count is
    wrong
  - `parse_number()` accepts plain integers and rationals written as `p/q`
- **Dynamic memory** — everything is `malloc` / `calloc` on the heap through the
  `xmalloc()` / `xcalloc()` wrappers (which abort with a message on
  out-of-memory), so there are **no large stack objects and no stack-overflow
  risk** even at the `10^6` limit. `free_game()` releases it all.
- **The index-mapping utilities** (the core maths that lets a brute-force loop
  address the flat NFG array):
  - `profile_to_index(s, stride, n)` — turns a strategy profile
    `(s_0,…,s_{n-1})` into its 1D contingency index `∑_i s[i]·stride[i]`
  - `strat_of(c, g, p)` — extracts player `p`'s strategy from contingency `c`:
    `(c / stride[p]) % size[p]`
  - `opp_key(c, g, p)` — contingency `c` with player `p`'s strategy forced to 0;
    a unique, in-range key for "the opponents' profile", used by both solvers
  - `profile_next(s, size, n)` — odometer that walks every profile in
    lexicographic order of `(s_1,…,s_n)` (player `n-1` changes fastest)
  - payoff accessor convention: player `p` at contingency `c` is `P[c·n + p]`
- **The shared solver kernel** `best_responses(g, p, isBR, best_buf)` — fills
  `isBR[c] = 1` iff player `p`'s strategy in contingency `c` is a best response
  to that contingency's opponent profile, computed once per player via
  `compute_all_isBR()` and reused by both `find_all_psne()` and
  `find_vwds()`, so the three group members share one clean interface and the
  best-response table is never computed twice. Runs in `O(NC)` per player.
- **Output formatting** (`write_results()`) — prints the strict format above:
  `npsne`, then the PSNE profiles (1-based, lexicographic), then one line per
  player with the VWDS count and indices. Indexing base is a single
  `#define OUT_BASE 1` at the top of the file (flip to `0` if the grader wants
  0-based output).

### Key assumptions baked in (flag to the group / TA if any are wrong)

1. After the sizes line, the input is just the flat payoff numbers — no Gambit
   `NFG 1 R "..." { ... } { ... }` header (the handout already gives `n` and the
   sizes separately).
2. NFG ordering: player 1's strategy index varies fastest; all `n` payoffs for a
   contingency are listed consecutively.
3. Output strategy indices are **1-based**.
4. "Very weakly dominant" = strategy `k` with `u_p(k, a) ≥ u_p(t, a)` for **all**
   alternatives `t` and **all** opponent profiles `a` (equivalently: `k` is a
   best response to every opponent profile). Several strategies can qualify;
   a player with only one strategy trivially qualifies.
5. Comparisons are **exact** — payoffs are exact reduced fractions compared by
   cross-multiplication, no floating point and no epsilon anywhere in the
   decision path.

---

## What Akshith did (Member 2 — PSNE)

- Reviewed and took ownership of `find_all_psne()`: a contingency is a PSNE
  iff every player's bit in `compute_all_isBR()`'s best-response table is
  set (`brcount[c] == n`); matching profiles are collected in the order the
  `profile_next` odometer visits them, i.e. lexicographic in `(s_1,…,s_n)`,
  which is the order the output format requires.


## What Kris did (Member 3 — VWDS)

- Reviewed and took ownership of `find_vwds()`: for player `p`, strategy `k`
  is very weakly dominant iff `hits[k] == opp` — i.e. `k` was a best
  response in *every* one of the `opp = NC / size[p]` opponent profiles it
  appeared in, which is exactly "best response to every opponent profile"
  ⇔ very weakly dominant.
-
---

