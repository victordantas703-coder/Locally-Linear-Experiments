#include "locallyLinear.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

LocallyLinearHashTable::LocallyLinearHashTable(int n, int beta) {
    if (n <= 0 || beta <= 0) {
        throw std::invalid_argument("n and beta must be positive");
    }

    size = n;
    blockSize = beta;
    numBlocks = (size + blockSize - 1) / blockSize;
    table.assign(size, 0);
    occupied.assign(size, false);
    blockLoads.assign(numBlocks, 0);
    elementCount = 0;
}

int LocallyLinearHashTable::realBlockSize(int block) const {
    int blockBegin = block * blockSize;
    if (blockBegin >= size) {
        return 0;
    }
    return std::min(blockSize, size - blockBegin);
}

InsertResult LocallyLinearHashTable::insert(uint64_t key) {
    if (elementCount >= size) {
        return {false, 0};
    }

    int probes = 0;
    int h1 = static_cast<int>(hash1(key) % size);
    int h2 = static_cast<int>(hash2(key) % size);

    int block1 = h1 / blockSize;
    int block2 = h2 / blockSize;
    bool block1Full = blockLoads[block1] >= realBlockSize(block1);
    bool block2Full = blockLoads[block2] >= realBlockSize(block2);

    int chosenBlock;
    int startOffset;

    if (block1Full && !block2Full) {
        chosenBlock = block2;
        startOffset = h2 % blockSize;
    } else if (block2Full && !block1Full) {
        chosenBlock = block1;
        startOffset = h1 % blockSize;
    } else if (blockLoads[block1] < blockLoads[block2]) {
        chosenBlock = block1;
        startOffset = h1 % blockSize;
    } else if (blockLoads[block2] < blockLoads[block1]) {
        chosenBlock = block2;
        startOffset = h2 % blockSize;
    } else if (block1 == block2) {
        chosenBlock = block1;
        startOffset = (std::rand() & 1) ? (h1 % blockSize) : (h2 % blockSize);
    } else if (std::rand() & 1) {
        chosenBlock = block1;
        startOffset = h1 % blockSize;
    } else {
        chosenBlock = block2;
        startOffset = h2 % blockSize;
    }

    for (int jump = 0; jump < numBlocks; jump++) {
        int currentBlock = (chosenBlock + jump) % numBlocks;
        int currentBlockSize = realBlockSize(currentBlock);

        if (currentBlockSize == 0 || blockLoads[currentBlock] >= currentBlockSize) {
            continue;
        }

        int blockBegin = currentBlock * blockSize;
        int localStart = (jump == 0) ? (startOffset % currentBlockSize) : 0;

        for (int i = 0; i < currentBlockSize; i++) {
            probes++;
            int pos = blockBegin + ((localStart + i) % currentBlockSize);

            if (!occupied[pos]) {
                table[pos] = key;
                occupied[pos] = true;
                blockLoads[currentBlock]++;
                elementCount++;
                return {true, probes};
            }
        }
    }

    return {false, probes};
}

SearchResult LocallyLinearHashTable::search(uint64_t key) const {
    struct SearchPath {
        int block;
        int startOffset;
        int scanned;
        bool done;
        BlockScanResult result;
    };

    int probes = 0;
    int h1 = static_cast<int>(hash1(key) % size);
    int h2 = static_cast<int>(hash2(key) % size);

    int block1 = h1 / blockSize;
    int block2 = h2 / blockSize;
    int offset1 = h1 % blockSize;
    int offset2 = h2 % blockSize;

    auto preparePath = [this](SearchPath& path, int block, int startOffset) {
        path.block = block;
        path.startOffset = startOffset;
        path.scanned = 0;
        path.done = realBlockSize(block) == 0;
        path.result = path.done
            ? BlockScanResult::ReachedEmpty
            : BlockScanResult::ReachedEmpty;
    };

    auto probePath = [this, key, &probes](SearchPath& path) {
        if (path.done) {
            return path.result;
        }

        int currentBlockSize = realBlockSize(path.block);
        int blockBegin = path.block * blockSize;
        int localStart = path.startOffset % currentBlockSize;
        int pos = blockBegin + ((localStart + path.scanned) % currentBlockSize);

        probes++;

        if (occupied[pos] && table[pos] == key) {
            path.done = true;
            path.result = BlockScanResult::Found;
            return path.result;
        }

        if (!occupied[pos]) {
            path.done = true;
            path.result = BlockScanResult::ReachedEmpty;
            return path.result;
        }

        path.scanned++;
        if (path.scanned == currentBlockSize) {
            path.done = true;
            path.result = BlockScanResult::FullWithoutKey;
        }

        return path.result;
    };

    for (int jump = 0; jump < numBlocks; jump++) {
        int current1 = (block1 + jump) % numBlocks;
        int current2 = (block2 + jump) % numBlocks;

        int start1 = (jump == 0) ? offset1 : 0;
        int start2 = (jump == 0) ? offset2 : 0;

        SearchPath path1;
        SearchPath path2;
        preparePath(path1, current1, start1);
        preparePath(path2, current2, start2);

        while (!path1.done || !path2.done) {
            if (probePath(path1) == BlockScanResult::Found) {
                return {true, probes};
            }

            if (current1 == current2 &&
                start1 == start2 &&
                path1.scanned == path2.scanned &&
                path1.done == path2.done) {
                path2 = path1;
            }

            if (probePath(path2) == BlockScanResult::Found) {
                return {true, probes};
            }
        }

        if (path1.result == BlockScanResult::FullWithoutKey &&
            path2.result == BlockScanResult::FullWithoutKey) {
            continue;
        }

        return {false, probes};
    }

    return {false, probes};
}

double LocallyLinearHashTable::loadFactor() const {
    return static_cast<double>(elementCount) / size;
}

ClusterStats LocallyLinearHashTable::clusterStats() const {
    if (elementCount == 0) {
        return {0.0, 0};
    }

    if (elementCount == size) {
        return {static_cast<double>(size), size};
    }

    int firstEmpty = 0;
    while (firstEmpty < size && occupied[firstEmpty]) {
        firstEmpty++;
    }

    int clusterCount = 0;
    int totalClusterSize = 0;
    int maximumClusterSize = 0;
    int currentClusterSize = 0;

    for (int step = 1; step <= size; step++) {
        int pos = (firstEmpty + step) % size;

        if (occupied[pos]) {
            currentClusterSize++;
        } else if (currentClusterSize > 0) {
            clusterCount++;
            totalClusterSize += currentClusterSize;
            maximumClusterSize = std::max(maximumClusterSize, currentClusterSize);
            currentClusterSize = 0;
        }
    }

    if (clusterCount == 0) {
        return {0.0, 0};
    }

    return {
        static_cast<double>(totalClusterSize) / clusterCount,
        maximumClusterSize
    };
}
