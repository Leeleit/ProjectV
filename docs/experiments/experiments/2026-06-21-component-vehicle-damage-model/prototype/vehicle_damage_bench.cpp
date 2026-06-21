#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numbers>
#include <random>
#include <span>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Geometry primitives
// ---------------------------------------------------------------------------

struct Vec3 {
    float x{}, y{}, z{};
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    auto operator<=>(const Vec3&) const = default;
    Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    friend Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
    friend Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
    friend Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
    friend Vec3 operator/(Vec3 a, float s) { return {a.x/s, a.y/s, a.z/s}; }
    Vec3 cross(Vec3 o) const { return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x}; }
    float dot(Vec3 o) const { return x*o.x + y*o.y + z*o.z; }
    float len2() const { return dot(*this); }
    float len() const { return std::sqrt(len2()); }
    Vec3 norm() const { float l = len(); return l > 0 ? *this / l : Vec3{}; }
};

struct Ray { Vec3 origin, dir; };

struct AABB {
    Vec3 min, max;
    bool intersect(Ray r, float& t) const {
        float tmin = -std::numeric_limits<float>::infinity();
        float tmax = std::numeric_limits<float>::infinity();
        for (int i = 0; i < 3; ++i) {
            float o = (&r.origin.x)[i];
            float d = (&r.dir.x)[i];
            float lo = (&min.x)[i];
            float hi = (&max.x)[i];
            if (std::abs(d) < 1e-8f) {
                if (o < lo || o > hi) return false;
            } else {
                float t1 = (lo - o) / d;
                float t2 = (hi - o) / d;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        t = tmin;
        return t >= 0;
    }
};

struct OBB {
    Vec3 center, half;   // Extents along local axes
    Vec3 axes[3];        // Orthonormal local frame
    bool intersect(Ray r, float& t_out) const {
        // Transform ray into OBB local space
        Vec3 p = r.origin - center;
        Vec3 d{r.dir.dot(axes[0]), r.dir.dot(axes[1]), r.dir.dot(axes[2])};
        Vec3 o{p.dot(axes[0]), p.dot(axes[1]), p.dot(axes[2])};
        Ray local_r{o, d};
        AABB local_aabb{{-half.x, -half.y, -half.z}, {half.x, half.y, half.z}};
        return local_aabb.intersect(local_r, t_out);
    }
};

// Module types
enum class ModuleType : uint8_t {
    Engine, Transmission, Crew, Ammo, Fuel,
    Optics, FCS, Radio, Barrel, Breech,
    Track, Wheel, Cargo, Radar, BlowoutPanel,
    Hull
};

static const char* module_name(ModuleType mt) {
    switch (mt) {
        case ModuleType::Engine: return "Engine";
        case ModuleType::Transmission: return "Transmission";
        case ModuleType::Crew: return "Crew";
        case ModuleType::Ammo: return "Ammo";
        case ModuleType::Fuel: return "Fuel";
        case ModuleType::Optics: return "Optics";
        case ModuleType::FCS: return "FCS";
        case ModuleType::Radio: return "Radio";
        case ModuleType::Barrel: return "Barrel";
        case ModuleType::Breech: return "Breech";
        case ModuleType::Track: return "Track";
        case ModuleType::Wheel: return "Wheel";
        case ModuleType::Cargo: return "Cargo";
        case ModuleType::Radar: return "Radar";
        case ModuleType::BlowoutPanel: return "BlowoutPanel";
        case ModuleType::Hull: return "Hull";
    }
    return "?";
}

struct Module {
    OBB obb;
    ModuleType type{};
    float max_hp{};
    float hp{};
};

struct Vehicle {
    std::vector<Module> modules;
    AABB hull_aabb;           // Full vehicle bounds
    Vec3 center;
    float hull_length{}, hull_width{}, hull_height{};
};

// ---------------------------------------------------------------------------
// Vehicle definitions (5 configs)
// ---------------------------------------------------------------------------

static Vehicle make_light_tank() {
    Vehicle v;
    v.hull_length = 6.0f; v.hull_width = 3.0f; v.hull_height = 2.2f;
    v.center = {0,0,0};
    v.hull_aabb = {{-3,-1.1f,-1.5f}, {3,1.1f,1.5f}};
    auto m = [&](float cx, float cy, float cz, float hx, float hy, float hz, ModuleType t, float hp) {
        return Module{{Vec3{cx,cy,cz}, Vec3{hx,hy,hz}, {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}}}, t, hp, hp};
    };
    v.modules = {
        m( 1.8f, 0.1f, 0.0f, 0.8f, 0.5f, 0.6f, ModuleType::Engine, 250),
        m( 2.0f,-0.3f, 0.0f, 0.5f, 0.3f, 0.4f, ModuleType::Transmission, 180),
        m(-1.5f, 0.2f, 0.4f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-1.5f, 0.2f,-0.4f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-0.5f, 0.2f, 0.4f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-1.0f, 0.4f, 0.0f, 0.4f, 0.2f, 0.3f, ModuleType::Ammo, 120),
        m( 0.5f,-0.3f, 0.6f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 100),
        m( 0.5f,-0.3f,-0.6f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 100),
        m(-0.2f, 0.3f, 0.0f, 0.2f, 0.1f, 0.1f, ModuleType::Radio, 40),
    };
    return v;
}

static Vehicle make_mbt() {
    Vehicle v;
    v.hull_length = 10.0f; v.hull_width = 3.6f; v.hull_height = 2.8f;
    v.center = {0,0,0};
    v.hull_aabb = {{-5,-1.4f,-1.8f}, {5,1.4f,1.8f}};
    auto m = [&](float cx, float cy, float cz, float hx, float hy, float hz, ModuleType t, float hp) {
        return Module{{Vec3{cx,cy,cz}, Vec3{hx,hy,hz}, {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}}}, t, hp, hp};
    };
    v.modules = {
        m( 3.5f, 0.0f, 0.0f, 1.2f, 0.5f, 0.8f, ModuleType::Engine, 400),
        m( 2.0f,-0.4f, 0.0f, 0.6f, 0.3f, 0.5f, ModuleType::Transmission, 250),
        m(-2.5f, 0.3f, 0.5f, 0.35f,0.25f,0.25f, ModuleType::Crew, 100),
        m(-2.5f, 0.3f,-0.5f, 0.35f,0.25f,0.25f, ModuleType::Crew, 100),
        m(-1.5f, 0.3f, 0.5f, 0.35f,0.25f,0.25f, ModuleType::Crew, 100),
        m(-1.5f, 0.3f,-0.5f, 0.35f,0.25f,0.25f, ModuleType::Crew, 100),
        m(-1.0f, 0.5f, 0.0f, 0.5f, 0.25f,0.35f, ModuleType::Ammo, 200),
        m( 1.0f, 0.5f, 0.6f, 0.4f, 0.2f, 0.2f, ModuleType::Ammo, 150),
        m( 1.0f,-0.4f, 0.8f, 0.5f, 0.2f, 0.25f, ModuleType::Fuel, 150),
        m( 1.0f,-0.4f,-0.8f, 0.5f, 0.2f, 0.25f, ModuleType::Fuel, 150),
        m(-0.5f, 0.4f, 0.0f, 0.2f, 0.1f, 0.1f, ModuleType::Optics, 30),
        m( 0.0f, 0.3f, 0.0f, 0.15f,0.1f, 0.1f, ModuleType::FCS, 50),
        m(-3.0f, 0.1f, 0.0f, 0.2f, 0.1f, 0.1f, ModuleType::Radio, 40),
        m( 2.5f, 0.4f, 0.0f, 0.3f, 0.1f, 0.2f, ModuleType::BlowoutPanel, 60),
    };
    return v;
}

