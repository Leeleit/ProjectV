#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>
#include <fstream>
#include <string>
#include <limits>
#include <map>
#include <tuple>

struct Vec3 {
    float x, y, z;
};

struct AudioSource {
    int id;
    Vec3 pos;
    float volume;
    float frequency;
    int priority;
};

struct OcclusionCache {
    float factor;
    int lastUpdateTick;
};

struct SpatialCell {
    Vec3 centroid;
    float totalVolume;
    std::vector<int> sourceIds;
};

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

Stats Compute(const std::vector<double>& samples) {
    Stats s{};
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

inline bool RaycastDDA(const Vec3& start, const Vec3& end, const uint8_t* grid) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dz = end.z - start.z;
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 0.001f) return false;

    dx /= len; dy /= len; dz /= len;

    int x = std::clamp(static_cast<int>(start.x), 0, 127);
    int y = std::clamp(static_cast<int>(start.y), 0, 127);
    int z = std::clamp(static_cast<int>(start.z), 0, 31);

    int targetX = std::clamp(static_cast<int>(end.x), 0, 127);
    int targetY = std::clamp(static_cast<int>(end.y), 0, 127);
    int targetZ = std::clamp(static_cast<int>(end.z), 0, 31);

    int stepX = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    int stepY = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    int stepZ = (dz > 0) ? 1 : ((dz < 0) ? -1 : 0);

    float tMaxX = (dx != 0.0f) ? ((x + (stepX > 0 ? 1 : 0) - start.x) / dx) : std::numeric_limits<float>::max();
    float tMaxY = (dy != 0.0f) ? ((y + (stepY > 0 ? 1 : 0) - start.y) / dy) : std::numeric_limits<float>::max();
    float tMaxZ = (dz != 0.0f) ? ((z + (stepZ > 0 ? 1 : 0) - start.z) / dz) : std::numeric_limits<float>::max();

    float tDeltaX = (dx != 0.0f) ? std::abs(1.0f / dx) : std::numeric_limits<float>::max();
    float tDeltaY = (dy != 0.0f) ? std::abs(1.0f / dy) : std::numeric_limits<float>::max();
    float tDeltaZ = (dz != 0.0f) ? std::abs(1.0f / dz) : std::numeric_limits<float>::max();

    while (true) {
        if (x < 0 || x >= 128 || y < 0 || y >= 128 || z < 0 || z >= 32) break;
        if (grid[z * 16384 + y * 128 + x] != 0) {
            return true;
        }
        if (x == targetX && y == targetY && z == targetZ) break;

        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX;
                tMaxX += tDeltaX;
            } else {
                z += stepZ;
                tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY;
                tMaxY += tDeltaY;
            } else {
                z += stepZ;
                tMaxZ += tDeltaZ;
            }
        }
    }
    return false;
}

void InitGrid(int sceneId, uint8_t* grid) {
    std::fill(grid, grid + 524288, 0);
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            grid[y * 128 + x] = 1;
        }
    }

    if (sceneId == 2) {
        std::mt19937 gen(42);
        std::uniform_int_distribution<> dis(5, 120);
        for (int i = 0; i < 200; ++i) {
            int cx = dis(gen);
            int cy = dis(gen);
            for (int z = 1; z < 8; ++z) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int px = cx + dx;
                        int py = cy + dy;
                        if (px >= 0 && px < 128 && py >= 0 && py < 128) {
                            grid[z * 16384 + py * 128 + px] = (z < 5) ? 2 : 3;
                        }
                    }
                }
            }
        }
    } else if (sceneId == 3) {
        std::mt19937 gen(1337);
        std::uniform_int_distribution<> dis(10, 110);
        std::uniform_int_distribution<> sizeDis(6, 15);
        for (int i = 0; i < 40; ++i) {
            int bx = dis(gen);
            int by = dis(gen);
            int sx = sizeDis(gen);
            int sy = sizeDis(gen);
            for (int z = 1; z < 15; ++z) {
                for (int y = by; y < by + sy; ++y) {
                    for (int x = bx; x < bx + sx; ++x) {
                        if (x >= 0 && x < 128 && y >= 0 && y < 128) {
                            if (x == bx || x == bx + sx - 1 || y == by || y == by + sy - 1) {
                                grid[z * 16384 + y * 128 + x] = 4;
                            }
                        }
                    }
                }
            }
        }
    } else if (sceneId == 4) {
        for (int z = 1; z <= 3; ++z) {
            for (int y = 0; y < 128; ++y) {
                for (int x = 0; x < 128; ++x) {
                    grid[z * 16384 + y * 128 + x] = 1;
                }
            }
        }
        for (int i = 20; i < 128; i += 30) {
            for (int y = 0; y < 128; ++y) {
                for (int z = 1; z <= 3; ++z) {
                    grid[z * 16384 + y * 128 + i] = 0;
                    grid[z * 16384 + y * 128 + i + 1] = 0;
                }
            }
        }
    } else if (sceneId == 5) {
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                float height = 2.0f + 3.0f * std::sin(x * 0.1f) * std::cos(y * 0.1f);
                int maxZ = std::clamp(static_cast<int>(height), 0, 31);
                for (int z = 1; z <= maxZ; ++z) {
                    grid[z * 16384 + y * 128 + x] = 1;
                }
            }
        }
    }
}

