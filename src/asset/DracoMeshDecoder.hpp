#pragma once

#include <string> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "asset/AssetLoader.hpp"

namespace fastgltf {
struct Asset;
struct Primitive;
} // namespace fastgltf

namespace projectv::asset {

bool DecodeDracoPrimitive(
	const fastgltf::Asset &asset,
	const fastgltf::Primitive &primitive,
	PrimitiveData &out,
	std::string *outError = nullptr);

} // namespace projectv::asset

