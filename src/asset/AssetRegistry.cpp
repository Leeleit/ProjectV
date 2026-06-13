#include "asset/AssetRegistry.hpp"

#include <algorithm>
#include <utility>

namespace projectv::asset {

bool AssetRegistry::Load(const std::string &id, const std::string &path)
{
	auto loaded = LoadGlb(path);
	if (!loaded) {
		return false;
	}
	std::lock_guard lock(mMutex);
	const auto it = mEntries.find(id);
	if (it == mEntries.end()) {
		mInsertionOrder.push_back(id);
	}
	mEntries[id] = std::move(loaded);
	return true;
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
