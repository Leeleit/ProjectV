#ifndef ASSET_REGISTRY_HPP
#define ASSET_REGISTRY_HPP

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "asset/AssetLoader.hpp"

namespace projectv::asset {

class AssetRegistry {
  public:
	AssetRegistry() = default;
	~AssetRegistry() = default;

	AssetRegistry(const AssetRegistry &) = delete;
	AssetRegistry &operator=(const AssetRegistry &) = delete;

	// Synchronous load: parses the .glb at `path` and stores the result under
	// `id`. Returns true on success, false on parse error (the last error
	// message can be retrieved via `GetAssetLoaderLastErrorMessage`).
	bool Load(const std::string &id, const std::string &path);

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

#endif
