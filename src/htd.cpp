// SPDX-License-Identifier: MIT

// Implementation of H-Td (arXiv:2506.02346).
//
// Notation follows the paper:
//   c_{v,i}       i-th child of v in the adjacency order (Section 4)
//   l_{v,1}       first leaf of the subtree of v;  l_{v,inf} its last leaf
//   u^+           next leaf on the leaf cycle (by x-value, cyclically)
//   omega_1(v,j) = l_{c_{v,j},inf}^+      omega_2(v,j) = l_{c_{v,j},inf}
//   representatives of AH_t(v,i,j) = { v, l_{c_{v,i},1}, omega_t(v,j) }
//
// AH-Td(t,v,i,j) (Section 5), given a node R whose bag is the set of
// representatives:
//   i == j: if c = c_{v,i} is a leaf, stop. Otherwise add
//             S = {v, l_{c,1}, omega, c}  as child of R,
//             A = {l_{c,1}, omega, c}     as child of S,
//           and continue with AH-Td(t, c, 1, deg_c(c)) using A as its R.
//   i <  j: let x = l_{c_{v,i+1},1}. Add
//             S = {v, l_{c_{v,i},1}, x, omega}  as child of R,
//             A = {v, l_{c_{v,i},1}, x}         as child of S,
//             B = {v, x, omega}                 as child of S,
//           and continue with AH-Td(1,v,i,i) on A and AH-Td(t,v,i+1,j) on B.
// H-Td(H) = AH-Td(2, r, 1, deg_c(r)) for a root r of degree >= 3.
//
// The recursion is run with an explicit stack so that very deep skeletons do
// not overflow the call stack.
#include "htd.hpp"

#include <stdexcept>