void GenerateSources(int sceneId, int seed, std::vector<AudioSource>& sources) {
    sources.clear();
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> posDis(5.0f, 123.0f);
    std::uniform_real_distribution<float> volDis(0.2f, 1.0f);
    std::uniform_real_distribution<float> freqDis(100.0f, 4000.0f);
    std::uniform_int_distribution<> prioDis(0, 3);

    if (sceneId != 5) {
        for (int i = 0; i < 1000; ++i) {
            AudioSource src;
            src.id = i;
            src.pos.x = posDis(gen);
            src.pos.y = posDis(gen);
            src.pos.z = 2.0f;
            src.volume = volDis(gen);
            src.frequency = freqDis(gen);
            src.priority = prioDis(gen);
            sources.push_back(src);
        }
    } else {
        std::uniform_real_distribution<float> airZDis(15.0f, 28.0f);
        for (int i = 0; i < 1000; ++i) {
            AudioSource src;
            src.id = i;
            src.pos.x = posDis(gen);
            src.pos.y = posDis(gen);
            if (i < 100) {
                src.pos.z = airZDis(gen);
                src.volume = 1.0f;
                src.frequency = 300.0f;
                src.priority = 3;
            } else if (i < 300) {
                src.pos.z = 2.0f;
                src.volume = 0.9f;
                src.frequency = 120.0f;
                src.priority = 2;
            } else {
                src.pos.z = 1.0f;
                src.volume = volDis(gen) * 0.4f;
                src.frequency = freqDis(gen) * 1.5f;
                src.priority = prioDis(gen) % 2;
            }
            sources.push_back(src);
        }
    }
}

float ProcessFakeDSP(float gain, float freq, int sampleCount) {
    float sum = 0.0f;
    float phase = 0.0f;
    float step = freq * 2.0f * 3.14159f / 44100.0f;
    for (int i = 0; i < sampleCount; ++i) {
        sum += std::sin(phase) * gain;
        phase += step;
    }
    return sum;
}

std::pair<double, float> RunNaive(const std::vector<AudioSource>& sources, const Vec3& listener, const uint8_t* grid, int& physicalCount, int& virtualCount) {
    auto start = std::chrono::high_resolution_clock::now();
    float totalMixedValue = 0.0f;
    physicalCount = 0;
    virtualCount = 0;
    for (const auto& src : sources) {
        float dx = src.pos.x - listener.x;
        float dy = src.pos.y - listener.y;
        float dz = src.pos.z - listener.z;
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);
        float gain = src.volume / (1.0f + 0.1f * d);
        if (gain > 1.0f) gain = 1.0f;
        bool occluded = RaycastDDA(listener, src.pos, grid);
        if (occluded) {
            gain *= 0.15f;
        }
        totalMixedValue += ProcessFakeDSP(gain, src.frequency, 128);
        physicalCount++;
    }
    auto end = std::chrono::high_resolution_clock::now();
    return { std::chrono::duration<double, std::nano>(end - start).count(), totalMixedValue };
}

