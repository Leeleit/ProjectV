#include "asset/AssetPipelineVersion.hpp"

#include <draco/compression/decode.h>
#include <fastgltf/types.hpp>
#include <meshoptimizer.h>

namespace projectv::asset {

namespace {
[[maybe_unused]] const size_t gAssetPipelineLinkerAnchor = []() {
	return MESHOPTIMIZER_VERSION
		+ static_cast<size_t>(fastgltf::AccessorType::Scalar)
		+ static_cast<size_t>(draco::EncodedGeometryType::POINT_CLOUD);
}();
} // namespace

} // namespace projectv::asset
