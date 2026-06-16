#ifndef LOCALLY_LINEAR_H
#define LOCALLY_LINEAR_H

#include <vector>
#include <cstdint>

struct InsertResult {
    bool success;
    int probes;
};

struct SearchResult {
    bool found;
    int probes;
};

struct ClusterStats {
    double average;
    int maximum;
};

class LocallyLinearHashTable {
private:
    enum class BlockScanResult {
        Found,
        ReachedEmpty,
        FullWithoutKey
    };

    int size;
    int blockSize;
    int numBlocks;

    std::vector<uint64_t> table;
    std::vector<bool> occupied;
    std::vector<int> blockLoads;

    int elementCount;

    static uint64_t hash1(uint64_t x) {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    static uint64_t hash2(uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    int realBlockSize(int block) const;

public:
    LocallyLinearHashTable(int n, int beta);
    InsertResult insert(uint64_t key);
    SearchResult search(uint64_t key) const;
    double loadFactor() const;
    ClusterStats clusterStats() const;
};

#endif
