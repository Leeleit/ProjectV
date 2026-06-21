CXX ?= clang++
CXXFLAGS ?= -std=c++26 -O3 -march=native -DNDEBUG -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS ?= -lvulkan -lpthread

SHADERS := \
    shaders/voxel_a.vert.spv \
    shaders/voxel_a.frag.spv \
    shaders/voxel_b.vert.spv \
    shaders/voxel_b.frag.spv \
    shaders/taa_resolve_a.comp.spv \
    shaders/taa_resolve_b.comp.spv

HEADERS := $(SHADERS:.spv=.spv.h)

all: taa_bench

taa_bench: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -I. -o $@ main.cpp $(LDFLAGS)

%.spv: %
	glslc --target-env=vulkan1.4 -O $< -o $@

%.spv.h: %.spv
	xxd -i $< | sed 's/\([0-9a-zA-Z_]\)voxel_a_vert/\1voxel_a_vert_spv/g; s/\([0-9a-zA-Z_]\)voxel_a_frag/\1voxel_a_frag_spv/g; s/\([0-9a-zA-Z_]\)voxel_b_vert/\1voxel_b_vert_spv/g; s/\([0-9a-zA-Z_]\)voxel_b_frag/\1voxel_b_frag_spv/g; s/\([0-9a-zA-Z_]\)taa_resolve_a/\1taa_resolve_a_comp_spv/g; s/\([0-9a-zA-Z_]\)taa_resolve_b/\1taa_resolve_b_comp_spv/g' > $@

clean:
	rm -f taa_bench $(SHADERS) $(HEADERS)

.PHONY: all clean
.DEFAULT_GOAL := taa_bench
