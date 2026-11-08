// SPDX-License-Identifier: MIT

// Implementation of Eppstein's D3-reduction based Halin recognition.
//
// Reductions (all three named vertices must have degree three):
//   D3a: a triangle {p,q,r} whose three outside neighbours are distinct is
//        contracted into one new vertex adjacent to those three neighbours.
//   D3b: an induced path p-q-r together with a single common neighbour s
//        (the apex); q is deleted and the edge p-r is inserted.
// For Halin recognition a set K of vertices known to lie on the leaf cycle is
// maintained:
//   D3a on {u,v,w}: forbidden if u,v,w are all in K. Otherwise the new vertex
//        is put in K, as are the outside neighbours of those of u,v,w that
//        already were in K.
//   D3b on u-v-w with apex x: forbidden if x is in K. Otherwise u and w are
//        put in K and, if the graph became K4, also its fourth vertex.
// The graph is Halin iff the process ends in K4 with a vertex outside K.
//
// The decomposition is recovered by undoing the reductions in reverse order
// while maintaining the set of leaves (cycle vertices):
//   * base K4: a vertex not in K is the hub, the other three are leaves;
//   * undo D3b: the deleted middle vertex becomes a leaf;
//   * undo D3a: the contracted vertex x was a leaf. Exactly one of its three
//        neighbours is internal (its tree parent); the triangle vertex that is
//        attached to that neighbour becomes internal, the other two leaves.
#include "eppstein.hpp"

#include <array>
#include <cassert>

namespace halin {
namespace {

// Dynamic simple graph with O(1) edge insertion/deletion (edge-id based
// adjacency arrays with back-pointers) and O(deg) adjacency tests.
class DynGraph {
public:
    DynGraph(int n, size_t edgeCapacity) : adj_(n), alive_(n, 1) {
        eu_.reserve(edgeCapacity);
        ev_.reserve(edgeCapacity);
        pu_.reserve(edgeCapacity);
        pv_.reserve(edgeCapacity);
    }
    int addVertex() {
        adj_.emplace_back();
        alive_.push_back(1);
        return static_cast<int>(adj_.size()) - 1;
    }
    int addEdge(int a, int b) {
        int e = static_cast<int>(eu_.size());
        eu_.push_back(a);
        ev_.push_back(b);
        pu_.push_back(static_cast<int>(adj_[a].size()));
        adj_[a].push_back(e);
        pv_.push_back(static_cast<int>(adj_[b].size()));
        adj_[b].push_back(e);
        return e;
    }
    void removeEdge(int e) {
        detach(eu_[e], pu_[e]);
        detach(ev_[e], pv_[e]);
    }
    // Removes the edge between a and b; requires that it exists. O(deg(a)).
    void removeEdgeBetween(int a, int b) {
        for (int e : adj_[a]) {
            if (other(e, a) == b) { removeEdge(e); return; }
        }
        assert(false && "edge not found");
    }
    void removeFirstEdge(int v) { removeEdge(adj_[v][0]); }
    void killVertex(int v) { alive_[v] = 0; }

