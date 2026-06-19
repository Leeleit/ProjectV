/// \brief Generate `tests/fixtures/box_uv.glb`:
///
/// \details
/// same canonical unit box as
///  `box.glb` (AABB (-0.5..0.5)³, 24 vertices, 36 indices, 12 triangles,

///  6 face groups) but with face-projected UVs. Used by M6 to verify

///  the model pass correctly reads `inUv` from `model.vert` and lets

///  `model.frag` use it (procedural checkerboard or texture sample).

///  We hand-author the GLB blob here (no python-gltf dependency).

///  Vertex / normal / index layout copied byte-for-byte from the

///  Khronos `box.glb` fixture (`COLLADA2GLTF` output, used as the M1

///  baseline). We re-use its winding, AABB, and per-face vertex

///  ordering so this fixture is a strict superset: same triangles,

///  same normals, plus a UV attribute. The M5 regression test

///  (`ProjectVAssetTests`) loads the original `box.glb` and asserts

///  the same AABB; if we change winding here we would silently break

///  the asset test.

///  The 4 UVs per face (face-projected, [0..1] across the face's

///  1×1 quad) are appended in the same order as the vertices, so

///  `addVertex(pos, normal, uv)` reads cleanly:

///    face 0 (+Z):  v0, v1, v2, v3

///    face 1 (-Y):  v4, v5, v6, v7

///    face 2 (+X):  v8, v9, v10, v11

///    face 3 (+Y):  v12, v13, v14, v15

///    face 4 (-X):  v16, v17, v18, v19

///    face 5 (-Z):  v20, v21, v22, v23

///  We have to be careful with the per-vertex UV mapping because the

///  Khronos vertex order is not "bottom-left, bottom-right, top-right,

///  top-left" for every face — the original fixture rotates the UV

///  quads to match the face's local orientation. We mirror that

///  rotation here.


#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct Vertex {
	float pos[3];
	float normal[3];
	float uv[2];
};

/// \brief 24 vertices in the exact order the Khronos `box.glb` uses.
///
/// \details
/// The
///  per-face winding is therefore already correct for the model's

///  `cullMode = VK_CULL_MODE_BACK_BIT, frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE`

///  pipeline: the original fixture is the live baseline and any

///  deviation in winding would show up as a missing face in the

///  ProjectV render. The UV field is added per vertex and is not

///  read by the existing M1 asset tests.

