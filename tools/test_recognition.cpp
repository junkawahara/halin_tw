// SPDX-License-Identifier: MIT

// test_recognition: randomized cross-check of the Eppstein recognizer and of
// H-Td against brute force on small graphs.
//
//   test_recognition [iterations] [seed]
//
// Brute force: G is Halin iff some vertex subset L satisfies
//   * the edges inside L form one cycle through all of L (|L| >= 3),
//   * the remaining edges form a spanning tree whose leaves are exactly L
//     and which has no vertex of degree 2,
//   * for every tree edge, the leaves on either side are contiguous along the
//     cycle (=> the cycle follows a planar embedding of the tree).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../src/eppstein.hpp"
#include "../src/graph_io.hpp"
#include "../src/htd.hpp"

using Edges = std::vector<std::pair<int, int>>;

namespace {

// Checks whether leaf set (bitmask) gives a valid Halin decomposition.
bool validLeafSet(int n, const Edges& E, const std::vector<char>& L) {
    int nl = 0;
    for (int v = 0; v < n; ++v) nl += L[v];
    if (nl < 3) return false;
    std::vector<std::vector<int>> cyc(n), tree(n);
    for (auto [a, b] : E) {
        if (L[a] && L[b]) { cyc[a].push_back(b); cyc[b].push_back(a); }
        else { tree[a].push_back(b); tree[b].push_back(a); }
    }
    int te = 0;
    for (int v = 0; v < n; ++v) {
        te += static_cast<int>(tree[v].size());
        if (L[v] && (cyc[v].size() != 2 || tree[v].size() != 1)) return false;
        if (!L[v] && (!cyc[v].empty() || tree[v].size() < 3)) return false;
    }
    if (te / 2 != n - 1) return false;
    // tree connected
    std::vector<int> seen(n, 0), st{0};
    seen[0] = 1;
    int cnt = 1;
    while (!st.empty()) {
        int v = st.back(); st.pop_back();
        for (int w : tree[v]) if (!seen[w]) { seen[w] = 1; ++cnt; st.push_back(w); }
    }
    if (cnt != n) return false;
    // cycle is one cycle
    int s = 0;
    while (!L[s]) ++s;
    int prev = -1, cur = s, len = 0;
    do {
        int nxt = cyc[cur][0] == prev ? cyc[cur][1] : cyc[cur][0];
        prev = cur; cur = nxt; ++len;
    } while (cur != s && len <= n);
    if (len != nl) return false;
    // contiguity for each tree edge
    for (auto [a, b] : E) {
        if (L[a] && L[b]) continue;
        // side of a when removing edge a-b
        std::vector<char> side(n, 0);
        std::vector<int> stk{a};
        side[a] = 1;
        while (!stk.empty()) {
            int v = stk.back(); stk.pop_back();
            for (int w : tree[v]) {
                if ((v == a && w == b) || side[w]) continue;
                side[w] = 1; stk.push_back(w);
            }
        }
        int boundary = 0;
        for (auto [x, y] : E)
            if (L[x] && L[y] && side[x] != side[y]) ++boundary;
        if (boundary != 2) return false;
    }
    return true;
}

bool bruteHalin(int n, const Edges& E) {
    if (n < 4 || n > 20) return false;
    std::vector<char> L(n);
    for (long long mask = 0; mask < (1LL << n); ++mask) {
        for (int v = 0; v < n; ++v) L[v] = (mask >> v) & 1;
        if (validLeafSet(n, E, L)) return true;
    }
    return false;
}

// Small in-process tree decomposition validator.
std::string checkTd(int n, const Edges& E, const halin::TreeDecomposition& td) {
    int B = td.numNodes();
    if (td.maxBagSize() > 4) return "width > 3";
    std::vector<std::vector<int>> occ(n);
    int roots = 0;
    for (int t = 0; t < B; ++t) {
        if (td.parent[t] < 0) ++roots;
        else if (td.parent[t] >= t) return "parent index not smaller";  // creation order => tree
        for (int k = 0; k < td.bagSize[t]; ++k) occ[td.bags[t][k]].push_back(t);
    }
    if (roots != 1) return "not exactly one root";
    auto has = [&](int t, int v) {
        for (int k = 0; k < td.bagSize[t]; ++k) if (td.bags[t][k] == v) return true;
        return false;
    };
    for (int v = 0; v < n; ++v) {
        if (occ[v].empty()) return "C1 fails";
        int inner = 0;
        for (int t : occ[v]) if (td.parent[t] >= 0 && has(td.parent[t], v)) ++inner;
        if (inner != static_cast<int>(occ[v].size()) - 1) return "C3 fails";
    }
    for (auto [a, b] : E) {
        bool ok = false;
        for (int t : occ[a]) ok |= has(t, b);
        if (!ok) return "C2 fails";
    }
    return "";
}

// Random Halin graph (same construction as gen_halin) on a random tree.
Edges randomHalin(int N, std::mt19937_64& rng, int& n) {
    std::vector<std::vector<int>> adj(N);
    Edges E;
    for (int i = 1; i < N; ++i) {
        int p = std::uniform_int_distribution<int>(0, i - 1)(rng);
        adj[i].push_back(p); adj[p].push_back(i); E.emplace_back(p, i);
    }
    for (int v = 0; v < N; ++v)
        if (adj[v].size() == 2) {
            int u = static_cast<int>(adj.size());
            adj.emplace_back(); adj[v].push_back(u); adj[u].push_back(v); E.emplace_back(v, u);
        }
    n = static_cast<int>(adj.size());
    for (auto& l : adj) std::shuffle(l.begin(), l.end(), rng);  // random embedding
    std::vector<int> leaves;
    std::vector<char> seen(n, 0);
    std::vector<std::pair<int, size_t>> st{{0, 0}};
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
    for (size_t k = 0; k < leaves.size(); ++k) E.emplace_back(leaves[k], leaves[(k + 1) % leaves.size()]);
    return E;
}

Edges relabel(int n, Edges E, std::mt19937_64& rng) {
    std::vector<int> p(n);
    std::iota(p.begin(), p.end(), 0);
    std::shuffle(p.begin(), p.end(), rng);
    for (auto& [a, b] : E) { a = p[a]; b = p[b]; if (rng() & 1) std::swap(a, b); }
    std::shuffle(E.begin(), E.end(), rng);
    return E;
}

bool isSimple(int n, const Edges& E) {
    std::set<std::pair<int, int>> s;
    for (auto [a, b] : E) {
        if (a == b || a < 0 || b < 0 || a >= n || b >= n) return false;
        if (!s.insert({std::min(a, b), std::max(a, b)}).second) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    long long iters = 20000;
    unsigned long long seed = 1;
    if (argc > 3 || (argc > 1 && (!halin::parseLongArg(argv[1], iters) || iters < 0)) ||
        (argc > 2 && !halin::parseULongArg(argv[2], seed))) {
        std::fprintf(stderr, "usage: %s [iterations >= 0] [seed >= 0]\n", argv[0]);
        return 2;
    }
    std::mt19937_64 rng(seed);
    long long stats[2][2] = {{0, 0}, {0, 0}};  // [brute][algo]
    long long failures = 0;

    auto runCase = [&](int n, const Edges& E, const char* family) {
        bool brute = bruteHalin(n, E);
        halin::HalinRecognition rec = halin::recognizeHalin(n, E);
        ++stats[brute][rec.isHalin];
        if (brute != rec.isHalin) {
            ++failures;
            if (failures <= 10) {
                std::printf("MISMATCH (%s) brute=%d algo=%d reason=%s n=%d edges:", family, brute, rec.isHalin,
                            rec.reason.c_str(), n);
                for (auto [a, b] : E) std::printf(" %d-%d", a, b);
                std::printf("\n");
            }
            return;
        }
        if (!rec.isHalin) return;
        if (!validLeafSet(n, E, rec.isLeaf)) {
            ++failures;
            std::printf("BAD LEAF SET (%s) n=%d\n", family, n);
            return;
        }
        try {
            halin::TreeDecomposition td = halin::halinTreeDecomposition(n, E, rec.isLeaf);
            std::string err = checkTd(n, E, td);
            if (!err.empty()) { ++failures; std::printf("BAD TD (%s): %s n=%d\n", family, err.c_str(), n); }
            else if (td.maxBagSize() != 4) { ++failures; std::printf("BAD TD (%s): width != 3\n", family); }
        } catch (const std::exception& e) {
            ++failures;
            std::printf("TD EXCEPTION (%s): %s\n", family, e.what());
        }
    };

    // fixed graphs
    {
        Edges k4{{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
        runCase(4, k4, "K4");
        Edges k5;
        for (int a = 0; a < 5; ++a) for (int b = a + 1; b < 5; ++b) k5.emplace_back(a, b);
        runCase(5, k5, "K5");
        Edges cube{{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        runCase(8, cube, "cube");
        Edges k33{{0, 3}, {0, 4}, {0, 5}, {1, 3}, {1, 4}, {1, 5}, {2, 3}, {2, 4}, {2, 5}};
        runCase(6, k33, "K33");
        Edges prism{{0, 1}, {1, 2}, {2, 0}, {3, 4}, {4, 5}, {5, 3}, {0, 3}, {1, 4}, {2, 5}};
        runCase(6, prism, "prism");
        Edges petersen{{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0}, {0, 5}, {1, 6}, {2, 7}, {3, 8}, {4, 9},
                       {5, 7}, {7, 9}, {9, 6}, {6, 8}, {8, 5}};
        runCase(10, petersen, "petersen");
        // truncated tetrahedron: D3-reducible but (per Eppstein) of treewidth 4
        Edges tt{{0, 1}, {1, 2}, {2, 0}, {3, 4}, {4, 5}, {5, 3}, {6, 7}, {7, 8}, {8, 6}, {9, 10}, {10, 11}, {11, 9},
                 {0, 3}, {1, 6}, {2, 9}, {4, 7}, {5, 10}, {8, 11}};
        runCase(12, tt, "truncated-tetrahedron");
        for (int k = 3; k <= 12; ++k) {  // wheels W_k
            Edges w;
            for (int i = 0; i < k; ++i) { w.emplace_back(k, i); w.emplace_back(i, (i + 1) % k); }
            runCase(k + 1, w, "wheel");
        }
        // edges with out-of-range endpoints must be rejected without memory errors
        for (const Edges& bad : {Edges{{0, 4}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}},
                                 Edges{{0, -1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}}}) {
            if (halin::recognizeHalin(4, bad).isHalin) {
                ++failures;
                std::printf("BAD ENDPOINT accepted by recognizeHalin\n");
            }
            try {
                halin::halinTreeDecomposition(4, bad, {0, 1, 1, 1});
                ++failures;
                std::printf("BAD ENDPOINT accepted by halinTreeDecomposition\n");
            } catch (const std::runtime_error&) {
            }
        }
        // a Halin graph plus a self-loop / a repeated edge is rejected with
        // notSimple and accepted after simplification (as halin_td does)
        {
            Edges w6;
            for (int i = 0; i < 6; ++i) { w6.emplace_back(6, i); w6.emplace_back(i, (i + 1) % 6); }
            for (const auto& extra : {std::pair<int, int>{2, 2}, std::pair<int, int>{1, 0}}) {
                halin::Graph g{7, w6};
                g.edges.push_back(extra);
                halin::HalinRecognition rec = halin::recognizeHalin(g.n, g.edges);
                if (rec.isHalin || !rec.notSimple) { ++failures; std::printf("NON-SIMPLE not flagged\n"); }
                if (halin::removeLoopsAndMultiEdges(g) != 1 || !halin::recognizeHalin(g.n, g.edges).isHalin) {
                    ++failures;
                    std::printf("SIMPLIFIED wheel rejected\n");
                }
            }
            if (halin::recognizeHalin(6, Edges{{0, 3}, {0, 4}, {0, 5}, {1, 3}, {1, 4}, {1, 5}, {2, 3}, {2, 4}, {2, 5}})
                    .notSimple) {
                ++failures;
                std::printf("simple graph flagged as non-simple\n");
            }
        }
        // removeLoopsAndMultiEdges keeps the first occurrence of each edge, in
        // order (own generator, so the random cases below do not change)
        std::mt19937_64 grng(seed + 0x9e3779b97f4a7c15ULL);
        for (int rep = 0; rep < 2000; ++rep) {
            halin::Graph g;
            g.n = 1 + static_cast<int>(grng() % 6);
            const size_t m = grng() % 20;
            for (size_t e = 0; e < m; ++e)
                g.edges.emplace_back(static_cast<int>(grng() % g.n), static_cast<int>(grng() % g.n));
            Edges expect;
            std::set<std::pair<int, int>> seen;
            for (auto [a, b] : g.edges)
                if (a != b && seen.insert({std::min(a, b), std::max(a, b)}).second) expect.emplace_back(a, b);
            const size_t removed = halin::removeLoopsAndMultiEdges(g);
            if (g.edges != expect || removed != m - expect.size()) {
                ++failures;
                std::printf("BAD SIMPLIFICATION n=%d m=%zu\n", g.n, m);
            }
        }
    }

    for (long long it = 0; it < iters; ++it) {
        int kind = static_cast<int>(rng() % 4);
        if (kind == 0) {  // Halin graphs
            int n;
            Edges E = randomHalin(3 + static_cast<int>(rng() % 9), rng, n);
            if (n > 16) continue;
            runCase(n, relabel(n, E, rng), "halin");
        } else if (kind == 1 || kind == 2) {  // perturbed Halin graphs
            int n;
            Edges E = randomHalin(3 + static_cast<int>(rng() % 8), rng, n);
            if (n > 15) continue;
            int op = static_cast<int>(rng() % 3);
            if (op == 0 || E.size() < 2) {  // add an edge
                int a = static_cast<int>(rng() % n), b = static_cast<int>(rng() % n);
                E.emplace_back(a, b);
            } else if (op == 1) {  // remove an edge
                E.erase(E.begin() + static_cast<long>(rng() % E.size()));
            } else {  // rewire one endpoint
                auto& e = E[rng() % E.size()];
                e.second = static_cast<int>(rng() % n);
            }
            if (!isSimple(n, E)) continue;
            runCase(n, relabel(n, E, rng), "perturbed");
        } else {  // random graphs with min degree >= 3
            int n = 4 + static_cast<int>(rng() % 7);
            double p = 0.3 + 0.5 * std::uniform_real_distribution<double>(0, 1)(rng);
            Edges E;
            std::vector<int> deg(n, 0);
            for (int a = 0; a < n; ++a)
                for (int b = a + 1; b < n; ++b)
                    if (std::uniform_real_distribution<double>(0, 1)(rng) < p) { E.emplace_back(a, b); ++deg[a]; ++deg[b]; }
            if (*std::min_element(deg.begin(), deg.end()) < 3) continue;
            runCase(n, E, "random");
        }
    }
    std::printf("cases: halin/accepted=%lld halin/rejected=%lld nonhalin/accepted=%lld nonhalin/rejected=%lld\n",
                stats[1][1], stats[1][0], stats[0][1], stats[0][0]);
    std::printf("%s (%lld failures)\n", failures ? "FAILED" : "ALL OK", failures);
    return failures ? 1 : 0;
}