    int deg(int v) const { return static_cast<int>(adj_[v].size()); }
    bool alive(int v) const { return alive_[v] != 0; }
    int other(int e, int v) const { return eu_[e] == v ? ev_[e] : eu_[e]; }
    int nb(int v, int k) const { return other(adj_[v][k], v); }
    // Adjacency test by scanning the list of `small` (use the low-degree side).
    bool adjacent(int small, int x) const {
        for (int e : adj_[small]) if (other(e, small) == x) return true;
        return false;
    }
    int numVertexSlots() const { return static_cast<int>(adj_.size()); }

private:
    void detach(int v, int pos) {
        std::vector<int>& list = adj_[v];
        int last = list.back();
        list[pos] = last;
        if (eu_[last] == v && pu_[last] == static_cast<int>(list.size()) - 1) pu_[last] = pos;
        else pv_[last] = pos;
        list.pop_back();
    }
    std::vector<std::vector<int>> adj_;
    std::vector<char> alive_;
    std::vector<int> eu_, ev_, pu_, pv_;
};

// One performed reduction, recorded for the reverse pass.
struct Record {
    bool isD3a;
    int newVertex;            // D3a: contracted vertex; D3b: unused
    std::array<int, 3> tri;   // D3a: triangle vertices; D3b: tri[0] = deleted middle
    std::array<int, 3> out;   // D3a: out[k] is the outside neighbour of tri[k]
};

}  // namespace

HalinRecognition recognizeHalin(int n, const std::vector<std::pair<int, int>>& edges) {
    HalinRecognition res;
    auto reject = [&](const std::string& why) {
        res.isHalin = false;
        res.reason = why;
        res.isLeaf.clear();
        return res;
    };
    if (n < 4) return reject("fewer than 4 vertices");

    // ---- input sanity: simple graph with minimum degree 3 -----------------
    {
        std::vector<int> deg(n, 0);
        for (auto [a, b] : edges) {
            if (a < 0 || b < 0 || a >= n || b >= n) return reject("edge endpoint out of range");
            if (a == b) return reject("self-loop");
            ++deg[a];
            ++deg[b];
        }
        std::vector<int> start(n + 1, 0);
        for (int v = 0; v < n; ++v) {
            if (deg[v] < 3) return reject("vertex of degree < 3");
            start[v + 1] = start[v] + deg[v];
        }
        std::vector<int> nbr(start[n]), fill(start.begin(), start.end() - 1);
        for (auto [a, b] : edges) { nbr[fill[a]++] = b; nbr[fill[b]++] = a; }
        std::vector<int> mark(n, -1);
        for (int v = 0; v < n; ++v)
            for (int k = start[v]; k < start[v + 1]; ++k) {
                if (mark[nbr[k]] == v) return reject("parallel edges");
                mark[nbr[k]] = v;
            }
    }

    // A Halin graph with n vertices has fewer than 2n edges, and every D3a
    // reduction adds 3 edges, so this capacity avoids reallocations.
    DynGraph g(n, edges.size() + 3 * static_cast<size_t>(n) + 8);
    for (auto [a, b] : edges) g.addEdge(a, b);

    std::vector<char> inK(n, 0);
    std::vector<int> work;  // the candidate collection "C" of the paper
    work.reserve(2 * static_cast<size_t>(n));
    for (int v = 0; v < n; ++v) if (g.deg(v) == 3) work.push_back(v);
    std::vector<Record> records;
    records.reserve(n);
    int aliveCount = n;

    auto pushIfDeg3 = [&](int v) {
        if (g.alive(v) && g.deg(v) == 3) work.push_back(v);
    };
    auto pushWithNeighbours = [&](int v) {
        if (!g.alive(v) || g.deg(v) != 3) return;
        work.push_back(v);
        for (int k = 0; k < 3; ++k) pushIfDeg3(g.nb(v, k));
    };

    while (!work.empty()) {
        int v = work.back();
        work.pop_back();
        if (!g.alive(v) || g.deg(v) != 3) continue;

        bool done = false;
        int nv[3] = {g.nb(v, 0), g.nb(v, 1), g.nb(v, 2)};

        // ---------------- D3a: triangle {v, a, b} ----------------
        for (int x = 0; x < 3 && !done; ++x) {
            for (int y = x + 1; y < 3 && !done; ++y) {
                int a = nv[x], b = nv[y], c = nv[3 - x - y];
                if (g.deg(a) != 3 || g.deg(b) != 3 || !g.adjacent(a, b)) continue;
                int oa = -1, ob = -1;
                for (int k = 0; k < 3; ++k) {
                    int w = g.nb(a, k);
                    if (w != v && w != b) oa = w;
                    w = g.nb(b, k);
                    if (w != v && w != a) ob = w;
                }
                int oc = c;
                if (oa == ob || oa == oc || ob == oc) continue;
                if (inK[v] && inK[a] && inK[b]) continue;  // forbidden by K

                Record rec{true, -1, {v, a, b}, {oc, oa, ob}};
                // Remove the triangle and its three outside edges.
                for (int t : rec.tri)
                    while (g.deg(t) > 0) g.removeFirstEdge(t);
                for (int t : rec.tri) g.killVertex(t);
                int z = g.addVertex();
                inK.push_back(1);  // the contracted vertex is a leaf
                rec.newVertex = z;
                for (int k = 0; k < 3; ++k) g.addEdge(z, rec.out[k]);
                for (int k = 0; k < 3; ++k)
                    if (inK[rec.tri[k]]) inK[rec.out[k]] = 1;
                aliveCount -= 2;
                records.push_back(rec);
                ++res.reductionsD3a;
                pushWithNeighbours(z);
                done = true;
            }
        }
        if (done) continue;

        // ---------------- D3b: v is the middle of path p-v-r, apex s -------
        for (int x = 0; x < 3 && !done; ++x) {
            int s = nv[x], p = nv[(x + 1) % 3], r = nv[(x + 2) % 3];
            if (g.deg(p) != 3 || g.deg(r) != 3) continue;
            if (g.adjacent(p, r)) continue;                      // must be induced
            if (!g.adjacent(p, s) || !g.adjacent(r, s)) continue; // common apex
            if (inK[s]) continue;                                  // forbidden by K

            g.removeEdgeBetween(v, p);
            g.removeEdgeBetween(v, r);
            g.removeEdgeBetween(v, s);
            g.killVertex(v);
            g.addEdge(p, r);
            --aliveCount;
            inK[p] = inK[r] = 1;
            if (aliveCount == 4) {
                // the fourth vertex of the resulting K4 (p, r, s are three)
                for (int k = 0; k < 3; ++k) {
                    int w = g.nb(p, k);
                    if (w != r && w != s) inK[w] = 1;
                }
            }
            records.push_back(Record{false, -1, {v, -1, -1}, {p, r, s}});
            ++res.reductionsD3b;
            pushWithNeighbours(p);
            pushWithNeighbours(r);
            pushIfDeg3(s);
            if (g.deg(s) == 3) pushWithNeighbours(s);
            done = true;
        }
    }

    // ---------------- final test: K4 with a vertex outside K ----------------
    if (aliveCount != 4) return reject("irreducible graph is not K4 (not Halin)");
    std::vector<int> rest;
    for (int v = 0; v < g.numVertexSlots(); ++v)
        if (g.alive(v)) rest.push_back(v);
    assert(rest.size() == 4);
    for (int v : rest) {
        if (g.deg(v) != 3) return reject("irreducible graph is not K4 (not Halin)");
        for (int w : rest)
            if (w != v && !g.adjacent(v, w)) return reject("irreducible graph is not K4 (not Halin)");
    }
    int hub = -1;
    for (int v : rest) if (!inK[v]) { hub = v; break; }
    if (hub < 0) return reject("all vertices of the final K4 are forced onto the cycle (not Halin)");

    // ---------------- reverse pass: recover the leaf set ----------------
    std::vector<char> leaf(g.numVertexSlots(), 0);
    for (int v : rest) leaf[v] = (v != hub);
    for (size_t idx = records.size(); idx-- > 0;) {
        const Record& rec = records[idx];
        if (!rec.isD3a) {
            // p and r are leaves, apex s internal; middle vertex becomes a leaf
            if (!leaf[rec.out[0]] || !leaf[rec.out[1]] || leaf[rec.out[2]])
                return reject("internal error: inconsistent D3b reversal");
            leaf[rec.tri[0]] = 1;
            continue;
        }
        if (!leaf[rec.newVertex]) return reject("internal error: contracted vertex is not a leaf");
        int internalIdx = -1, internalCount = 0;
        for (int k = 0; k < 3; ++k)
            if (!leaf[rec.out[k]]) { internalIdx = k; ++internalCount; }
        if (internalCount != 1) return reject("internal error: inconsistent D3a reversal");
        for (int k = 0; k < 3; ++k) leaf[rec.tri[k]] = (k != internalIdx);
    }
    leaf.resize(n);
    res.isHalin = true;
    res.isLeaf = std::move(leaf);
    return res;
}

}  // namespace halin
