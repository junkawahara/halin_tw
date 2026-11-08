// td_validate: checks that a PACE 2017 .td file is a valid tree decomposition
// of a PACE 2017 .gr graph.
//
//   td_validate <graph.gr> <decomposition.td> [--expect-width W]
//
// Checks: header consistency, each bag line, decomposition graph is a tree,
// (C1) every vertex is in some bag, (C2) every edge is in some bag,
// (C3) bags containing a vertex induce a connected subtree, and the declared
// maximum bag size. Prints "valid width=W" and exits 0 on success, otherwise
// prints the first violation and exits 1.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <numeric>
#include <string>
#include <vector>

#include "../src/graph_io.hpp"

namespace {

[[noreturn]] void invalid(const std::string& msg) {
    std::printf("invalid: %s\n", msg.c_str());
    std::exit(1);
}

struct Dsu {
    std::vector<int> p;
    explicit Dsu(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) {
        while (p[x] != x) x = p[x] = p[p[x]];
        return x;
    }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        p[a] = b;
        return true;
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 && !(argc == 5 && !std::strcmp(argv[3], "--expect-width"))) {
        std::fprintf(stderr, "usage: %s <graph.gr> <decomposition.td> [--expect-width W]\n", argv[0]);
        return 2;
    }
    long long expectWidth = argc == 5 ? std::atoll(argv[4]) : -2;
    halin::Graph g;
    std::string tdData;
    try {
        g = halin::parseGr(halin::readAll(argv[1]));
        tdData = halin::readAll(argv[2]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }

    // ---- parse .td --------------------------------------------------------
    halin::LineReader in(tdData);
    long long numBags = -1, declaredMax = -1, declaredN = -1;
    std::vector<long long> bagStart;   // CSR over bag contents
    std::vector<int> bagData;
    std::vector<char> bagSeen;
    std::vector<std::pair<int, int>> tedges;
    std::string tok;
    try {
        while (in.nextLine()) {
            char c = in.firstChar();
            if (c == 0 || c == 'c') continue;
            if (c == 's') {
                if (numBags >= 0) invalid("duplicate s line");
                in.nextToken(tok);
                if (!in.nextToken(tok) || tok != "td") invalid("expected 's td ...'");
                if (!in.nextLong(numBags) || !in.nextLong(declaredMax) || !in.nextLong(declaredN) || !in.atLineEnd())
                    invalid("malformed s line");
                if (declaredN != g.n) invalid("s line vertex count does not match graph");
                if (numBags < 1) invalid("decomposition has no bags");
                bagSeen.assign(numBags, 0);
                continue;
            }
            if (numBags < 0) invalid("line before s line");
            if (c == 'b') {
                in.nextToken(tok);
                long long id;
                if (!in.nextLong(id) || id < 1 || id > numBags) invalid("bad bag id");
                if (bagSeen[id - 1]) invalid("bag " + std::to_string(id) + " listed twice");
                bagSeen[id - 1] = 1;
                // bags may be listed in any order; store (id, vertices) temporarily
                bagData.push_back(-static_cast<int>(id));  // marker
                long long v;
                while (in.nextLong(v)) {
                    if (v < 1 || v > g.n) invalid("vertex out of range in bag " + std::to_string(id));
                    bagData.push_back(static_cast<int>(v - 1));
                }
                continue;
            }
            long long a, b;
            if (!in.nextLong(a) || !in.nextLong(b) || !in.atLineEnd()) invalid("malformed line " + std::to_string(in.lineNo()));
            if (a < 1 || b < 1 || a > numBags || b > numBags) invalid("tree edge with bad bag id");
            tedges.emplace_back(static_cast<int>(a - 1), static_cast<int>(b - 1));
        }
    } catch (const std::exception& e) {
        invalid(e.what());
    }
    if (numBags < 0) invalid("missing s line");
    for (long long t = 0; t < numBags; ++t)
        if (!bagSeen[t]) invalid("bag " + std::to_string(t + 1) + " missing");

    // regroup bag contents by id, sorted and duplicate-free
    const int B = static_cast<int>(numBags);
    std::vector<int> bsize(B, 0);
    {
        int cur = -1;
        for (int x : bagData) {
            if (x < 0) cur = -x - 1;
            else ++bsize[cur];
        }
    }
    std::vector<long long> bstart(B + 1, 0);
    for (int t = 0; t < B; ++t) bstart[t + 1] = bstart[t] + bsize[t];
    std::vector<int> bag(bstart[B]);
    {
        std::vector<long long> fill(bstart.begin(), bstart.end() - 1);
        int cur = -1;
        for (int x : bagData) {
            if (x < 0) cur = -x - 1;
            else bag[fill[cur]++] = x;
        }
    }
    bagData.clear();
    bagData.shrink_to_fit();
    long long maxSize = 0;
    for (int t = 0; t < B; ++t) {
        auto b0 = bag.begin() + bstart[t], b1 = bag.begin() + bstart[t + 1];
        std::sort(b0, b1);
        if (std::adjacent_find(b0, b1) != b1) invalid("bag " + std::to_string(t + 1) + " has a repeated vertex");
        maxSize = std::max<long long>(maxSize, b1 - b0);
    }
    if (maxSize != declaredMax) invalid("declared max bag size " + std::to_string(declaredMax) + " but actual is " + std::to_string(maxSize));
    auto inBag = [&](int t, int v) {
        return std::binary_search(bag.begin() + bstart[t], bag.begin() + bstart[t + 1], v);
    };

    // ---- decomposition must be a tree -------------------------------------
    if (static_cast<long long>(tedges.size()) != numBags - 1) invalid("decomposition has " + std::to_string(tedges.size()) + " edges, expected " + std::to_string(numBags - 1));
    {
        Dsu d(B);
        for (auto [a, b] : tedges)
            if (!d.unite(a, b)) invalid("decomposition contains a cycle");
    }

    // ---- occurrences of each vertex ---------------------------------------
    std::vector<long long> occ(g.n, 0);
    for (int x : bag) ++occ[x];
    for (int v = 0; v < g.n; ++v)
        if (occ[v] == 0) invalid("(C1) vertex " + std::to_string(v + 1) + " is in no bag");
    std::vector<long long> ostart(g.n + 1, 0);
    for (int v = 0; v < g.n; ++v) ostart[v + 1] = ostart[v] + occ[v];
    std::vector<int> occBags(ostart[g.n]);
    {
        std::vector<long long> fill(ostart.begin(), ostart.end() - 1);
        for (int t = 0; t < B; ++t)
            for (long long k = bstart[t]; k < bstart[t + 1]; ++k) occBags[fill[bag[k]]++] = t;
    }

    // ---- (C2) every edge covered -------------------------------------------
    for (auto [u, v] : g.edges) {
        int a = occ[u] <= occ[v] ? u : v, b = (a == u) ? v : u;
        bool ok = false;
        for (long long k = ostart[a]; k < ostart[a + 1] && !ok; ++k) ok = inBag(occBags[k], b);
        if (!ok) invalid("(C2) edge " + std::to_string(u + 1) + "-" + std::to_string(v + 1) + " is in no bag");
    }

    // ---- (C3) connectivity: for each vertex, #tree edges inside = occ - 1 --
    std::vector<long long> inner(g.n, 0);
    for (auto [a, b] : tedges) {
        // intersect two sorted bags
        long long i = bstart[a], j = bstart[b];
        while (i < bstart[a + 1] && j < bstart[b + 1]) {
            if (bag[i] < bag[j]) ++i;
            else if (bag[i] > bag[j]) ++j;
            else { ++inner[bag[i]]; ++i; ++j; }
        }
    }
    for (int v = 0; v < g.n; ++v)
        if (inner[v] != occ[v] - 1) invalid("(C3) bags containing vertex " + std::to_string(v + 1) + " are not connected");

    long long width = maxSize - 1;
    if (expectWidth != -2 && width != expectWidth)
        invalid("width " + std::to_string(width) + " differs from expected " + std::to_string(expectWidth));
    std::printf("valid width=%lld bags=%d\n", width, B);
    return 0;
}
