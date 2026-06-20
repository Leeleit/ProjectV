CXX ?= clang++
CXXFLAGS ?= -std=c++26 -O3 -march=native -DNDEBUG -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations
LDFLAGS ?= -lvulkan -lpthread

SHADERS := \
    shaders/graphics.vert.spv \
    shaders/graphics.frag.spv \
    shaders/compute_a.comp.spv \
    shaders/compute_b.comp.spv \
    shaders/compute_c.comp.spv

HEADERS := $(SHADERS:.spv=.spv.h)

all: async_bench

async_bench: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -I. -o $@ main.cpp $(LDFLAGS)

%.spv: %
	glslc --target-env=vulkan1.4 -O $< -o $@

%.spv.h: %.spv
	xxd -i $< | sed 's/\([^_]\)compute_a/\1compute_a_comp/g; s/\([^_]\)compute_b/\1compute_b_comp/g; s/\([^_]\)compute_c/\1compute_c_comp/g; s/\([^_]\)graphics_vert/\1graphics_vert_spv/g; s/\([^_]\)graphics_frag/\1graphics_frag_spv/g' > $@

clean:
	rm -f async_bench $(SHADERS) $(HEADERS)

.PHONY: all clean
.DEFAULT_GOAL := async_bench