namespace halin {

namespace {

struct Task {
    int t;  // Almost Halin type (1 or 2)
    int v;  // root of the Almost Halin
    int i;  // first child index (0-based)
    int j;  // last child index (0-based)
    int R;  // decomposition node whose bag is the set of representatives
};

}  // namespace

TreeDecomposition halinTreeDecomposition(int n, const std::vector<std::pair<int, int>>& edges,
                                         const std::vector<char>& isLeaf) {
    auto fail = [](const std::string& why) { throw std::runtime_error("invalid Halin decomposition: " + why); };
    if (static_cast<int>(isLeaf.size()) != n) fail("isLeaf has wrong size");
    if (n < 4) fail("fewer than 4 vertices");

    // ---- split edges into skeleton (tree) and leaf cycle ----------------
    std::vector<int> treeDeg(n, 0), cyc(2 * static_cast<size_t>(n), -1);
    long long treeEdges = 0;
    for (auto [a, b] : edges) {
        if (isLeaf[a] && isLeaf[b]) {
            int* ca = &cyc[2 * static_cast<size_t>(a)];
            int* cb = &cyc[2 * static_cast<size_t>(b)];
            if (ca[1] != -1 || cb[1] != -1) fail("leaf with more than two cycle edges");
            (ca[0] == -1 ? ca[0] : ca[1]) = b;
            (cb[0] == -1 ? cb[0] : cb[1]) = a;
        } else {
            ++treeDeg[a];
            ++treeDeg[b];
            ++treeEdges;
        }
    }
    if (treeEdges != n - 1) fail("skeleton does not have n-1 edges");
    int numLeaves = 0, root = -1;
    for (int v = 0; v < n; ++v) {
        if (isLeaf[v]) {
            ++numLeaves;
            if (treeDeg[v] != 1) fail("leaf with skeleton degree != 1");
            if (cyc[2 * static_cast<size_t>(v) + 1] == -1) fail("leaf with fewer than two cycle edges");
        } else {
            if (treeDeg[v] < 3) fail("internal vertex of skeleton degree < 3");
            if (root < 0) root = v;  // any internal vertex has degree >= 3
        }
    }
    if (root < 0) fail("no internal vertex");

    // CSR adjacency of the skeleton.
    std::vector<int> tstart(n + 1, 0);
    for (int v = 0; v < n; ++v) tstart[v + 1] = tstart[v] + treeDeg[v];
    std::vector<int> tadj(tstart[n]);
    {
        std::vector<int> fill(tstart.begin(), tstart.end() - 1);
        for (auto [a, b] : edges)
            if (!(isLeaf[a] && isLeaf[b])) { tadj[fill[a]++] = b; tadj[fill[b]++] = a; }
    }

    // ---- root the skeleton at `root` (BFS) ------------------------------
    std::vector<int> parent(n, -1), order;
    order.reserve(n);
    {
        std::vector<char> seen(n, 0);
        seen[root] = 1;
        order.push_back(root);
        for (size_t h = 0; h < order.size(); ++h) {
            int v = order[h];
            for (int k = tstart[v]; k < tstart[v + 1]; ++k) {
                int w = tadj[k];
                if (seen[w]) continue;
                seen[w] = 1;
                parent[w] = v;
                order.push_back(w);
            }
        }
        if (static_cast<int>(order.size()) != n) fail("skeleton is not connected");
    }

    // ---- Appendix A: choose v, w with lca(v, w) = root ------------------
    // Colour the subtree of one child c of the root; some cycle edge joins a
    // coloured leaf u with an uncoloured leaf w, and lca(u, w) = root.
    std::vector<char> colored(n, 0);
    {
        int c = tadj[tstart[root]];
        for (int v : order)
            if (v != root) colored[v] = (v == c) || colored[parent[v]];
    }
    int startLeaf = -1, endLeaf = -1;
    for (int u = 0; u < n && startLeaf < 0; ++u) {
        if (!isLeaf[u] || !colored[u]) continue;
        for (int k = 0; k < 2; ++k) {
            int w = cyc[2 * static_cast<size_t>(u) + k];
            if (!colored[w]) { startLeaf = u; endLeaf = w; break; }
        }
    }
    if (startLeaf < 0) fail("no cycle edge leaves the subtree of a root child");

    // Traverse C from startLeaf, away from endLeaf: x-values 0 .. L-1.
    std::vector<int> leafByX(numLeaves), xval(n, -1);
    {
        int prev = endLeaf, cur = startLeaf;
        for (int x = 0; x < numLeaves; ++x) {
            if (xval[cur] != -1) fail("leaf cycle is not a single cycle through all leaves");
            xval[cur] = x;
            leafByX[x] = cur;
            int a = cyc[2 * static_cast<size_t>(cur)], b = cyc[2 * static_cast<size_t>(cur) + 1];
            int nxt = (a == prev) ? b : a;
            prev = cur;
            cur = nxt;
        }
        if (cur != startLeaf || prev != endLeaf) fail("leaf cycle is not a single cycle through all leaves");
    }

    // ---- adjacency order: children sorted by x-value (Appendix A) -------
    std::vector<int> cstart(n + 1, 0);
    for (int v = 0; v < n; ++v) cstart[v + 1] = cstart[v] + treeDeg[v] - (v == root ? 0 : 1);
    std::vector<int> children(cstart[n]);
    {
        std::vector<int> fill(cstart.begin(), cstart.end() - 1);
        std::vector<char> linked(n, 0);
        for (int x = 0; x < numLeaves; ++x) {
            int y = leafByX[x];
            while (y != root && !linked[y]) {
                linked[y] = 1;
                children[fill[parent[y]]++] = y;
                y = parent[y];
            }
        }
    }

    // ---- first/last leaf of each subtree; verify planarity consistency --
    // The leaves of every subtree must form a contiguous interval of x-values
    // (Remark 2); otherwise the tree/cycle pair is not a Halin decomposition.
    std::vector<int> xmin(n), xmax(n), cnt(n);
    for (size_t h = order.size(); h-- > 0;) {
        int v = order[h];
        if (isLeaf[v]) {
            xmin[v] = xmax[v] = xval[v];
            cnt[v] = 1;
        } else {
            int lo = numLeaves, hi = -1, c = 0;
            for (int k = cstart[v]; k < cstart[v + 1]; ++k) {
                int ch = children[k];
                lo = xmin[ch] < lo ? xmin[ch] : lo;
                hi = xmax[ch] > hi ? xmax[ch] : hi;
                c += cnt[ch];
            }
            xmin[v] = lo;
            xmax[v] = hi;
            cnt[v] = c;
            if (hi - lo + 1 != c) fail("leaf order is not consistent with a planar embedding");
        }
    }
    auto firstLeaf = [&](int v) { return leafByX[xmin[v]]; };
    auto lastLeaf = [&](int v) { return leafByX[xmax[v]]; };
    auto nextLeaf = [&](int u) { return leafByX[(xval[u] + 1) % numLeaves]; };
    auto child = [&](int v, int i) { return children[cstart[v] + i]; };
    auto numChildren = [&](int v) { return cstart[v + 1] - cstart[v]; };
    auto omega = [&](int t, int v, int j) {
        int l = lastLeaf(child(v, j));
        return t == 1 ? nextLeaf(l) : l;
    };

    // ---- AH-Td ----------------------------------------------------------
    TreeDecomposition td;
    // Each internal vertex with k children creates 3(k-1) + 2 nodes at most.
    size_t expected = 3 * static_cast<size_t>(n) + 1;
    td.bags.reserve(expected);
    td.bagSize.reserve(expected);
    td.parent.reserve(expected);
    auto newNode = [&](std::initializer_list<int> verts, int par) {
        std::array<int, 4> bag{};
        int s = 0;
        for (int x : verts) {
            bool dup = false;
            for (int q = 0; q < s; ++q) dup |= (bag[q] == x);
            if (!dup) bag[s++] = x;
        }
        td.bags.push_back(bag);
        td.bagSize.push_back(static_cast<unsigned char>(s));
        td.parent.push_back(par);
        return static_cast<int>(td.bags.size()) - 1;
    };

    std::vector<Task> stack;
    int top = numChildren(root) - 1;
    int R0 = newNode({root, firstLeaf(root), omega(2, root, top)}, -1);
    stack.push_back({2, root, 0, top, R0});
    while (!stack.empty()) {
        Task tk = stack.back();
        stack.pop_back();
        const int t = tk.t, v = tk.v, i = tk.i, j = tk.j, R = tk.R;
        if (i == j) {
            int c = child(v, i);
            if (isLeaf[c]) continue;  // h(c_{v,i}) = 0: R alone suffices
            int l1 = firstLeaf(c), om = omega(t, v, j);
            int S = newNode({v, l1, om, c}, R);
            int A = newNode({l1, om, c}, S);
            stack.push_back({t, c, 0, numChildren(c) - 1, A});
        } else {
            int li = firstLeaf(child(v, i));
            int x = firstLeaf(child(v, i + 1));
            int om = omega(t, v, j);
            int S = newNode({v, li, x, om}, R);
            int A = newNode({v, li, x}, S);
            int B = newNode({v, x, om}, S);
            stack.push_back({t, v, i + 1, j, B});
            stack.push_back({1, v, i, i, A});
        }
    }
    return td;
}

}  // namespace halin
