# PA_1_utilixGamers — Programming Assignment 1 (IGT, Monsoon 2026)

**Question 1:** Given an *n*-player game (Gambit NFG payoff format), list all pure-strategy
Nash equilibria (PSNE) and the very weakly dominant strategies for each player.
Brute force → implemented in C.

## Build & run

```sh
gcc -O2 -std=c11 -o pa1_q1 pa1_q1.c
./pa1_q1 < input.txt
```

## Input

```
line 1 : n
line 2 : |S_1| |S_2| ... |S_n|
rest   : flat whitespace-separated payoff list in Gambit NFG order
         (per contingency all n payoffs together; player 1's index varies fastest;
          integers or rationals p/q)
```

## Output

```
line 1      : npsne
next npsne  : n space-separated 1-based strategy indices, one PSNE per line,
              in lexicographic order of (s_1, ..., s_n)
next n lines: for player i, count of very weakly dominant strategies then their
              1-based ascending indices
```

## Example (Prisoner's Dilemma)

```
2
2 2
3 3  5 0  0 5  1 1
```
→
```
1
2 2
1 2
1 2
```

## Constraints handled

`n · ∏|S_i| ≤ 10⁶`, 60 s, 1 GB. Core is `O(n · ∏|S_i|)`.

## Group split

| Member | Responsibility | Code |
|---|---|---|
| 1 | Architecture, memory, I/O, profile↔index mapping, output formatting | `read_game`, `xmalloc`/`xcalloc`, `profile_to_index`, `write_results` |
| 2 | PSNE computation | `find_all_psne` |
| 3 | Very weakly dominant strategies | `find_vwds` |

Both solvers build on the shared `best_responses()` kernel.