static Vehicle make_apc() {
    Vehicle v;
    v.hull_length = 7.0f; v.hull_width = 3.0f; v.hull_height = 2.8f;
    v.center = {0,0,0};
    v.hull_aabb = {{-3.5f,-1.4f,-1.5f}, {3.5f,1.4f,1.5f}};
    auto m = [&](float cx, float cy, float cz, float hx, float hy, float hz, ModuleType t, float hp) {
        return Module{{Vec3{cx,cy,cz}, Vec3{hx,hy,hz}, {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}}}, t, hp, hp};
    };
    v.modules = {
        m( 2.5f, 0.1f, 0.0f, 0.8f, 0.5f, 0.6f, ModuleType::Engine, 300),
        m(-2.5f, 0.2f, 0.5f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-2.5f, 0.2f,-0.5f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-1.0f, 0.2f, 0.5f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m( 1.0f,-0.3f, 0.0f, 0.6f, 0.3f, 0.5f, ModuleType::Cargo, 100),
        m( 1.0f,-0.3f, 0.7f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 100),
        m( 1.0f,-0.3f,-0.7f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 100),
        m( 1.5f, 0.3f, 0.0f, 0.3f, 0.2f, 0.3f, ModuleType::Ammo, 120),
        m( 0.0f, 0.2f, 0.0f, 0.3f, 0.1f, 0.2f, ModuleType::Cargo, 80),
        m(-0.5f, 0.2f,-0.5f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
    };
    return v;
}

static Vehicle make_spaa() {
    Vehicle v;
    v.hull_length = 6.5f; v.hull_width = 3.2f; v.hull_height = 3.5f;
    v.center = {0,0,0};
    v.hull_aabb = {{-3.25f,-1.75f,-1.6f}, {3.25f,1.75f,1.6f}};
    auto m = [&](float cx, float cy, float cz, float hx, float hy, float hz, ModuleType t, float hp) {
        return Module{{Vec3{cx,cy,cz}, Vec3{hx,hy,hz}, {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}}}, t, hp, hp};
    };
    v.modules = {
        m( 2.2f, 0.0f, 0.0f, 0.8f, 0.5f, 0.6f, ModuleType::Engine, 280),
        m(-2.0f, 0.3f, 0.5f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-2.0f, 0.3f,-0.5f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-0.5f, 0.3f, 0.0f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m( 1.0f, 0.8f, 0.0f, 0.2f, 0.3f, 0.2f, ModuleType::Radar, 50),
        m( 0.5f, 0.2f, 0.0f, 0.15f,0.1f, 0.1f, ModuleType::Optics, 30),
        m(-0.5f,-0.3f, 0.6f, 0.4f, 0.2f, 0.2f, ModuleType::Ammo, 100),
        m(-0.5f,-0.3f,-0.6f, 0.4f, 0.2f, 0.2f, ModuleType::Ammo, 100),
        m( 1.0f,-0.3f, 0.6f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 100),
    };
    return v;
}

static Vehicle make_truck() {
    Vehicle v;
    v.hull_length = 8.0f; v.hull_width = 2.6f; v.hull_height = 2.8f;
    v.center = {0,0,0};
    v.hull_aabb = {{-4,-1.4f,-1.3f}, {4,1.4f,1.3f}};
    auto m = [&](float cx, float cy, float cz, float hx, float hy, float hz, ModuleType t, float hp) {
        return Module{{Vec3{cx,cy,cz}, Vec3{hx,hy,hz}, {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}}}, t, hp, hp};
    };
    v.modules = {
        m( 2.8f, 0.1f, 0.0f, 0.7f, 0.5f, 0.5f, ModuleType::Engine, 200),
        m( 2.0f, 0.2f, 0.4f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m( 2.0f, 0.2f,-0.4f, 0.3f, 0.2f, 0.2f, ModuleType::Crew, 80),
        m(-1.5f, 0.3f, 0.0f, 1.5f, 0.8f, 1.0f, ModuleType::Cargo, 150),
        m( 0.0f,-0.3f, 0.5f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 80),
        m( 0.0f,-0.3f,-0.5f, 0.4f, 0.2f, 0.2f, ModuleType::Fuel, 80),
    };
    return v;
}

// ---------------------------------------------------------------------------
// Hit-table: precomputed 3D bitmask per vehicle
// ---------------------------------------------------------------------------

struct HitTable3D {
    Vec3 origin;
    float cell_size{};
    Vec3 dims;                     // cells per axis
    std::vector<uint8_t> table;    // module index per cell (0 = none)

    void build(const Vehicle& v, float cell_sz) {
        cell_size = cell_sz;
        origin = v.hull_aabb.min;
        Vec3 extent = v.hull_aabb.max - v.hull_aabb.min;
        dims = {std::ceil(extent.x / cell_size),
                std::ceil(extent.y / cell_size),
                std::ceil(extent.z / cell_size)};
        table.resize(static_cast<size_t>(dims.x) * dims.y * dims.z, 0);
        // For each module, rasterize its OBB into the grid
        for (size_t mi = 0; mi < v.modules.size(); ++mi) {
            const auto& mod = v.modules[mi];
            // OBB extent in world space → find AABB
            Vec3 corners[8];
            for (int i = 0; i < 8; ++i) {
                Vec3 p = mod.obb.center;
                for (int a = 0; a < 3; ++a) {
                    float s = (i & (1 << a)) ? 1.0f : -1.0f;
                    p += mod.obb.axes[a] * (mod.obb.half[a] * s);
                }
                corners[i] = p;
            }
            Vec3 cmin = corners[0], cmax = corners[0];
            for (int i = 1; i < 8; ++i) {
                cmin.x = std::min(cmin.x, corners[i].x);
                cmin.y = std::min(cmin.y, corners[i].y);
                cmin.z = std::min(cmin.z, corners[i].z);
                cmax.x = std::max(cmax.x, corners[i].x);
                cmax.y = std::max(cmax.y, corners[i].y);
                cmax.z = std::max(cmax.z, corners[i].z);
            }
            int ix0 = std::max(0, static_cast<int>((cmin.x - origin.x) / cell_size));
            int iy0 = std::max(0, static_cast<int>((cmin.y - origin.y) / cell_size));
            int iz0 = std::max(0, static_cast<int>((cmin.z - origin.z) / cell_size));
            int ix1 = std::min(static_cast<int>(dims.x-1), static_cast<int>((cmax.x - origin.x) / cell_size));
            int iy1 = std::min(static_cast<int>(dims.y-1), static_cast<int>((cmax.y - origin.y) / cell_size));
            int iz1 = std::min(static_cast<int>(dims.z-1), static_cast<int>((cmax.z - origin.z) / cell_size));
            // Simple occupancy fill: if cell center is inside OBB, mark it
            Vec3 cell_center;
            for (int iz = iz0; iz <= iz1; ++iz) {
                cell_center.z = origin.z + (iz + 0.5f) * cell_size;
                for (int iy = iy0; iy <= iy1; ++iy) {
                    cell_center.y = origin.y + (iy + 0.5f) * cell_size;
                    for (int ix = ix0; ix <= ix1; ++ix) {
                        cell_center.x = origin.x + (ix + 0.5f) * cell_size;
                        // Transform to OBB local
                        Vec3 lp = cell_center - mod.obb.center;
                        Vec3 local{lp.dot(mod.obb.axes[0]), lp.dot(mod.obb.axes[1]), lp.dot(mod.obb.axes[2])};
                        if (std::abs(local.x) <= mod.obb.half.x &&
                            std::abs(local.y) <= mod.obb.half.y &&
                            std::abs(local.z) <= mod.obb.half.z) {
                            table[static_cast<size_t>(iz) * dims.y * dims.x + iy * dims.x + ix] = static_cast<uint8_t>(mi + 1);
                        }
                    }
                }
            }
        }
    }

    uint8_t lookup(const Vec3& world_pos) const {
        int ix = static_cast<int>((world_pos.x - origin.x) / cell_size);
        int iy = static_cast<int>((world_pos.y - origin.y) / cell_size);
        int iz = static_cast<int>((world_pos.z - origin.z) / cell_size);
        if (ix < 0 || ix >= dims.x || iy < 0 || iy >= dims.y || iz < 0 || iz >= dims.z) return 0;
        return table[static_cast<size_t>(iz) * dims.y * dims.x + iy * dims.x + ix];
    }
};

// ---------------------------------------------------------------------------
// Occupancy grid (E_OccupancyGrid): coarser resolution, O(1) lookup
// ---------------------------------------------------------------------------

struct OccupancyGrid {
    Vec3 origin;
    float cell_size{};
    Vec3 dims;
    std::vector<uint8_t> table; // module index per cell

    void build(const Vehicle& v, float cell_sz) {
        cell_size = cell_sz;
        origin = v.hull_aabb.min;
        Vec3 extent = v.hull_aabb.max - v.hull_aabb.min;
        dims = {std::ceil(extent.x / cell_size),
                std::ceil(extent.y / cell_size),
                std::ceil(extent.z / cell_size)};
        table.resize(static_cast<size_t>(dims.x) * dims.y * dims.z, 0);
        for (size_t mi = 0; mi < v.modules.size(); ++mi) {
            const auto& mod = v.modules[mi];
            Vec3 corners[8];
            for (int i = 0; i < 8; ++i) {
                Vec3 p = mod.obb.center;
                for (int a = 0; a < 3; ++a) {
                    p += mod.obb.axes[a] * (mod.obb.half[a] * ((i & (1 << a)) ? 1.0f : -1.0f));
                }
                corners[i] = p;
            }
            Vec3 cmin = corners[0], cmax = corners[0];
            for (int i = 1; i < 8; ++i) {
                cmin.x = std::min(cmin.x, corners[i].x); cmax.x = std::max(cmax.x, corners[i].x);
                cmin.y = std::min(cmin.y, corners[i].y); cmax.y = std::max(cmax.y, corners[i].y);
                cmin.z = std::min(cmin.z, corners[i].z); cmax.z = std::max(cmax.z, corners[i].z);
            }
            int ix0 = std::max(0, (int)((cmin.x - origin.x) / cell_size));
            int iy0 = std::max(0, (int)((cmin.y - origin.y) / cell_size));
            int iz0 = std::max(0, (int)((cmin.z - origin.z) / cell_size));
            int ix1 = std::min((int)dims.x-1, (int)((cmax.x - origin.x) / cell_size));
            int iy1 = std::min((int)dims.y-1, (int)((cmax.y - origin.y) / cell_size));
            int iz1 = std::min((int)dims.z-1, (int)((cmax.z - origin.z) / cell_size));
            for (int iz = iz0; iz <= iz1; ++iz)
                for (int iy = iy0; iy <= iy1; ++iy)
                    for (int ix = ix0; ix <= ix1; ++ix) {
                        size_t idx = (size_t)iz * dims.y * dims.x + iy * dims.x + ix;
                        if (table[idx] == 0)
                            table[idx] = (uint8_t)(mi + 1);
                    }
        }
    }

    uint8_t lookup(const Vec3& p) const {
        int ix = (int)((p.x - origin.x) / cell_size);
        int iy = (int)((p.y - origin.y) / cell_size);
        int iz = (int)((p.z - origin.z) / cell_size);
        if (ix < 0 || ix >= dims.x || iy < 0 || iy >= dims.y || iz < 0 || iz >= dims.z) return 0;
        return table[(size_t)iz * dims.y * dims.x + iy * dims.x + ix];
    }
};

// ---------------------------------------------------------------------------
// Binned spatial grid (B_BinnedGrid)
// ---------------------------------------------------------------------------

struct BinnedGrid {
    float cell_size{1.0f};
    Vec3 origin;
    Vec3 dims;
    std::vector<std::vector<size_t>> bins; // per-cell list of module indices

    void build(const Vehicle& v, float cs) {
        cell_size = cs;
        origin = v.hull_aabb.min;
        Vec3 ext = v.hull_aabb.max - v.hull_aabb.min;
        dims = {std::ceil(ext.x/cell_size), std::ceil(ext.y/cell_size), std::ceil(ext.z/cell_size)};
        bins.resize((size_t)dims.x * dims.y * dims.z);

        for (size_t mi = 0; mi < v.modules.size(); ++mi) {
            const auto& mod = v.modules[mi];
            Vec3 cmin = mod.obb.center, cmax = mod.obb.center;
            for (int a = 0; a < 3; ++a) {
                Vec3 ax = mod.obb.axes[a] * mod.obb.half[a];
                cmin -= ax; cmax += ax;
            }
            int ix0 = std::max(0, (int)((cmin.x - origin.x)/cell_size));
            int iy0 = std::max(0, (int)((cmin.y - origin.y)/cell_size));
            int iz0 = std::max(0, (int)((cmin.z - origin.z)/cell_size));
            int ix1 = std::min((int)dims.x-1, (int)((cmax.x - origin.x)/cell_size));
            int iy1 = std::min((int)dims.y-1, (int)((cmax.y - origin.y)/cell_size));
            int iz1 = std::min((int)dims.z-1, (int)((cmax.z - origin.z)/cell_size));
            for (int iz = iz0; iz <= iz1; ++iz)
                for (int iy = iy0; iy <= iy1; ++iy)
                    for (int ix = ix0; ix <= ix1; ++ix)
                        bins[(size_t)iz * dims.y * dims.x + iy * dims.x + ix].push_back(mi);
        }
    }

    std::span<const size_t> query(int ix, int iy, int iz) const {
        if (ix < 0 || ix >= dims.x || iy < 0 || iy >= dims.y || iz < 0 || iz >= dims.z)
            return {};
        const auto& b = bins[(size_t)iz * dims.y * dims.x + iy * dims.x + ix];
        return {b.data(), b.size()};
    }
};

// ---------------------------------------------------------------------------
// BVH (D_BVH_WithinVehicle)
// ---------------------------------------------------------------------------

struct BVHNode {
    AABB bounds;
    size_t module_idx{};
    BVHNode* left{};
    BVHNode* right{};
};

struct BVHEntry {
    AABB b;
    size_t idx;
};

struct BVH {
    std::vector<BVHNode> nodes;
    BVHNode* root{};

    void build(std::span<const Module> modules) {
        nodes.clear();
        if (modules.empty()) { root = nullptr; return; }
        std::vector<BVHEntry> entries;
        for (size_t i = 0; i < modules.size(); ++i) {
            const auto& m = modules[i];
            Vec3 cmin = m.obb.center, cmax = m.obb.center;
            for (int a = 0; a < 3; ++a) {
                Vec3 ax = m.obb.axes[a] * m.obb.half[a];
                cmin -= ax; cmax += ax;
            }
            entries.push_back({{cmin, cmax}, i});
        }
        root = build_rec(entries, 0, (int)entries.size());
    }

    BVHNode* build_rec(std::vector<BVHEntry>& entries, int l, int r) {
        if (l >= r) return nullptr;
        AABB b = entries[l].b;
        for (int i = l+1; i < r; ++i) {
            b.min.x = std::min(b.min.x, entries[i].b.min.x);
            b.min.y = std::min(b.min.y, entries[i].b.min.y);
            b.min.z = std::min(b.min.z, entries[i].b.min.z);
            b.max.x = std::max(b.max.x, entries[i].b.max.x);
            b.max.y = std::max(b.max.y, entries[i].b.max.y);
            b.max.z = std::max(b.max.z, entries[i].b.max.z);
        }
        auto& node = nodes.emplace_back();
        node.bounds = b;
        if (r - l == 1) {
            node.module_idx = entries[l].idx;
            node.left = node.right = nullptr;
            return &node;
        }
        // Split along longest axis at midpoint
        Vec3 dim = {b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z};
        int axis = (dim.x >= dim.y && dim.x >= dim.z) ? 0 : (dim.y >= dim.z ? 1 : 2);
        float mid = (&b.min.x)[axis] + (&dim.x)[axis] * 0.5f;
        int m = l;
        for (int i = l; i < r; ++i) {
            if ((axis == 0 && entries[i].b.min.x < mid) ||
                (axis == 1 && entries[i].b.min.y < mid) ||
                (axis == 2 && entries[i].b.min.z < mid)) {
                std::swap(entries[i], entries[m]); ++m;
            }
        }
        if (m == l || m == r) m = l + (r-l)/2; // Fallback
        node.left = build_rec(entries, l, m);
        node.right = build_rec(entries, m, r);
        return &node;
    }

    size_t intersect(Ray r, float& t_out, const std::vector<Module>& modules) const {
        t_out = std::numeric_limits<float>::infinity();
        size_t hit_idx = std::numeric_limits<size_t>::max();
        if (!root) return hit_idx;
        intersect_rec(r, root, t_out, hit_idx, modules);
        return hit_idx;
    }

    void intersect_rec(Ray r, BVHNode* n, float& t_best, size_t& best_idx, const std::vector<Module>& modules) const {
        if (!n) return;
        float t_node;
        if (!n->bounds.intersect(r, t_node) || t_node >= t_best) return;
        if (n->left || n->right) {
            if (n->left) intersect_rec(r, n->left, t_best, best_idx, modules);
            if (n->right) intersect_rec(r, n->right, t_best, best_idx, modules);
            return;
        }
        float t_mod;
        if (modules[n->module_idx].obb.intersect(r, t_mod) && t_mod < t_best) {
            t_best = t_mod;
            best_idx = n->module_idx;
        }
    }
};

// ---------------------------------------------------------------------------
// Strategy implementations
// ---------------------------------------------------------------------------

// A: Naive linear scan
struct StrategyA {
    template<typename F>
    void hit_test(Ray r, const Vehicle& v, F&& callback) const {
        float best_t = std::numeric_limits<float>::infinity();
        size_t best_idx = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < v.modules.size(); ++i) {
            float t;
            if (v.modules[i].obb.intersect(r, t) && t < best_t) {
                best_t = t;
                best_idx = i;
            }
        }
        if (best_idx < v.modules.size())
            callback(best_idx, best_t);
    }
};

