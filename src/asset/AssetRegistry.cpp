#include "asset/AssetRegistry.hpp"

#include <algorithm>
#include <utility>

namespace projectv::asset {

std::expected<const LoadedAsset *, AssetLoadError> AssetRegistry::Load(const std::string &id, const std::string &path)
{
	// **Tier 1.B (`2026-06-13`).** `std::expected<const LoadedAsset *,
	// AssetLoadError>` — the success value is the freshly-loaded
	// pointer (already in `mEntries` after the std::move, so the
	// caller can use it directly without a second `Get(id)` round
	// trip). The error variant surfaces the specific failure
	// (currently `LoadGlbFailed`).
	auto loaded = LoadGlb(path);
	if (!loaded) {
		return std::unexpected(AssetLoadError::LoadGlbFailed);
	}
	std::lock_guard lock(mMutex);
	const auto it = mEntries.find(id);
	if (it == mEntries.end()) {
		mInsertionOrder.push_back(id);
	}
	LoadedAsset *rawPtr = loaded.get();
	mEntries[id] = std::move(loaded);
	return rawPtr;
}

const LoadedAsset *AssetRegistry::Get(const std::string &id) const
{
	std::lock_guard lock(mMutex);
	const auto it = mEntries.find(id);
	if (it == mEntries.end() || !it->second) {
		return nullptr;
	}
	return it->second.get();
}

void AssetRegistry::Unload(const std::string &id)
{
	std::lock_guard lock(mMutex);
	mEntries.erase(id);
	mInsertionOrder.erase(
		std::ranges::remove(mInsertionOrder, id).begin(),
		mInsertionOrder.end());
}

void AssetRegistry::Clear()
{
	std::lock_guard lock(mMutex);
	mEntries.clear();
	mInsertionOrder.clear();
}

std::vector<std::string> AssetRegistry::Ids() const
{
	std::lock_guard lock(mMutex);
	return mInsertionOrder;
}

std::size_t AssetRegistry::Size() const
{
	std::lock_guard lock(mMutex);
	return mEntries.size();
}

} // namespace projectv::asset
