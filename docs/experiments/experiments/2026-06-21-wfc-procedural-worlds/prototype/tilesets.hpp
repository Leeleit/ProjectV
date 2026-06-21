#pragma once
// Tileset definitions: A (cave) + B (biome).
// 8 tiles each, 3-axis neighbor rules (+X, +Y, +Z). Negative axis uses mirror assumption.
#include "wfc.hpp"

namespace wfc::tilesets {

inline Tileset make_cave() {
    // Tiles: 0=stone 1=air 2=water 3=lava 4=gravel 5=sand 6=ore_coal 7=ore_iron
    Tileset t;
    t.tile_count = 8;
    int w[8] = {30, 25, 8, 4, 12, 8, 8, 5};
    for (int i = 0; i < 8; ++i) t.weights[i] = w[i];

    auto allow = [&](int a, int axis, int b) {
        t.adjacency[a][axis][b] = 1;
    };

    // Stone is universal — touches everything.
    for (int b = 0; b < 8; ++b) for (int axis = 0; axis < 3; ++axis) allow(0, axis, b);
    // Air — touches air, water surface, sand, gravel.
    for (int b : {1, 2, 4, 5}) for (int axis = 0; axis < 3; ++axis) allow(1, axis, b);
    // Water — touches water, stone, sand, gravel (settles).
    for (int b : {0, 2, 4, 5}) for (int axis = 0; axis < 3; ++axis) allow(2, axis, b);
    // Lava — touches lava, stone, ore (rare surface).
    for (int b : {0, 3, 6, 7}) for (int axis = 0; axis < 3; ++axis) allow(3, axis, b);
    // Gravel — touches gravel, stone, water, sand.
    for (int b : {0, 2, 4, 5}) for (int axis = 0; axis < 3; ++axis) allow(4, axis, b);
    // Sand — touches sand, stone, water, gravel (beach).
    for (int b : {0, 2, 4, 5}) for (int axis = 0; axis < 3; ++axis) allow(5, axis, b);
    // Ore coal — touches stone, ore_iron (vein).
    for (int b : {0, 6, 7}) for (int axis = 0; axis < 3; ++axis) allow(6, axis, b);
    // Ore iron — touches stone, ore_coal (vein).
    for (int b : {0, 6, 7}) for (int axis = 0; axis < 3; ++axis) allow(7, axis, b);

    return t;
}

inline Tileset make_biome() {
    // Tiles: 0=forest 1=desert 2=tundra 3=savanna 4=mountain 5=swamp 6=jungle 7=plains
    Tileset t;
    t.tile_count = 8;
    int w[8] = {25, 10, 8, 10, 8, 8, 8, 23};
    for (int i = 0; i < 8; ++i) t.weights[i] = w[i];

    auto allow = [&](int a, int axis, int b) {
        t.adjacency[a][axis][b] = 1;
    };

    // Forest — touches forest, jungle, plains, swamp.
    for (int b : {0, 6, 7, 5}) for (int axis = 0; axis < 3; ++axis) allow(0, axis, b);
    // Desert — touches desert, savanna, plains, mountain (arid edge).
    for (int b : {1, 3, 7, 4}) for (int axis = 0; axis < 3; ++axis) allow(1, axis, b);
    // Tundra — touches tundra, mountain, plains, forest (cold edge).
    for (int b : {2, 4, 7, 0}) for (int axis = 0; axis < 3; ++axis) allow(2, axis, b);
    // Savanna — touches savanna, desert, plains, jungle.
    for (int b : {3, 1, 7, 6}) for (int axis = 0; axis < 3; ++axis) allow(3, axis, b);
    // Mountain — touches mountain, tundra, desert, forest, plains.
    for (int b : {4, 2, 1, 0, 7}) for (int axis = 0; axis < 3; ++axis) allow(4, axis, b);
    // Swamp — touches swamp, forest, jungle, plains.
    for (int b : {5, 0, 6, 7}) for (int axis = 0; axis < 3; ++axis) allow(5, axis, b);
    // Jungle — touches jungle, forest, swamp, savanna.
    for (int b : {6, 0, 5, 3}) for (int axis = 0; axis < 3; ++axis) allow(6, axis, b);
    // Plains — touches plains (universal buffer between biomes).
    for (int b = 0; b < 8; ++b) for (int axis = 0; axis < 3; ++axis) allow(7, axis, b);

    return t;
}

} // namespace wfc::tilesets
