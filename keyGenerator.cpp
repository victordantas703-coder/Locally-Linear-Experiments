#include "keyGenerator.h"

#include <array>
#include <stdexcept>

KeyGenerator::KeyGenerator(KeyPattern keyPattern, uint64_t seed)
    : pattern(keyPattern),
      index(0),
      currentClusterStart(0),
      currentClusterOffset(6),
      rng(seed),
      clusterStartDist(1000ULL, 999999999999ULL),
      cpfFirstDigitDist(1, 9),
      cpfDigitDist(0, 9) {
}

uint64_t KeyGenerator::nextRandomKey() {
    uint64_t key = 0;
    while (key == 0) {
        key = rng();
    }
    return key;
}

uint64_t KeyGenerator::nextClusteredKey() {
    constexpr int CLUSTER_SIZE = 6;

    if (currentClusterOffset >= CLUSTER_SIZE) {
        currentClusterStart = clusterStartDist(rng);
        currentClusterOffset = 0;
    }

    return currentClusterStart + static_cast<uint64_t>(currentClusterOffset++);
}

uint64_t KeyGenerator::nextCPFKey() {
    std::array<int, 11> cpf{};

    cpf[0] = cpfFirstDigitDist(rng);
    for (int i = 1; i < 9; i++) {
        cpf[i] = cpfDigitDist(rng);
    }

    int sum = 0;
    for (int i = 0; i < 9; i++) {
        sum += cpf[i] * (10 - i);
    }

    int remainder = sum % 11;
    cpf[9] = (remainder < 2) ? 0 : 11 - remainder;

    sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += cpf[i] * (11 - i);
    }

    remainder = sum % 11;
    cpf[10] = (remainder < 2) ? 0 : 11 - remainder;

    uint64_t number = 0;
    for (int digit : cpf) {
        number = number * 10ULL + static_cast<uint64_t>(digit);
    }

    return number;
}

uint64_t KeyGenerator::next() {
    index++;

    switch (pattern) {
    case KeyPattern::Random:
        return nextRandomKey();
    case KeyPattern::Sequential:
        return index;
    case KeyPattern::StepFive:
        return index * 5ULL;
    case KeyPattern::Clustered:
        return nextClusteredKey();
    case KeyPattern::CPF:
        return nextCPFKey();
    }

    throw std::logic_error("Unknown key pattern");
}

std::vector<KeyPatternInfo> availableKeyPatterns() {
    return {
        {KeyPattern::Random, "random", "Random keys"},
        {KeyPattern::Sequential, "sequential", "Sequential keys - 1 by 1"},
        {KeyPattern::StepFive, "step5", "Patterned keys - 5 by 5"},
        {KeyPattern::Clustered, "clustered", "Clustered random ranges - groups of 6"},
        {KeyPattern::CPF, "cpf", "CPF identification numbers"}
    };
}

std::vector<KeyPatternInfo> selectKeyPatterns(const std::string& requestedPattern) {
    std::vector<KeyPatternInfo> patterns = availableKeyPatterns();

    if (requestedPattern == "all") {
        return patterns;
    }

    for (const KeyPatternInfo& pattern : patterns) {
        if (pattern.id == requestedPattern) {
            return {pattern};
        }
    }

    throw std::invalid_argument(
        "Invalid key pattern. Use random, sequential, step5, clustered, cpf, or all."
    );
}
