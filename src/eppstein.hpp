// SPDX-License-Identifier: MIT

// Linear-time Halin graph recognition and tree/cycle decomposition based on
// the D3 reduction rules of
//   D. Eppstein, "Simple Recognition of Halin Graphs and Their
//   Generalizations", JGAA 20(2), 2016 (arXiv:1502.05334).
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace halin {

struct HalinRecognition {
    bool isHalin = false;
    std::string reason;          // why the graph was rejected (if it was)
    bool notSimple = false;      // rejected because of a self-loop or parallel edges
    std::vector<char> isLeaf;    // size n; 1 = vertex lies on the leaf cycle
    long long reductionsD3a = 0;
    long long reductionsD3b = 0;
};

// Decides whether the simple graph (n, edges) is a Halin graph. On success,
// isLeaf describes one valid decomposition: edges with both endpoints marked
// as leaves form the leaf cycle C; all remaining edges form the skeleton tree.
// Edges with an endpoint outside [0, n) make the graph rejected. A graph
// with a self-loop or parallel edges is rejected with notSimple set; the
// underlying simple graph has then not been tested.
HalinRecognition recognizeHalin(int n, const std::vector<std::pair<int, int>>& edges);

}  // namespace halin