const std::array<Vertex, 24> kBoxVertices = {{
/// \brief face 0:
///
/// \details
/// +Z, normal = (0, 0, +1)
	Vertex{ {-0.5f, -0.5f, +0.5f}, { 0.0f,  0.0f, +1.0f}, {0.0f, 0.0f} },
	Vertex{ {+0.5f, -0.5f, +0.5f}, { 0.0f,  0.0f, +1.0f}, {1.0f, 0.0f} },
	Vertex{ {-0.5f, +0.5f, +0.5f}, { 0.0f,  0.0f, +1.0f}, {0.0f, 1.0f} },
	Vertex{ {+0.5f, +0.5f, +0.5f}, { 0.0f,  0.0f, +1.0f}, {1.0f, 1.0f} },
/// \brief face 1:
///
/// \details
/// -Y, normal = (0, -1, 0)
	Vertex{ {+0.5f, -0.5f, +0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f} },
	Vertex{ {-0.5f, -0.5f, +0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f} },
	Vertex{ {+0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f} },
	Vertex{ {-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f} },
/// \brief face 2:
///
/// \details
/// +X, normal = (+1, 0, 0)
	Vertex{ {+0.5f, +0.5f, +0.5f}, {+1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
	Vertex{ {+0.5f, -0.5f, +0.5f}, {+1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },
	Vertex{ {+0.5f, +0.5f, -0.5f}, {+1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
	Vertex{ {+0.5f, -0.5f, -0.5f}, {+1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
/// \brief face 3:
///
/// \details
/// +Y, normal = (0, +1, 0)
	Vertex{ {-0.5f, +0.5f, +0.5f}, { 0.0f, +1.0f,  0.0f}, {0.0f, 1.0f} },
	Vertex{ {+0.5f, +0.5f, +0.5f}, { 0.0f, +1.0f,  0.0f}, {1.0f, 1.0f} },
	Vertex{ {-0.5f, +0.5f, -0.5f}, { 0.0f, +1.0f,  0.0f}, {0.0f, 0.0f} },
	Vertex{ {+0.5f, +0.5f, -0.5f}, { 0.0f, +1.0f,  0.0f}, {1.0f, 0.0f} },
/// \brief face 4:
///
/// \details
/// -X, normal = (-1, 0, 0)
	Vertex{ {-0.5f, -0.5f, +0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
	Vertex{ {-0.5f, +0.5f, +0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
	Vertex{ {-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },
	Vertex{ {-0.5f, +0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
/// \brief face 5:
///
/// \details
/// -Z, normal = (0, 0, -1)
	Vertex{ {-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f} },
	Vertex{ {-0.5f, +0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f} },
	Vertex{ {+0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f} },
	Vertex{ {+0.5f, +0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f} },
}};

const std::array<uint16_t, 36> kBoxIndices = {
	/// \brief face 0:
	///
	/// \details
	/// +Z
	0, 1, 2,  3, 2, 1,
	/// \brief face 1:
	///
	/// \details
	/// -Y
	4, 5, 6,  7, 6, 5,
	/// \brief face 2:
	///
	/// \details
	/// +X
	8, 9, 10,  11, 10, 9,
	/// \brief face 3:
	///
	/// \details
	/// +Y
	12, 13, 14,  15, 14, 13,
	/// \brief face 4:
	///
	/// \details
	/// -X
	16, 17, 18,  19, 18, 17,
	/// \brief face 5:
	///
	/// \details
	/// -Z
	20, 21, 22,  23, 22, 21,
};

} // namespace

int main() {
	std::vector<uint8_t> bin;
	bin.resize(840);
	uint8_t *p = bin.data();
	for (const Vertex &v : kBoxVertices) {
		std::memcpy(p, v.pos, 12);
		p += 12;
	}
	for (const Vertex &v : kBoxVertices) {
		std::memcpy(p, v.normal, 12);
		p += 12;
	}
	for (const Vertex &v : kBoxVertices) {
		std::memcpy(p, v.uv, 8);
		p += 8;
	}
	for (uint16_t i : kBoxIndices) {
		std::memcpy(p, &i, 2);
		p += 2;
	}

	const std::string json = R"({
  "asset": { "version": "2.0", "generator": "ProjectV tests/fixtures/box_uv.glb generator" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes":  [ { "mesh": 0 } ],
  "meshes": [ {
    "primitives": [ {
      "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 },
      "indices": 3,
      "material": 0,
      "mode": 4
    } ]
  } ],
  "materials": [ {
    "pbrMetallicRoughness": {
      "baseColorFactor": [ 0.85, 0.65, 0.45, 1.0 ],
      "metallicFactor": 0.0,
      "roughnessFactor": 0.55
    }
  } ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 24, "type": "VEC3", "min": [-0.5, -0.5, -0.5], "max": [ 0.5,  0.5,  0.5] },
    { "bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": 24, "type": "VEC2" },
    { "bufferView": 3, "componentType": 5123, "count": 36, "type": "SCALAR" }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,   "byteLength": 288 },
    { "buffer": 0, "byteOffset": 288, "byteLength": 288 },
    { "buffer": 0, "byteOffset": 576, "byteLength": 192 },
    { "buffer": 0, "byteOffset": 768, "byteLength":  72 }
  ],
  "buffers": [ { "byteLength": 840 } ]
})";

	std::vector<uint8_t> jsonBytes(json.begin(), json.end());
	while (jsonBytes.size() % 4 != 0) {
		jsonBytes.push_back(' ');
	}

	const uint32_t totalLength = 12 + 8 + static_cast<uint32_t>(jsonBytes.size()) + 8 + static_cast<uint32_t>(bin.size());

	std::ofstream out("/home/le1t/Projects/ProjectV/tests/fixtures/box_uv.glb", std::ios::binary);
	if (!out) {
		std::fprintf(stderr, "failed to open output file\n");
		return 1;
	}
	const uint32_t magic = 0x46546C67;
	const uint32_t version = 2;
	out.write(reinterpret_cast<const char *>(&magic), 4);
	out.write(reinterpret_cast<const char *>(&version), 4);
	out.write(reinterpret_cast<const char *>(&totalLength), 4);
	const uint32_t jsonChunkType = 0x4E4F534A;
	const uint32_t jsonChunkLength = static_cast<uint32_t>(jsonBytes.size());
	out.write(reinterpret_cast<const char *>(&jsonChunkLength), 4);
	out.write(reinterpret_cast<const char *>(&jsonChunkType), 4);
	out.write(reinterpret_cast<const char *>(jsonBytes.data()), jsonBytes.size());
	const uint32_t binChunkType = 0x004E4942;
	const uint32_t binChunkLength = static_cast<uint32_t>(bin.size());
	out.write(reinterpret_cast<const char *>(&binChunkLength), 4);
	out.write(reinterpret_cast<const char *>(&binChunkType), 4);
	out.write(reinterpret_cast<const char *>(bin.data()), bin.size());

	std::printf("wrote tests/fixtures/box_uv.glb: 24 verts, 36 indices, 840 byte BIN chunk\n");
	return 0;
}