// B: Binned grid
struct StrategyB {
    BinnedGrid grid;
    void build(const Vehicle& v) { grid.build(v, 0.5f); }
    template<typename F>
    void hit_test(Ray r, const Vehicle& v, F&& callback) const {
        float best_t = std::numeric_limits<float>::infinity();
        size_t best_idx = std::numeric_limits<size_t>::max();
        // Find which bin the ray origin is in
        int ix = (int)((r.origin.x - grid.origin.x) / grid.cell_size);
        int iy = (int)((r.origin.y - grid.origin.y) / grid.cell_size);
        int iz = (int)((r.origin.z - grid.origin.z) / grid.cell_size);
        auto candidates = grid.query(ix, iy, iz);
        for (size_t ci = 0; ci < candidates.size(); ++ci) {
            size_t mi = candidates[ci];
            if (mi >= v.modules.size()) continue;
            float t;
            if (v.modules[mi].obb.intersect(r, t) && t < best_t) {
                best_t = t;
                best_idx = mi;
            }
        }
        if (best_idx < v.modules.size())
            callback(best_idx, best_t);
    }
};

// C: Hit-table 3D mask
struct StrategyC {
    HitTable3D table;
    void build(const Vehicle& v) { table.build(v, 0.125f); }
    template<typename F>
    void hit_test(Ray r, const Vehicle& v, F&& callback) const {
        // Step along ray until we enter the hull AABB
        float t_entry;
        if (!v.hull_aabb.intersect(r, t_entry)) return;
        Vec3 p = r.origin + r.dir * t_entry;
        uint8_t id = table.lookup(p);
        if (id > 0 && (id-1) < v.modules.size()) {
            float t_mod;
            if (v.modules[id-1].obb.intersect(r, t_mod))
                callback(id-1, t_mod);
        }
    }
};

