#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "asset/AssetLoader.hpp"

namespace projectv::asset {

// **Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
// `AssetRegistry::Load`. Cold path (1× per asset), so the
// `std::expected` cost is irrelevant. The success value is
// the previously-loaded `LoadedAsset*` (already in the
// registry's `mEntries` map) so the caller doesn't have to
// do a second `Get(id)` lookup; `std::unexpected` carries
// the specific failure variant.
enum class AssetLoadError : std::uint8_t {
	LoadGlbFailed = 0,
};

constexpr std::string_view toString(AssetLoadError e) noexcept {
	switch (e) {
	case AssetLoadError::LoadGlbFailed: return "LoadGlbFailed";
	}
	return "Unknown";
}

class AssetRegistry {
  public:
	AssetRegistry() = default;
	~AssetRegistry() = default;

	AssetRegistry(const AssetRegistry &) = delete;
	AssetRegistry &operator=(const AssetRegistry &) = delete;

	// Synchronous load: parses the .glb at `path` and stores the result under
	// `id`. Returns `std::expected<const LoadedAsset*, AssetLoadError>` —
	// the success value is the freshly-loaded pointer (already in the
	// registry, so no second `Get(id)` round-trip), the error variant
	// names the specific failure (currently `LoadGlbFailed` for parse
	// errors; the diagnostic message is on
	// `GetAssetLoaderLastErrorMessage`).
	std::expected<const LoadedAsset *, AssetLoadError> Load(const std::string &id, const std::string &path);

	// Returns the previously-loaded asset, or nullptr if `id` is unknown.
	// The returned pointer is owned by the registry and is invalidated on
	// `Unload` / `Clear` / destruction.
	const LoadedAsset *Get(const std::string &id) const;

	// Drops a single entry. No-op if the id is unknown.
	void Unload(const std::string &id);

	// Drops every entry.
	void Clear();

	// All known ids in insertion order.
	std::vector<std::string> Ids() const;

	std::size_t Size() const;

  private:
	mutable std::mutex mMutex;
	std::unordered_map<std::string, std::unique_ptr<LoadedAsset>> mEntries;
	std::vector<std::string> mInsertionOrder;
};

} // namespace projectv::asset

