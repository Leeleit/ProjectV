#include <draco/compression/decode.h>
#include <fastgltf/types.hpp>
#include <meshoptimizer.h>

namespace projectv::asset {

namespace {
[[maybe_unused]] constexpr std::size_t gAssetPipelineLinkerAnchor = [] {
	return MESHOPTIMIZER_VERSION + static_cast<std::size_t>(fastgltf::AccessorType::Scalar) + static_cast<std::size_t>(draco::EncodedGeometryType::POINT_CLOUD);
}();
} // namespace

} // namespace projectv::asset