// D: BVH
struct StrategyD {
    BVH bvh;
    void build(const Vehicle& v) { bvh.build(v.modules); }
    template<typename F>
    void hit_test(Ray r, const Vehicle& v, F&& callback) const {
        float t;
        size_t idx = bvh.intersect(r, t, v.modules);
        if (idx < v.modules.size())
            callback(idx, t);
    }
};

// E: Occupancy grid (coarse)
struct StrategyE {
    OccupancyGrid grid;
    void build(const Vehicle& v) { grid.build(v, 0.25f); }
    template<typename F>
    void hit_test(Ray r, const Vehicle& v, F&& callback) const {
        float t_entry;
        if (!v.hull_aabb.intersect(r, t_entry)) return;
        Vec3 p = r.origin + r.dir * t_entry;
        uint8_t id = grid.lookup(p);
        if (id > 0 && (id-1) < v.modules.size()) {
            float t_mod;
            if (v.modules[id-1].obb.intersect(r, t_mod))
                callback(id-1, t_mod);
        }
    }
};

// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------

struct Result {
    const char* strategy;
    const char* vehicle;
    int64_t total_ns{};
    uint64_t hits{};
    uint64_t total_shots{};
    uint64_t accumulator{}; // Forces observable side effect
    double mean_ns() const { return total_shots ? (double)total_ns / total_shots : 0; }
};

