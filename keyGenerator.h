#ifndef KEY_GENERATOR_H
#define KEY_GENERATOR_H

#include <cstdint>
#include <random>
#include <string>
#include <vector>

enum class KeyPattern {
    Random,
    Sequential,
    StepFive,
    Clustered,
    CPF
};

struct KeyPatternInfo {
    KeyPattern pattern;
    std::string id;
    std::string label;
};

class KeyGenerator {
private:
    KeyPattern pattern;
    uint64_t index;
    uint64_t currentClusterStart;
    int currentClusterOffset;
    std::mt19937_64 rng;
    std::uniform_int_distribution<uint64_t> clusterStartDist;
    std::uniform_int_distribution<int> cpfFirstDigitDist;
    std::uniform_int_distribution<int> cpfDigitDist;

    uint64_t nextRandomKey();
    uint64_t nextClusteredKey();
    uint64_t nextCPFKey();

public:
    KeyGenerator(KeyPattern keyPattern, uint64_t seed);
    uint64_t next();
};

std::vector<KeyPatternInfo> availableKeyPatterns();
std::vector<KeyPatternInfo> selectKeyPatterns(const std::string& requestedPattern);

#endif
