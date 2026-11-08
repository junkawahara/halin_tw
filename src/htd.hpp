// SPDX-License-Identifier: MIT

// H-Td: linear-time optimal (width 3) tree decomposition of Halin graphs.
//   J.A. Alejandro-Soto, J.A. Trejo-Sanchez, C. Segura,
//   "A Practical Linear Time Algorithm for Optimal Tree Decomposition of
//   Halin Graphs", arXiv:2506.02346, 2025.
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace halin {

struct TreeDecomposition {
    // Bag of node t: bags[t][0 .. bagSize[t]-1] (0-indexed graph vertices).
    std::vector<std::array<int, 4>> bags;
    std::vector<unsigned char> bagSize;
    // Parent node in the decomposition tree (-1 for the root node).
    std::vector<int> parent;

    int numNodes() const { return static_cast<int>(bags.size()); }
    int maxBagSize() const {
        int m = 0;
        for (unsigned char s : bagSize) m = s > m ? s : m;
        return m;
    }
};

// Computes a width-3 tree decomposition of the Halin graph (n, edges), whose
// leaf cycle vertices are given by isLeaf (e.g. from recognizeHalin).
// Throws std::runtime_error if (edges, isLeaf) is not a valid tree + leaf
// cycle decomposition.
TreeDecomposition halinTreeDecomposition(int n, const std::vector<std::pair<int, int>>& edges,
                                         const std::vector<char>& isLeaf);

}  // namespace halin
