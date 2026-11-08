// halin_td: reads a Halin graph in PACE 2017 .gr format and writes an optimal
// (width 3) tree decomposition in PACE 2017 .td format.
//
//   halin_td [-o out.td] [--stats] [input.gr]
//
// Without input file the graph is read from stdin; without -o the
// decomposition is written to stdout. --stats prints timings to stderr.
// Exit codes: 0 success, 1 usage/I-O error, 2 input is not a Halin graph.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "eppstein.hpp"
#include "graph_io.hpp"
#include "htd.hpp"

namespace {

using Clock = std::chrono::steady_clock;
double secondsSince(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

void usage(const char* prog) {
    std::fprintf(stderr, "usage: %s [-o out.td] [--stats] [input.gr]\n", prog);
}

}  // namespace

int main(int argc, char** argv) {
    std::string inPath, outPath;
    bool stats = false;
    for (int a = 1; a < argc; ++a) {
        if (!std::strcmp(argv[a], "-o") && a + 1 < argc) outPath = argv[++a];
        else if (!std::strcmp(argv[a], "--stats")) stats = true;
        else if (!std::strcmp(argv[a], "-h") || !std::strcmp(argv[a], "--help")) { usage(argv[0]); return 0; }
        else if (argv[a][0] == '-' && argv[a][1] != 0) { usage(argv[0]); return 1; }
        else inPath = argv[a];
    }

    try {
        auto t0 = Clock::now();
        halin::Graph g = halin::parseGr(halin::readAll(inPath));
        double tRead = secondsSince(t0);

        auto t1 = Clock::now();
        halin::HalinRecognition rec = halin::recognizeHalin(g.n, g.edges);
        double tRecog = secondsSince(t1);
        if (!rec.isHalin) {
            std::fprintf(stderr, "error: input is not a Halin graph (%s)\n", rec.reason.c_str());
            return 2;
        }

        auto t2 = Clock::now();
        halin::TreeDecomposition td = halin::halinTreeDecomposition(g.n, g.edges, rec.isLeaf);
        double tTd = secondsSince(t2);

        auto t3 = Clock::now();
        FILE* out = outPath.empty() ? stdout : std::fopen(outPath.c_str(), "wb");
        if (!out) { std::fprintf(stderr, "error: cannot open %s\n", outPath.c_str()); return 1; }
        {
            halin::Writer w(out);
            w.put("s td "); w.put(static_cast<long long>(td.numNodes()));
            w.put(' ');     w.put(static_cast<long long>(td.maxBagSize()));
            w.put(' ');     w.put(static_cast<long long>(g.n));
            w.put('\n');
            for (int t = 0; t < td.numNodes(); ++t) {
                w.put("b "); w.put(static_cast<long long>(t + 1));
                for (int k = 0; k < td.bagSize[t]; ++k) { w.put(' '); w.put(static_cast<long long>(td.bags[t][k] + 1)); }
                w.put('\n');
            }
            for (int t = 0; t < td.numNodes(); ++t) {
                if (td.parent[t] < 0) continue;
                w.put(static_cast<long long>(td.parent[t] + 1)); w.put(' ');
                w.put(static_cast<long long>(t + 1)); w.put('\n');
            }
        }
        if (out != stdout) std::fclose(out); else std::fflush(stdout);
        double tWrite = secondsSince(t3);

        if (stats) {
            long long leaves = 0;
            for (char c : rec.isLeaf) leaves += c;
            std::fprintf(stderr,
                         "n=%d m=%zu leaves=%lld D3a=%lld D3b=%lld nodes=%d width=%d\n"
                         "time_read=%.6f time_recognition=%.6f time_htd=%.6f time_write=%.6f\n",
                         g.n, g.edges.size(), leaves, rec.reductionsD3a, rec.reductionsD3b, td.numNodes(),
                         td.maxBagSize() - 1, tRead, tRecog, tTd, tWrite);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
