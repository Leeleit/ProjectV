#pragma once

#include <string>

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

