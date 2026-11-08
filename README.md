# halin_tw — Linear-time optimal tree decompositions of Halin graphs

A C++17 implementation of **H-Td**, the linear-time algorithm for computing an
optimal-width (width 3) tree decomposition of a Halin graph described in

> J. A. Alejandro-Soto, J. A. Trejo-Sanchez, C. Segura.
> *A Practical Linear Time Algorithm for Optimal Tree Decomposition of Halin Graphs.*
> arXiv:2506.02346, 2025.

The input graph is first split into its skeleton tree and leaf cycle with
the linear-time reduction-based recognition algorithm of

> D. Eppstein. *Simple Recognition of Halin Graphs and Their Generalizations.*
> Journal of Graph Algorithms and Applications 20(2), 2016. arXiv:1502.05334.

which is implemented here as well. The program therefore takes an arbitrary
graph, decides whether it is a Halin graph, and if so outputs a width-3 tree
decomposition. Input and output use the
[PACE 2017](https://pacechallenge.org/2017/treewidth/) `.gr` / `.td` formats.

## Contents

| Path | Description |
|---|---|
| `src/eppstein.{hpp,cpp}` | Halin recognition with the D3a/D3b reductions and recovery of the tree/cycle decomposition |
| `src/htd.{hpp,cpp}` | H-Td: adjacency order (Prop. 1, Appendix A) and the AH-Td recursion (Section 5) |
| `src/halin_td.cpp` | Command-line program: `.gr` in, `.td` out |
| `src/graph_io.hpp` | Fast PACE `.gr` reader and buffered writer |
| `tools/gen_halin.cpp` | Random Halin graph generator (the instance generator of Section 6 of the paper) |
| `tools/td_validate.cpp` | Independent tree-decomposition validator (checks C1–C3, tree structure and width) |
| `tools/test_recognition.cpp` | Randomized brute-force cross-check of the recognizer and H-Td on small graphs |
| `scripts/run_limited.sh` | Runs a command with a 32 GB memory limit and a 10-minute timeout |
| `scripts/benchmark.sh` | Reproduces the Small/Medium/Large/Giant experiment of the paper |

## Building

Requires a C++17 compiler (tested with GCC 15.2) and `make`.

```sh
make            # builds bin/halin_td, bin/gen_halin, bin/td_validate, bin/test_recognition
make test       # randomized cross-check against brute force
```

## Usage

```sh
bin/halin_td [-o out.td] [--stats] [input.gr]
```

* Reads the graph from `input.gr` (or stdin) and writes the decomposition to
  `out.td` (or stdout).
* The PACE format allows self-loops and repeated edges. They do not change
  the tree decompositions of a graph, so `halin_td` removes them and works on
  the resulting simple graph. Its decomposition is also a tree decomposition
  of the input.
* `--stats` prints sizes, the number of removed self-loops and repeated
  edges, the number of reductions and timings to stderr.
* Exit status: `0` success, `1` usage or I/O error, `2` the input is not a
  Halin graph (after removing self-loops and repeated edges).

Example:

```sh
bin/gen_halin 1000 9999 42 -o g.gr          # random Halin graph, N uniform in [1000, 9999]
bin/halin_td --stats -o g.td g.gr
bin/td_validate g.gr g.td --expect-width 3  # prints "valid width=3 bags=..."
```

### File formats

Input (`.gr`, vertices numbered from 1):

```
c optional comment
p tw <n> <m>
<u> <v>
...
```

Output (`.td`):

```
s td <number of bags> <max bag size> <n>
b <bag id> <vertex> <vertex> ...
...
<bag id> <bag id>        # edges of the decomposition tree
...
```

## Algorithm overview

1. **Recognition and decomposition (Eppstein).** Degree-3 configurations are
   reduced repeatedly:
   * **D3a**: a triangle of degree-3 vertices with three distinct outside
     neighbours is contracted to one vertex;
   * **D3b**: an induced path `p–q–r` of degree-3 vertices with a common
     neighbour (apex) `s` is shortened by deleting `q` and adding `p–r`.

   A set `K` of vertices that must lie on the leaf cycle forbids reductions
   that are impossible in a Halin graph. The input is Halin if and only if the
   process ends in `K4` with a vertex outside `K`. Undoing the reductions in
   reverse order recovers a leaf set. Edges between two leaves form the leaf
   cycle and all other edges form the skeleton.
2. **Adjacency order.** The skeleton is rooted at an internal vertex `r`.
   Starting from a leaf `v` adjacent on the cycle to a leaf `w` with
   `lca(v, w) = r`, the cycle is traversed to assign the x-values. Then the
   children of every vertex are ordered so that a DFS visits the leaves in
   cycle order.
3. **AH-Td.** `AH-Td(2, r, 1, deg_c(r))` builds the decomposition by splitting
   each Almost Halin subgraph `AH_t(v, i, j)` into `AH_1(v, i, i)` and
   `AH_t(v, i+1, j)`, or by descending into the child when `i = j`. All bags
   have at most four vertices.

Every step runs in O(n) time.

### Implementation notes

* The AH-Td recursion and all tree traversals use explicit stacks, so the
  program does not overflow the call stack on very deep skeletons. A
  caterpillar of depth 500,000 is processed in well under a second.
* In the paper, node `A` (or `B`) of a call is "identified with" node `R` of
  the invoked call. Here the child call simply receives `A` (or `B`) as its
  `R`, so no bag is duplicated.
* Repeated vertices inside a bag are removed. For example, in the `i = j`
  leaf case of `AH_2`, `omega_2(v, j)` equals the leaf itself.
* Before running AH-Td, `halinTreeDecomposition` checks that the given
  tree/cycle pair is valid: the skeleton is a tree without degree-2 vertices,
  the leaf cycle is a single cycle, and the leaves of every subtree are
  contiguous on the cycle.
* The dynamic graph used by the reductions stores edge ids with
  back-pointers, so edge insertion and deletion take O(1) time even at
  high-degree vertices. Adjacency tests always scan the list of a degree-3
  vertex.

## Testing

`bin/test_recognition [iterations] [seed]` compares the recognizer against a
brute-force characterization of Halin graphs on small graphs (up to 16
vertices). The test graphs are random Halin graphs, Halin graphs with one
edge added, removed or rewired, and random graphs of minimum degree 3. It
also checks some fixed graphs: K4, K5, K3,3, the cube, the prism, the
Petersen graph, the truncated tetrahedron, and wheels. For every accepted
graph it checks that the recovered leaf set is a valid Halin decomposition and
that the H-Td output is a tree decomposition of width 3.

```
$ bin/test_recognition 30000 7
cases: halin/accepted=8253 halin/rejected=0 nonhalin/accepted=0 nonhalin/rejected=11754
ALL OK (0 failures)
```

The test suite also passes under `-fsanitize=address,undefined`.

## Benchmark

`scripts/benchmark.sh [instances-per-group]` generates instances exactly as in
Section 6 of the paper. N is drawn uniformly from [10², 10³−1] (Small),
[10³, 10⁴−1] (Medium), [10⁴, 10⁵−1] (Large) or [10⁵, 10⁶−1] (Giant).
The script runs `halin_td` on every instance and validates each output with
`td_validate`. All runs go through `scripts/run_limited.sh` (32 GB memory, 10 minutes).
If any output fails validation, the script keeps that `.td` file, prints the
validator's message and exits with status 1 after all instances have run.

Results for 10 instances per group on an Intel Core i3-1115G4 (3.0 GHz),
GCC 15.2 with `-O2`. Times are averages in seconds. *Recognition* is
Eppstein's algorithm and *H-Td* is the adjacency order plus AH-Td. *Wall* is
the whole process, including reading and writing files.

| Group  | avg. n  | Recognition | H-Td     | Wall   | Valid, width 3 |
|--------|--------:|------------:|---------:|-------:|:--------------:|
| Small  |     820 | 0.000307    | 0.000085 | 0.0078 | 10/10 |
| Medium |   7,182 | 0.002760    | 0.000799 | 0.0131 | 10/10 |
| Large  |  72,555 | 0.040089    | 0.009893 | 0.0805 | 10/10 |
| Giant  | 794,397 | 1.015540    | 0.359259 | 1.6737 | 10/10 |

The time per vertex grows slowly with n, from about 0.9 to 1.4 µs per vertex
for recognition. This comes from memory locality, not from the algorithm. The
generator relabels vertices randomly, so memory accesses become random once
the graph no longer fits in the CPU cache. The same 10⁶-vertex caterpillar
takes 0.19 s for recognition with sequential labels and 0.49 s after a random
relabelling. The number of operations is linear in both cases.

## References

* J. A. Alejandro-Soto, J. A. Trejo-Sanchez, C. Segura. A Practical Linear
  Time Algorithm for Optimal Tree Decomposition of Halin Graphs.
  arXiv:2506.02346, 2025.
* D. Eppstein. Simple Recognition of Halin Graphs and Their Generalizations.
  JGAA 20(2):323–346, 2016. arXiv:1502.05334.
* PACE 2017 Track A: Treewidth. https://pacechallenge.org/2017/treewidth/

This repository is an independent implementation written from the papers.
It is not the authors' code.

## License

Released under the MIT License. See [LICENSE](LICENSE) for the full text.

Copyright (c) 2026 Jun Kawahara
