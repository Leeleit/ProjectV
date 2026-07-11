#include "asset/AssetRegistry.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <algorithm>
#include <utility>

namespace projectv::asset {

std::expected<const LoadedAsset *, AssetLoadError> AssetRegistry::Load(const std::string &id, const std::string &path)
{
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
	std::erase(mInsertionOrder, id);
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
	std::vector<std::string> result = mInsertionOrder;
	return result;
}

std::size_t AssetRegistry::Size() const
{
	std::lock_guard lock(mMutex);
	return mEntries.size();
}

} // namespace projectv::asset