static int64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

template<typename Strat>
static Result bench_strategy(const char* sname, const char* vname, Strat& strat,
                             const Vehicle& veh, const std::vector<Ray>& rays, int iterations) {
    Result r{sname, vname};
    std::vector<Ray> batch;
    batch.reserve(rays.size() * 16);

    for (int iter = 0; iter < iterations; ++iter) {
        batch.insert(batch.end(), rays.begin(), rays.end());
    }
    r.total_shots = batch.size();

    int64_t t0 = now_ns();
    for (const auto& ray : batch) {
        strat.hit_test(ray, veh, [&](size_t idx, float t) {
            r.accumulator += static_cast<uint64_t>(idx + 1);
            r.accumulator += static_cast<uint64_t>(t * 1e9f);
        });
    }
    r.total_ns = now_ns() - t0;
    return r;
}

// ---------------------------------------------------------------------------
// CSV writer
// ---------------------------------------------------------------------------

static void write_csv(FILE* f, const char* strategy, const char* vehicle,
                      int n_modules, float mean_ns, uint64_t hits, uint64_t total,
                      size_t mem_bytes, uint64_t accumulator) {
    std::fprintf(f, "%s,%s,%d,%.4f,%lu,%lu,%zu,%lu\n",
                 strategy, vehicle, n_modules, mean_ns,
                 (unsigned long)hits, (unsigned long)total, mem_bytes,
                 (unsigned long)accumulator);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

static std::vector<Ray> generate_rays(int seed, const AABB& target, int n) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist_x(target.min.x - 10, target.max.x + 10);
    std::uniform_real_distribution<float> dist_y(target.min.y - 10, target.max.y + 10);
    std::uniform_real_distribution<float> dist_z(target.min.z - 10, target.max.z + 10);
    std::uniform_real_distribution<float> angle(0, 2 * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> elev(-1, 1);

    Vec3 target_center = {(target.min.x + target.max.x) * 0.5f,
                          (target.min.y + target.max.y) * 0.5f,
                          (target.min.z + target.max.z) * 0.5f};

    std::vector<Ray> rays;
    rays.reserve(n);
    for (int i = 0; i < n; ++i) {
        Vec3 origin{dist_x(gen), dist_y(gen), dist_z(gen)};
        // Ensure ray goes toward target
        Vec3 dir = target_center - origin;
        // Add some spread
        float a = angle(gen), e = std::asin(elev(gen));
        Vec3 spread{std::cos(a) * std::cos(e), std::sin(e), std::sin(a) * std::cos(e)};
        dir = dir.norm() + spread * 0.5f;
        dir = dir.norm();
        if (dir.len2() < 0.01f) dir = Vec3{1,0,0};
        rays.push_back({origin, dir});
    }
    return rays;
}

int main() {
    // Vehicle configurations
    struct VC { const char* name; Vehicle(*maker)(); };
    VC configs[] = {
        {"light_tank", make_light_tank},
        {"mbt", make_mbt},
        {"apc", make_apc},
        {"spaa", make_spaa},
        {"truck", make_truck},
    };

    const int seeds[] = {1, 7, 42, 1234, 31337};
    const int rays_per_config = 200;
    const int iterations = 1000;

    // Pre-generate all rays
    struct RaySet { std::vector<Ray> rays; };
    std::vector<RaySet> all_rays;

    for (auto& vc : configs) {
        Vehicle v = vc.maker();
        for (int s : seeds) {
            auto& rs = all_rays.emplace_back();
            rs.rays = generate_rays(s, v.hull_aabb, rays_per_config);
        }
    }

    char fname[] = "build/results.csv";
    FILE* f = std::fopen(fname, "w");
    if (!f) { std::fprintf(stderr, "Can't open %s\n", fname); return 1; }
    std::fprintf(f, "strategy,vehicle,n_modules,mean_ns,hits,total_shots,mem_bytes,accumulator\n");

    int total_configs __attribute__((unused)) = 0;

    for (auto& vc : configs) {
        Vehicle veh = vc.maker();

        // Build precomputed structures
        StrategyB sb; sb.build(veh);
        StrategyC sc; sc.build(veh);
        StrategyD sd; sd.build(veh);
        StrategyE se; se.build(veh);

        // Memory estimates
        size_t mem_strat_a = 0;
        size_t mem_strat_b = sb.grid.bins.size() * sizeof(std::vector<size_t>) * 2
                           + sb.grid.bins.capacity() * sizeof(size_t) / 2;
        size_t mem_strat_c = sc.table.table.size();
        size_t mem_strat_d = sd.bvh.nodes.size() * sizeof(BVHNode);
        size_t mem_strat_e = se.grid.table.size();

        for (size_t ri = 0; ri < 5; ++ri) {
            size_t global_ri = (&vc - configs) * 5 + ri;
            const auto& rays = all_rays[global_ri].rays;

            // Strategy A (baseline)
            {
                StrategyA sa;
                auto r = bench_strategy("A_NaiveLinear", vc.name, sa, veh, rays, iterations);
                write_csv(f, "A_NaiveLinear", vc.name, (int)veh.modules.size(),
                          r.mean_ns(), r.hits, r.total_shots, mem_strat_a, r.accumulator);
                total_configs++;
            }
            // Strategy B
            {
                auto r = bench_strategy("B_BinnedGrid", vc.name, sb, veh, rays, iterations);
                write_csv(f, "B_BinnedGrid", vc.name, (int)veh.modules.size(),
                          r.mean_ns(), r.hits, r.total_shots, mem_strat_b, r.accumulator);
                total_configs++;
            }
            // Strategy C
            {
                auto r = bench_strategy("C_HitTable3D", vc.name, sc, veh, rays, iterations);
                write_csv(f, "C_HitTable3D", vc.name, (int)veh.modules.size(),
                          r.mean_ns(), r.hits, r.total_shots, mem_strat_c, r.accumulator);
                total_configs++;
            }
            // Strategy D
            {
                auto r = bench_strategy("D_BVH", vc.name, sd, veh, rays, iterations);
                write_csv(f, "D_BVH", vc.name, (int)veh.modules.size(),
                          r.mean_ns(), r.hits, r.total_shots, mem_strat_d, r.accumulator);
                total_configs++;
            }
            // Strategy E
            {
                auto r = bench_strategy("E_OccupancyGrid", vc.name, se, veh, rays, iterations);
                write_csv(f, "E_OccupancyGrid", vc.name, (int)veh.modules.size(),
                          r.mean_ns(), r.hits, r.total_shots, mem_strat_e, r.accumulator);
                total_configs++;
            }
        }
    }

    std::fclose(f);
    // Also write a summary
    std::printf("Benchmark complete: %d configs × 5 strategies × 5 seeds × %d rays × %d iter = %d shots\n",
                (int)(sizeof(configs)/sizeof(configs[0])), rays_per_config, iterations,
                (int)(sizeof(configs)/sizeof(configs[0])) * 5 * 5 * rays_per_config * iterations);
    std::printf("Results written to %s\n", fname);
    return 0;
}
