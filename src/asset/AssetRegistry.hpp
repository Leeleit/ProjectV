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


	std::expected<const LoadedAsset *, AssetLoadError> Load(const std::string &id, const std::string &path);


	const LoadedAsset *Get(const std::string &id) const;

	void Unload(const std::string &id);

	void Clear();

	std::vector<std::string> Ids() const;

	std::size_t Size() const;

  private:
	mutable std::mutex mMutex;
	std::unordered_map<std::string, std::unique_ptr<LoadedAsset>> mEntries;
	std::vector<std::string> mInsertionOrder;
};

} // namespace projectv::asset

