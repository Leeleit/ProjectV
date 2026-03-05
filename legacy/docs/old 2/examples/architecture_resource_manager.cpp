// Пример: Архитектура ProjectV — Централизованное Управление Ресурсами
// Документация: docs/architecture/resource-management.md
// Уровень: 🔴 Высокий (Vulkan Resource Lifetime)

#define NOMINMAX
#include "vma/vk_mem_alloc.h"
#include "volk.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * РЕАЛИЗАЦИЯ RESOURCE MANAGER (из docs/architecture/resource-management.md)
 * 1. Type-erased handle (ResourceHandle)
 * 2. RAII и Reference Counting
 * 3. Type Erasure через интерфейсы (IResourceData)
 */

// Handle ресурса (32-битный ID + 8-битное поколение для безопасности)
struct ResourceHandle {
	uint32_t id : 24;
	uint32_t generation : 8;

	bool isValid() const { return id != 0; }
	bool operator==(const ResourceHandle &other) const = default;
};

enum class ResourceType : uint8_t { Unknown = 0, VulkanBuffer, VulkanImage, GLTFModel, COUNT };

// Базовый интерфейс для ресурсов (Type Erasure)
class IResourceData {
  public:
	virtual ~IResourceData() = default;
	virtual ResourceType getType() const = 0;
	virtual size_t getMemoryUsage() const = 0;
	virtual void destroy(VmaAllocator allocator) = 0;
};

// Конкретная реализация для Vulkan Buffer
class VulkanBufferData : public IResourceData {
  public:
	VulkanBufferData(VkBuffer buffer, VmaAllocation allocation, size_t size)
		: buffer_(buffer), allocation_(allocation), size_(size)
	{
	}

	ResourceType getType() const override { return ResourceType::VulkanBuffer; }
	size_t getMemoryUsage() const override { return size_; }
	void destroy(VmaAllocator allocator) override
	{
		if (buffer_ != VK_NULL_HANDLE) {
			vmaDestroyBuffer(allocator, buffer_, allocation_);
		}
	}

  private:
	VkBuffer buffer_;
	VmaAllocation allocation_;
	size_t size_;
};

class ResourceManager {
  public:
	static ResourceManager &get()
	{
		static ResourceManager instance;
		return instance;
	}

	// Создание ресурса (из документации)
	ResourceHandle createBuffer(const std::string &name, size_t size, VkBufferUsageFlags usage)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		// Поиск в кэше по имени
		if (auto it = nameToHandle_.find(name); it != nameToHandle_.end()) {
			ResourceEntry &entry = resources_[it->second.id];
			entry.refCount++;
			return it->second;
		}

		// Создание Vulkan буфера через VMA
		VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bufferInfo.size = size;
		bufferInfo.usage = usage;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		if (vmaCreateBuffer(vmaAllocator_, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
			return ResourceHandle{0, 0};
		}

		// Регистрация в менеджере
		uint32_t id = static_cast<uint32_t>(resources_.size());
		ResourceHandle handle = {id, 0};

		ResourceEntry entry;
		entry.data = std::make_unique<VulkanBufferData>(buffer, allocation, size);
		entry.refCount = 1;
		entry.name = name;

		resources_.push_back(std::move(entry));
		nameToHandle_[name] = handle;

		return handle;
	}

	void release(ResourceHandle handle)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (handle.id < resources_.size()) {
			resources_[handle.id].refCount--;
			if (resources_[handle.id].refCount == 0) {
				// Очистка при refCount == 0 (Garbage Collection)
				resources_[handle.id].data->destroy(vmaAllocator_);
				resources_[handle.id].data.reset();
				nameToHandle_.erase(resources_[handle.id].name);
			}
		}
	}

  private:
	struct ResourceEntry {
		std::unique_ptr<IResourceData> data;
		uint32_t refCount = 0;
		std::string name;
	};

	std::vector<ResourceEntry> resources_;
	std::unordered_map<std::string, ResourceHandle> nameToHandle_;
	std::mutex mutex_;
	VmaAllocator vmaAllocator_ = nullptr;

	ResourceManager() = default;
};

int main()
{
	// В реальности инициализируется в AppInit
	// auto& rm = ResourceManager::get();
	// auto handle = rm.createBuffer("VoxelChunk_0", 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	// rm.release(handle);
	return 0;
}
