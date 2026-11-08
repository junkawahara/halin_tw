// SPDX-License-Identifier: MIT

// Minimal, fast I/O helpers for the PACE 2017 graph (.gr) and tree
// decomposition (.td) formats.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace halin {

struct Graph {
    int n = 0;                              // vertices are 0 .. n-1
    std::vector<std::pair<int, int>> edges; // 0-indexed endpoints
};

// Reads a whole file (or stdin when path is "-" or empty) into memory.
inline std::string readAll(const std::string& path) {
    FILE* f = (path.empty() || path == "-") ? stdin : std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open file: " + path);
    std::string data;
    char buf[1 << 16];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, got);
    if (f != stdin) std::fclose(f);
    return data;
}

// Simple line-oriented tokenizer over an in-memory buffer.
class LineReader {
public:
    explicit LineReader(const std::string& d) : s_(d) {}

    // Advances to the next line. Returns false at end of input.
    bool nextLine() {
        if (pos_ >= s_.size()) return false;
        lineStart_ = pos_;
        while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
        lineEnd_ = pos_;
        if (pos_ < s_.size()) ++pos_;
        cur_ = lineStart_;
        ++lineNo_;
        return true;
    }
    // First non-blank character of the current line, or 0 if blank.
    char firstChar() {
        size_t p = lineStart_;
        while (p < lineEnd_ && isBlank(s_[p])) ++p;
        return p < lineEnd_ ? s_[p] : 0;
    }
    bool nextToken(std::string& tok) {
        skipBlank();
        if (cur_ >= lineEnd_) return false;
        size_t b = cur_;
        while (cur_ < lineEnd_ && !isBlank(s_[cur_])) ++cur_;
        tok.assign(s_, b, cur_ - b);
        return true;
    }
    bool nextLong(long long& v) {
        skipBlank();
        if (cur_ >= lineEnd_) return false;
        bool neg = false;
        if (s_[cur_] == '-' || s_[cur_] == '+') neg = (s_[cur_++] == '-');
        if (cur_ >= lineEnd_ || s_[cur_] < '0' || s_[cur_] > '9')
            throw std::runtime_error("line " + std::to_string(lineNo_) + ": expected integer");
        long long x = 0;
        while (cur_ < lineEnd_ && s_[cur_] >= '0' && s_[cur_] <= '9') x = x * 10 + (s_[cur_++] - '0');
        if (cur_ < lineEnd_ && !isBlank(s_[cur_]))
            throw std::runtime_error("line " + std::to_string(lineNo_) + ": malformed integer");
        v = neg ? -x : x;
        return true;
    }
    bool atLineEnd() {
        skipBlank();
        return cur_ >= lineEnd_;
    }
    long long lineNo() const { return lineNo_; }

private:
    static bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\r'; }
    void skipBlank() {
        while (cur_ < lineEnd_ && isBlank(s_[cur_])) ++cur_;
    }
    const std::string& s_;
    size_t pos_ = 0, lineStart_ = 0, lineEnd_ = 0, cur_ = 0;
    long long lineNo_ = 0;
};

// Parses a PACE 2017 .gr file: "c" comment lines, one "p tw n m" line,
// then m lines "u v" with 1-indexed vertices.
inline Graph parseGr(const std::string& data) {
    LineReader in(data);
    Graph g;
    bool seenP = false;
    long long m = 0;
    std::string tok;
    while (in.nextLine()) {
        char c = in.firstChar();
        if (c == 0 || c == 'c') continue;
        if (c == 'p') {
            if (seenP) throw std::runtime_error("duplicate p line");
            in.nextToken(tok);  // "p"
            if (!in.nextToken(tok) || tok != "tw") throw std::runtime_error("expected 'p tw n m'");
            long long n;
            if (!in.nextLong(n) || !in.nextLong(m) || n < 0 || m < 0)
                throw std::runtime_error("expected 'p tw n m'");
            g.n = static_cast<int>(n);
            g.edges.reserve(static_cast<size_t>(m));
            seenP = true;
            continue;
        }
        if (!seenP) throw std::runtime_error("edge before p line");
        long long u, v;
        if (!in.nextLong(u) || !in.nextLong(v) || !in.atLineEnd())
            throw std::runtime_error("line " + std::to_string(in.lineNo()) + ": expected 'u v'");
        if (u < 1 || v < 1 || u > g.n || v > g.n)
            throw std::runtime_error("line " + std::to_string(in.lineNo()) + ": vertex out of range");
        g.edges.emplace_back(static_cast<int>(u - 1), static_cast<int>(v - 1));
    }
    if (!seenP) throw std::runtime_error("missing p line");
    if (static_cast<long long>(g.edges.size()) != m)
        throw std::runtime_error("edge count does not match p line");
    return g;
}

// Buffered writer with fast integer formatting. Write errors set the error
// indicator of the stream; check them with finishOutput() after the Writer
// has been destroyed.
class Writer {
public:
    explicit Writer(FILE* f) : f_(f) { buf_.reserve(kCap + 64); }
    ~Writer() { flush(); }
    void put(char c) {
        buf_.push_back(c);
        if (buf_.size() >= kCap) flush();
    }
    void put(const char* s) {
        while (*s) put(*s++);
    }
    void put(long long x) {
        if (x < 0) { put('-'); x = -x; }
        char tmp[24];
        int k = 0;
        do { tmp[k++] = static_cast<char>('0' + x % 10); x /= 10; } while (x);
        while (k) put(tmp[--k]);
    }
    void flush() {
        if (!buf_.empty()) std::fwrite(buf_.data(), 1, buf_.size(), f_);
        buf_.clear();
    }

private:
    static constexpr size_t kCap = 1 << 16;
    FILE* f_;
    std::string buf_;
};

// Flushes f and closes it unless it is stdout. Returns false if any write to
// f, the final flush or the close failed.
inline bool finishOutput(FILE* f) {
    bool ok = std::fflush(f) == 0 && !std::ferror(f);
    if (f != stdout && std::fclose(f) != 0) ok = false;
    return ok;
}

}  // namespace halin
