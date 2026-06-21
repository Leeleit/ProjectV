# Makefile — `2026-06-21-audio-diffraction-hybrid` prototype
#
# Build:  `make`
# Run:    `./bench results.csv`
# Clean:  `make clean`
#
# Per `docs/experiments/benchmarks/methodology.md §2`:
#   Clang 22.1.6 (per `hardware-profile.md §6`), -O3 -march=native -std=c++26 -DNDEBUG
#
# Per `hardware-profile.md §1`: Zen 3 5800X. No AVX-512 (Zen 3 limit). -march=native
#   generates AVX2 + FMA + BMI1/2 — appropriate for dev host.
# Governor: `powersave` (per `hardware-profile.md §1`). ProjectV release preset would use
#   `-march=x86-64-v3` for cross-host portability (per `agent/knowledge.md §17`), but research
#   prototype uses `-march=native` for max measurement precision on dev host.
#

CXX        := clang++
CXXFLAGS   := -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
LDFLAGS    :=
TARGET     := bench
SOURCES    := bench.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) voxel_grid.hpp audio_path.hpp diffraction.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) results.csv

clean:
	rm -f $(TARGET) results.csv
