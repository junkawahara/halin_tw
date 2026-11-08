// SPDX-License-Identifier: MIT

// gen_halin: random Halin graph generator following Section 6 of
// arXiv:2506.02346.
//
//   gen_halin <Nmin> <Nmax> <seed> [-o out.gr]
//
// 1. N is drawn uniformly from [Nmin, Nmax].
// 2. A random recursive tree on N vertices: vertex i (2..N) is joined to a
//    uniformly random vertex in [1, i-1].
// 3. Every vertex of degree two receives an extra pendant vertex.
// 4. A DFS from vertex 1 lists the leaves; consecutive leaves (cyclically)
//    are joined, forming the leaf cycle.
// 5. Vertices are relabelled by a random permutation (edges are shuffled).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "../src/graph_io.hpp"

int main(int argc, char** argv) {
    std::string outPath;
    std::vector<const char*> pos;
    for (int a = 1; a < argc; ++a) {
        if (!std::strcmp(argv[a], "-o") && a + 1 < argc) outPath = argv[++a];
        else pos.push_back(argv[a]);
    }
    if (pos.size() != 3) {
        std::fprintf(stderr, "usage: %s <Nmin> <Nmax> <seed> [-o out.gr]\n", argv[0]);
        return 1;
    }
    long long nmin = std::atoll(pos[0]), nmax = std::atoll(pos[1]);
    unsigned long long seed = std::strtoull(pos[2], nullptr, 10);
    if (nmin < 3 || nmax < nmin) {
        std::fprintf(stderr, "error: need 3 <= Nmin <= Nmax\n");
        return 1;
    }
    std::mt19937_64 rng(seed);
    const int N = static_cast<int>(std::uniform_int_distribution<long long>(nmin, nmax)(rng));

    // random recursive tree (0-indexed: vertex i joins to [0, i-1])
    std::vector<std::vector<int>> adj(N);
    std::vector<std::pair<int, int>> edges;
    for (int i = 1; i < N; ++i) {
        int p = std::uniform_int_distribution<int>(0, i - 1)(rng);
        adj[i].push_back(p);
        adj[p].push_back(i);
        edges.emplace_back(p, i);
    }
    // eliminate degree-two vertices by attaching a pendant vertex
    const int base = N;
    for (int v = 0; v < base; ++v) {
        if (adj[v].size() == 2) {
            int u = static_cast<int>(adj.size());
            adj.emplace_back();
            adj[v].push_back(u);
            adj[u].push_back(v);
            edges.emplace_back(v, u);
        }
    }
    const int n = static_cast<int>(adj.size());

    // iterative DFS from vertex 0; collect leaves (degree one) in visiting order
    std::vector<int> leaves;
    {
        std::vector<char> seen(n, 0);
        std::vector<std::pair<int, size_t>> st;
        st.emplace_back(0, 0);
        seen[0] = 1;
        if (adj[0].size() == 1) leaves.push_back(0);
        while (!st.empty()) {
            auto& [v, k] = st.back();
            if (k == adj[v].size()) { st.pop_back(); continue; }
            int w = adj[v][k++];
            if (seen[w]) continue;
            seen[w] = 1;
            if (adj[w].size() == 1) leaves.push_back(w);
            st.emplace_back(w, 0);
        }
    }
    if (leaves.size() < 3) {
        std::fprintf(stderr, "error: generated tree has fewer than 3 leaves\n");
        return 1;
    }
    for (size_t k = 0; k < leaves.size(); ++k) edges.emplace_back(leaves[k], leaves[(k + 1) % leaves.size()]);

    // random relabelling, edge order and orientation
    std::vector<int> perm(n);
    for (int v = 0; v < n; ++v) perm[v] = v;
    std::shuffle(perm.begin(), perm.end(), rng);
    std::shuffle(edges.begin(), edges.end(), rng);

    FILE* out = outPath.empty() ? stdout : std::fopen(outPath.c_str(), "wb");
    if (!out) { std::fprintf(stderr, "error: cannot open %s\n", outPath.c_str()); return 1; }
    {
        halin::Writer w(out);
        w.put("c random Halin graph, N="); w.put(static_cast<long long>(N));
        w.put(" seed="); w.put(seed); w.put('\n');
        w.put("p tw "); w.put(static_cast<long long>(n)); w.put(' ');
        w.put(static_cast<long long>(edges.size())); w.put('\n');
        for (auto [a, b] : edges) {
            int x = perm[a] + 1, y = perm[b] + 1;
            if (rng() & 1) std::swap(x, y);
            w.put(static_cast<long long>(x)); w.put(' '); w.put(static_cast<long long>(y)); w.put('\n');
        }
    }
    if (!halin::finishOutput(out)) {
        std::fprintf(stderr, "error: cannot write %s\n", outPath.empty() ? "stdout" : outPath.c_str());
        return 1;
    }
    return 0;
}