std::pair<double, float> RunDistanceLOD(const std::vector<AudioSource>& sources, const Vec3& listener, const uint8_t* grid, int& physicalCount, int& virtualCount) {
    auto start = std::chrono::high_resolution_clock::now();
    float totalMixedValue = 0.0f;
    physicalCount = 0;
    virtualCount = 0;
    float farEnergySum = 0.0f;
    float farFreqAvg = 0.0f;
    int farCount = 0;

    for (const auto& src : sources) {
        float dx = src.pos.x - listener.x;
        float dy = src.pos.y - listener.y;
        float dz = src.pos.z - listener.z;
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (d < 30.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            bool occluded = RaycastDDA(listener, src.pos, grid);
            if (occluded) gain *= 0.15f;
            totalMixedValue += ProcessFakeDSP(gain, src.frequency, 128);
            physicalCount++;
        } else if (d < 120.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            totalMixedValue += ProcessFakeDSP(gain, src.frequency, 64);
            physicalCount++;
        } else {
            float gain = src.volume / (1.0f + 0.1f * d);
            farEnergySum += gain;
            farFreqAvg += src.frequency;
            farCount++;
            virtualCount++;
        }
    }

    if (farCount > 0) {
        farFreqAvg /= farCount;
        totalMixedValue += ProcessFakeDSP(farEnergySum / farCount, farFreqAvg, 32);
        physicalCount++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    return { std::chrono::duration<double, std::nano>(end - start).count(), totalMixedValue };
}

std::pair<double, float> RunOcclusionCache(const std::vector<AudioSource>& sources, const Vec3& listener, const uint8_t* grid, std::vector<OcclusionCache>& cache, int tick, int& physicalCount, int& virtualCount) {
    auto start = std::chrono::high_resolution_clock::now();
    float totalMixedValue = 0.0f;
    physicalCount = 0;
    virtualCount = 0;
    float farEnergySum = 0.0f;
    float farFreqAvg = 0.0f;
    int farCount = 0;

    for (const auto& src : sources) {
        float dx = src.pos.x - listener.x;
        float dy = src.pos.y - listener.y;
        float dz = src.pos.z - listener.z;
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (d < 30.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            int idx = src.id;
            if ((tick + idx) % 6 == 0 || cache[idx].lastUpdateTick == -1) {
                bool occluded = RaycastDDA(listener, src.pos, grid);
                cache[idx].factor = occluded ? 0.15f : 1.0f;
                cache[idx].lastUpdateTick = tick;
            }
            gain *= cache[idx].factor;
            totalMixedValue += ProcessFakeDSP(gain, src.frequency, 128);
            physicalCount++;
        } else if (d < 120.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            totalMixedValue += ProcessFakeDSP(gain, src.frequency, 64);
            physicalCount++;
        } else {
            float gain = src.volume / (1.0f + 0.1f * d);
            farEnergySum += gain;
            farFreqAvg += src.frequency;
            farCount++;
            virtualCount++;
        }
    }

    if (farCount > 0) {
        farFreqAvg /= farCount;
        totalMixedValue += ProcessFakeDSP(farEnergySum / farCount, farFreqAvg, 32);
        physicalCount++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    return { std::chrono::duration<double, std::nano>(end - start).count(), totalMixedValue };
}

std::pair<double, float> RunSpatialGrid(const std::vector<AudioSource>& sources, const Vec3& listener, const uint8_t* grid, std::vector<OcclusionCache>& cache, int tick, int& physicalCount, int& virtualCount) {
    auto start = std::chrono::high_resolution_clock::now();
    float totalMixedValue = 0.0f;
    physicalCount = 0;
    virtualCount = 0;

    std::map<uint32_t, SpatialCell> cellMap;

    for (const auto& src : sources) {
        float dx = src.pos.x - listener.x;
        float dy = src.pos.y - listener.y;
        float dz = src.pos.z - listener.z;
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (d < 30.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            int idx = src.id;
            if ((tick + idx) % 6 == 0 || cache[idx].lastUpdateTick == -1) {
                bool occluded = RaycastDDA(listener, src.pos, grid);
                cache[idx].factor = occluded ? 0.15f : 1.0f;
                cache[idx].lastUpdateTick = tick;
            }
            gain *= cache[idx].factor;
            totalMixedValue += ProcessFakeDSP(gain, src.frequency, 128);
            physicalCount++;
        } else if (d < 120.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            totalMixedValue += ProcessFakeDSP(gain, src.frequency, 64);
            physicalCount++;
        } else {
            int gx = static_cast<int>(src.pos.x / 16.0f);
            int gy = static_cast<int>(src.pos.y / 16.0f);
            int gz = static_cast<int>(src.pos.z / 16.0f);
            uint32_t cellKey = (gx & 0xFF) | ((gy & 0xFF) << 8) | ((gz & 0xFF) << 16);
            auto& cell = cellMap[cellKey];
            cell.centroid.x += src.pos.x * src.volume;
            cell.centroid.y += src.pos.y * src.volume;
            cell.centroid.z += src.pos.z * src.volume;
            cell.totalVolume += src.volume;
            cell.sourceIds.push_back(src.id);
            virtualCount++;
        }
    }

    for (auto& pair : cellMap) {
        auto& cell = pair.second;
        if (cell.totalVolume > 0.0f) {
            cell.centroid.x /= cell.totalVolume;
            cell.centroid.y /= cell.totalVolume;
            cell.centroid.z /= cell.totalVolume;
            float dx = cell.centroid.x - listener.x;
            float dy = cell.centroid.y - listener.y;
            float dz = cell.centroid.z - listener.z;
            float d = std::sqrt(dx*dx + dy*dy + dz*dz);
            float gain = cell.totalVolume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            float avgFreq = 0.0f;
            for (int id : cell.sourceIds) {
                avgFreq += sources[id].frequency;
            }
            avgFreq /= cell.sourceIds.size();
            totalMixedValue += ProcessFakeDSP(gain, avgFreq, 32);
            physicalCount++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    return { std::chrono::duration<double, std::nano>(end - start).count(), totalMixedValue };
}

std::pair<double, float> RunHybridGPU(const std::vector<AudioSource>& sources, const Vec3& listener, const uint8_t* grid, std::vector<OcclusionCache>& cache, int tick, int& physicalCount, int& virtualCount) {
    auto start = std::chrono::high_resolution_clock::now();
    float totalMixedValue = 0.0f;
    physicalCount = 0;
    virtualCount = 0;

    std::vector<float> packedGains;
    std::vector<float> packedFreqs;
    std::vector<int> packedSampleCounts;
    packedGains.reserve(1000);
    packedFreqs.reserve(1000);
    packedSampleCounts.reserve(1000);

    for (const auto& src : sources) {
        float dx = src.pos.x - listener.x;
        float dy = src.pos.y - listener.y;
        float dz = src.pos.z - listener.z;
        float d = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (d < 30.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            int idx = src.id;
            if ((tick + idx) % 6 == 0 || cache[idx].lastUpdateTick == -1) {
                bool occluded = RaycastDDA(listener, src.pos, grid);
                cache[idx].factor = occluded ? 0.15f : 1.0f;
                cache[idx].lastUpdateTick = tick;
            }
            gain *= cache[idx].factor;
            packedGains.push_back(gain);
            packedFreqs.push_back(src.frequency);
            packedSampleCounts.push_back(128);
            physicalCount++;
        } else if (d < 120.0f) {
            float gain = src.volume / (1.0f + 0.1f * d);
            if (gain > 1.0f) gain = 1.0f;
            packedGains.push_back(gain);
            packedFreqs.push_back(src.frequency);
            packedSampleCounts.push_back(64);
            physicalCount++;
        } else {
            float gain = src.volume / (1.0f + 0.1f * d);
            packedGains.push_back(gain * 0.1f);
            packedFreqs.push_back(src.frequency);
            packedSampleCounts.push_back(16);
            virtualCount++;
        }
    }

    int batchSize = packedGains.size();
    for (int i = 0; i < batchSize; ++i) {
        totalMixedValue += ProcessFakeDSP(packedGains[i], packedFreqs[i], packedSampleCounts[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    return { std::chrono::duration<double, std::nano>(end - start).count(), totalMixedValue };
}

int main() {
    uint8_t* grid = new uint8_t[524288];
    Vec3 listener = {64.0f, 64.0f, 2.0f};

    std::vector<std::string> strategies = {"A_Naive_NoLOD", "B_Distance_LOD", "C_OcclusionCache_Raycast", "D_SpatialGrid_Binning", "E_Hybrid_LOD_GPU"};
    std::vector<std::string> scenes = {"s1_open_field", "s2_dense_forest", "s3_urban_ruins", "s4_trench_network", "s5_combined_arms"};
    std::vector<int> seeds = {1, 7, 42, 1234, 31337};

    std::ofstream resultsFile("results.csv");
    resultsFile << "Strategy,Scene,Seed,Mean_ns,Median_ns,P95_ns,P99_ns,StdDev_ns,Min_ns,Max_ns,Plausibility,PhysicalVoices,VirtualVoices\n";

    std::map<std::string, std::vector<double>> strategyGains;

    for (int stratIdx = 0; stratIdx < 5; ++stratIdx) {
        for (int sceneIdx = 0; sceneIdx < 5; ++sceneIdx) {
            InitGrid(sceneIdx + 1, grid);
            for (int seedIdx = 0; seedIdx < 5; ++seedIdx) {
                std::vector<AudioSource> sources;
                GenerateSources(sceneIdx + 1, seeds[seedIdx], sources);

                std::vector<OcclusionCache> cache(1000, {1.0f, -1});

                int physicalCount = 0;
                int virtualCount = 0;
                for (int i = 0; i < 10; ++i) {
                    if (stratIdx == 0) RunNaive(sources, listener, grid, physicalCount, virtualCount);
                    else if (stratIdx == 1) RunDistanceLOD(sources, listener, grid, physicalCount, virtualCount);
                    else if (stratIdx == 2) RunOcclusionCache(sources, listener, grid, cache, i, physicalCount, virtualCount);
                    else if (stratIdx == 3) RunSpatialGrid(sources, listener, grid, cache, i, physicalCount, virtualCount);
                    else if (stratIdx == 4) RunHybridGPU(sources, listener, grid, cache, i, physicalCount, virtualCount);
                }

                std::vector<double> samples;
                samples.reserve(1000);

                float baselineMixedValue = 0.0f;
                float currentMixedValue = 0.0f;

                for (int i = 0; i < 1000; ++i) {
                    std::pair<double, float> res;
                    if (stratIdx == 0) {
                        res = RunNaive(sources, listener, grid, physicalCount, virtualCount);
                        baselineMixedValue = res.second;
                    }
                    else if (stratIdx == 1) res = RunDistanceLOD(sources, listener, grid, physicalCount, virtualCount);
                    else if (stratIdx == 2) res = RunOcclusionCache(sources, listener, grid, cache, i + 10, physicalCount, virtualCount);
                    else if (stratIdx == 3) res = RunSpatialGrid(sources, listener, grid, cache, i + 10, physicalCount, virtualCount);
                    else if (stratIdx == 4) res = RunHybridGPU(sources, listener, grid, cache, i + 10, physicalCount, virtualCount);

                    samples.push_back(res.first);
                    currentMixedValue = res.second;
                }

                if (stratIdx != 0) {
                    int dummyPhys = 0, dummyVirt = 0;
                    auto baseRes = RunNaive(sources, listener, grid, dummyPhys, dummyVirt);
                    baselineMixedValue = baseRes.second;
                }

                float plausibility = 0.0f;
                if (std::abs(baselineMixedValue) > 0.001f) {
                    float diff = std::abs(baselineMixedValue - currentMixedValue);
                    plausibility = 1.0f - (diff / std::abs(baselineMixedValue));
                    if (plausibility < 0.0f) plausibility = 0.0f;
                } else {
                    plausibility = 1.0f;
                }

                Stats stats = Compute(samples);

                resultsFile << strategies[stratIdx] << ","
                            << scenes[sceneIdx] << ","
                            << seeds[seedIdx] << ","
                            << stats.mean << ","
                            << stats.median << ","
                            << stats.p95 << ","
                            << stats.p99 << ","
                            << stats.stddev << ","
                            << stats.min << ","
                            << stats.max << ","
                            << plausibility << ","
                            << physicalCount << ","
                            << virtualCount << "\n";
            }
        }
    }
    resultsFile.close();

    std::map<std::string, double> meanSum;
    std::map<std::string, int> meanCount;
    std::ifstream checkFile("results.csv");
    std::string line;
    std::getline(checkFile, line);
    while (std::getline(checkFile, line)) {
        size_t firstComma = line.find(',');
        std::string strat = line.substr(0, firstComma);
        size_t nextComma = line.find(',', firstComma + 1);
        nextComma = line.find(',', nextComma + 1);
        size_t meanStart = nextComma + 1;
        size_t meanEnd = line.find(',', meanStart);
        double mean = std::stod(line.substr(meanStart, meanEnd - meanStart));
        meanSum[strat] += mean;
        meanCount[strat]++;
    }
    checkFile.close();

    std::ofstream summaryFile("summary_means.csv");
    summaryFile << "Strategy,Mean_ns\n";
    for (const auto& strat : strategies) {
        summaryFile << strat << "," << (meanSum[strat] / meanCount[strat]) << "\n";
    }
    summaryFile.close();

    delete[] grid;
    return 0;
}
