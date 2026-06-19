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

	/// \brief Synchronous load:
	///
	/// \details
	/// parses the .glb at `path` and stores the result under
	///  `id`. Returns `std::expected<const LoadedAsset*, AssetLoadError>` —

	///  the success value is the freshly-loaded pointer (already in the

	///  registry, so no second `Get(id)` round-trip), the error variant

	///  names the specific failure (currently `LoadGlbFailed` for parse

	///  errors; the diagnostic message is on

	///  `GetAssetLoaderLastErrorMessage`).

	std::expected<const LoadedAsset *, AssetLoadError> Load(const std::string &id, const std::string &path);

	/// \brief Returns the previously-loaded asset, or nullptr if `id` is unknown.
	///
	/// \details
	///  The returned pointer is owned by the registry and is invalidated on

	///  `Unload` / `Clear` / destruction.

	const LoadedAsset *Get(const std::string &id) const;

	/// \brief Drops a single entry.
	///
	/// \details
	/// No-op if the id is unknown.
	void Unload(const std::string &id);

	/// \brief Drops every entry.
	void Clear();

	/// \brief All known ids in insertion order.
	std::vector<std::string> Ids() const;

	std::size_t Size() const;

  private:
	mutable std::mutex mMutex;
	std::unordered_map<std::string, std::unique_ptr<LoadedAsset>> mEntries;
	std::vector<std::string> mInsertionOrder;
};

} // namespace projectv::asset

