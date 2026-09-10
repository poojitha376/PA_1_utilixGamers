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
         - numbers may be integers or rationals of the form  p/q
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
| Member 2 | PSNE computation — `find_all_psne()` | ⬜ scaffold present (working brute force), to be reviewed/finalised |
| Member 3 | Very weakly dominant strategies — `find_vwds()` | ⬜ scaffold present (working brute force), to be reviewed/finalised |

> Note: so that the program compiles and runs end-to-end from day one, Poojitha
> put working brute-force versions of `find_all_psne()` and `find_vwds()` in place.
> Members 2 and 3 should review, optimise, document, and take ownership of those
> functions (or replace them).

---

## What Poojitha did (Member 1 — Architecture, Memory, I/O)

- **Set up the repository** and the single-file C project `pa1_q1.c`, plus this
  README and a `.gitignore` (keeps the compiled `pa1_q1` binary and `*.o` /
  `*.out` files out of git).
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
  - `parse_number()` accepts plain integers, decimals, and rationals written as
    `p/q`
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
  to that contingency's opponent profile. Both `find_all_psne()` and
  `find_vwds()` are written on top of this, so the three group members share one
  clean interface. Runs in `O(NC)` per player.
- **Output formatting** (`write_results()`) — prints the strict format above:
  `npsne`, then the PSNE profiles (1-based, lexicographic), then one line per
  player with the VWDS count and indices. Indexing base is a single
  `#define OUT_BASE 1` at the top of the file (flip to `0` if the grader wants
  0-based output).
- **Testing** — verified against Prisoner's Dilemma, Matching Pennies (no PSNE,
  no VWDS), a pure-coordination game (two PSNE), an all-equal-payoff game (every
  profile a PSNE, every strategy VWDS), a 3-player matching game, a 1-player
  game, games with a forced (size-1) player, rational-number payoffs, and a
  random `700×700` max-size instance for performance.

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
5. Comparisons use `EPS = 1e-9` slack — safe because every PSNE/VWDS condition is
   non-strict (`≥`), and it absorbs floating-point error from `p/q` payoffs.

---

## Instructions for teammates working next

### 0. One-time setup

Ask Poojitha to add you as a collaborator (GitHub → repo → **Settings → Collaborators**),
then:

```sh
git clone https://github.com/poojitha376/PA_1_utilixGamers.git
cd PA_1_utilixGamers
gcc -O2 -std=c11 -o pa1_q1 pa1_q1.c   # confirm it builds before you change anything
```

### 1. Before you start each session

```sh
git pull
```

### 2. While you work

- Work **only in your function(s)**:
  - **Member 2** → `find_all_psne()`
  - **Member 3** → `find_vwds()`
- Do **not** change `read_game()`, the `Game` struct, the index-mapping
  utilities, or `write_results()` without telling the group first — those are the
  shared interface.
- You may use the shared helpers: `best_responses()`, `strat_of()`,
  `opp_key()`, `profile_to_index()`, `profile_next()`, `xmalloc()`, `xcalloc()`.
- Keep the language C and keep it brute-force (per the handout).
- Rebuild and re-test after every change:
  ```sh
  gcc -O2 -std=c11 -Wall -Wextra -o pa1_q1 pa1_q1.c
  ./pa1_q1 < tests/pd.txt        # add your own test files under tests/
  ```
- Do not commit the compiled `pa1_q1` binary (`.gitignore` already blocks it).

### 3. Update THIS README with what you did

Add a section just like Poojitha's, in **points**, immediately below the last
member's section. Template:

```markdown
## What <Your Name> did (Member N — <your responsibility>)

- <point>
- <point>
- ...

### Assumptions / notes
- ...

## Instructions for the next teammate
- <anything the next person needs to know before touching your code>
```

Then move your row in the **Group split** table from ⬜ to ✅.

### 4. Commit and push

```sh
git add pa1_q1.c README.md tests/
git commit -m "Member N: <short description of what you did>"
git push
```

If `git push` is rejected because someone else pushed first:

```sh
git pull --rebase
# fix any conflict markers in pa1_q1.c, then:
git rebase --continue
git push
```

### 5. Keep a running changelog at the bottom of this README

Add one line per push:

```
## Changelog
- 2026-09-10  Poojitha  — repo + Member 1 (architecture, memory, I/O) + brute-force scaffolds for the two solvers
```

---

## Changelog

- **2026-09-10 — Poojitha** — created the repo; implemented Member 1
  (architecture, `Game` model, `read_game`, dynamic allocation, index-mapping
  utilities, `best_responses` kernel, `write_results`); added working
  brute-force `find_all_psne` / `find_vwds` so the program runs end-to-end;
  tested against 10+ cases including a max-size performance run.
