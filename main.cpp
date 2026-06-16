#include "locallyLinear.h"
#include "keyGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int DEFAULT_ITERATIONS = 1;
constexpr int DEFAULT_SIMULATIONS_PER_ITERATION = 100;
constexpr uint64_t BASE_SEED = 0xC0FFEE123456789ULL;

struct Config {
    int iterations = DEFAULT_ITERATIONS;
    int simulationsPerIteration = DEFAULT_SIMULATIONS_PER_ITERATION;
    int maxExponent = 22;
    std::string keyPattern = "all";
};

struct Metrics {
    double searchAvg = 0.0;
    double searchMax = 0.0;
    double insertAvg = 0.0;
    double insertMax = 0.0;
    double clusterAvg = 0.0;
    double clusterMax = 0.0;
};

struct ScenarioResult {
    std::string keyPatternLabel;
    int exponent;
    double alpha;
    int n;
    int m;
    int blockSize;
    Metrics metrics;
};

int locallyLinearBlockSize(int n, double alpha) {
    double beta = (1.0 / (1.0 - alpha)) * std::log2(std::log2(static_cast<double>(n)));
    return std::max(1, static_cast<int>(std::floor(beta)));
}

Metrics runSingleSimulation(
    int n,
    int m,
    int blockSize,
    KeyPattern keyPattern,
    uint64_t seed
) {
    KeyGenerator keyGenerator(keyPattern, seed);
    std::srand(static_cast<unsigned int>(seed));

    LocallyLinearHashTable table(n, blockSize);
    std::vector<uint64_t> insertedKeys;
    insertedKeys.reserve(m);

    long long insertProbeSum = 0;
    int maxInsertProbes = 0;

    for (int i = 0; i < m; i++) {
        uint64_t key = keyGenerator.next();
        InsertResult result = table.insert(key);

        if (!result.success) {
            std::cerr << "Insertion failed before reaching the requested load factor.\n";
            std::exit(EXIT_FAILURE);
        }

        insertedKeys.push_back(key);
        insertProbeSum += result.probes;
        maxInsertProbes = std::max(maxInsertProbes, result.probes);
    }

    long long searchProbeSum = 0;
    int maxSearchProbes = 0;

    for (uint64_t key : insertedKeys) {
        SearchResult result = table.search(key);

        if (!result.found) {
            std::cerr << "A previously inserted key was not found.\n";
            std::exit(EXIT_FAILURE);
        }

        searchProbeSum += result.probes;
        maxSearchProbes = std::max(maxSearchProbes, result.probes);
    }

    ClusterStats clusters = table.clusterStats();

    return {
        static_cast<double>(searchProbeSum) / m,
        static_cast<double>(maxSearchProbes),
        static_cast<double>(insertProbeSum) / m,
        static_cast<double>(maxInsertProbes),
        clusters.average,
        static_cast<double>(clusters.maximum)
    };
}

ScenarioResult runScenario(
    const KeyPatternInfo& keyPattern,
    int exponent,
    double alpha,
    const Config& config
) {
    int n = 1 << exponent;
    int m = static_cast<int>(std::floor(alpha * n));
    int blockSize = locallyLinearBlockSize(n, alpha);

    Metrics total;

    for (int iteration = 0; iteration < config.iterations; iteration++) {
        Metrics iterationTotal;

        for (int simulation = 0; simulation < config.simulationsPerIteration; simulation++) {
            uint64_t simulationIndex =
                static_cast<uint64_t>(exponent) * 1000000ULL +
                static_cast<uint64_t>(alpha * 10.0) * 100000ULL +
                static_cast<uint64_t>(iteration) * config.simulationsPerIteration +
                static_cast<uint64_t>(simulation);

            Metrics current = runSingleSimulation(
                n,
                m,
                blockSize,
                keyPattern.pattern,
                BASE_SEED + simulationIndex
            );

            iterationTotal.searchAvg += current.searchAvg;
            iterationTotal.searchMax += current.searchMax;
            iterationTotal.insertAvg += current.insertAvg;
            iterationTotal.insertMax += current.insertMax;
            iterationTotal.clusterAvg += current.clusterAvg;
            iterationTotal.clusterMax += current.clusterMax;
        }

        total.searchAvg += iterationTotal.searchAvg / config.simulationsPerIteration;
        total.searchMax += iterationTotal.searchMax / config.simulationsPerIteration;
        total.insertAvg += iterationTotal.insertAvg / config.simulationsPerIteration;
        total.insertMax += iterationTotal.insertMax / config.simulationsPerIteration;
        total.clusterAvg += iterationTotal.clusterAvg / config.simulationsPerIteration;
        total.clusterMax += iterationTotal.clusterMax / config.simulationsPerIteration;

        std::cout << "Finished n=2^" << exponent
                  << ", alpha=" << alpha
                  << ", iteration " << (iteration + 1)
                  << "/" << config.iterations << '\n';
    }

    total.searchAvg /= config.iterations;
    total.searchMax /= config.iterations;
    total.insertAvg /= config.iterations;
    total.insertMax /= config.iterations;
    total.clusterAvg /= config.iterations;
    total.clusterMax /= config.iterations;

    return {keyPattern.label, exponent, alpha, n, m, blockSize, total};
}

void printSearchTable(
    const std::string& keyPatternLabel,
    const std::vector<ScenarioResult>& results
) {
    std::cout << "\nSuccessful search time - LOCALLYLINEAR - "
              << keyPatternLabel << '\n';
    std::cout << "n\talpha\tbeta\tAvg\tMax\n";

    for (const ScenarioResult& result : results) {
        std::cout << "2^" << result.exponent << '\t'
                  << result.alpha << '\t'
                  << result.blockSize << '\t'
                  << result.metrics.searchAvg << '\t'
                  << result.metrics.searchMax << '\n';
    }
}

void printInsertTable(
    const std::string& keyPatternLabel,
    const std::vector<ScenarioResult>& results
) {
    std::cout << "\nInsert time - LOCALLYLINEAR - "
              << keyPatternLabel << '\n';
    std::cout << "n\talpha\tbeta\tAvg\tMax\n";

    for (const ScenarioResult& result : results) {
        std::cout << "2^" << result.exponent << '\t'
                  << result.alpha << '\t'
                  << result.blockSize << '\t'
                  << result.metrics.insertAvg << '\t'
                  << result.metrics.insertMax << '\n';
    }
}

void printClusterTable(
    const std::string& keyPatternLabel,
    const std::vector<ScenarioResult>& results
) {
    std::cout << "\nCluster size - LOCALLYLINEAR - "
              << keyPatternLabel << '\n';
    std::cout << "n\talpha\tbeta\tAvg\tMax\n";

    for (const ScenarioResult& result : results) {
        std::cout << "2^" << result.exponent << '\t'
                  << result.alpha << '\t'
                  << result.blockSize << '\t'
                  << result.metrics.clusterAvg << '\t'
                  << result.metrics.clusterMax << '\n';
    }
}

Config parseConfig(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; i++) {
        std::string option = argv[i];

        if (option == "--iterations" && i + 1 < argc) {
            config.iterations = std::max(1, std::stoi(argv[++i]));
        } else if (option == "--simulations" && i + 1 < argc) {
            config.simulationsPerIteration = std::max(1, std::stoi(argv[++i]));
        } else if (option == "--max-exponent" && i + 1 < argc) {
            config.maxExponent = std::max(8, std::stoi(argv[++i]));
        } else if (option == "--key-type" && i + 1 < argc) {
            config.keyPattern = argv[++i];
        }
    }

    return config;
}
}

int main(int argc, char* argv[]) {
    Config config = parseConfig(argc, argv);
    const std::vector<int> exponents = {8, 12, 16, 20, 22};
    const std::vector<double> alphas = {0.4, 0.9};
    const std::vector<KeyPatternInfo> keyPatterns = selectKeyPatterns(config.keyPattern);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "LOCALLYLINEAR simulation based on Section 6\n";
    std::cout << "Iterations: " << config.iterations << '\n';
    std::cout << "Simulations per iteration: " << config.simulationsPerIteration << '\n';
    std::cout << "Key type: " << config.keyPattern << '\n';
    std::cout << "Available key types: random, sequential, step5, clustered, cpf, all\n";
    std::cout << "Block size: floor((1 - alpha)^-1 * log2(log2(n)))\n\n";

    for (const KeyPatternInfo& keyPattern : keyPatterns) {
        std::vector<ScenarioResult> results;
        results.reserve(exponents.size() * alphas.size());

        std::cout << "\nRunning key pattern: " << keyPattern.label << "\n\n";

        for (int exponent : exponents) {
            if (exponent > config.maxExponent) {
                continue;
            }

            for (double alpha : alphas) {
                results.push_back(runScenario(keyPattern, exponent, alpha, config));
            }
        }

        printSearchTable(keyPattern.label, results);
        printInsertTable(keyPattern.label, results);
        printClusterTable(keyPattern.label, results);
    }

    return 0;
}
