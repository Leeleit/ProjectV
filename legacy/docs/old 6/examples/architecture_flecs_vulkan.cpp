// Пример: Flecs ECS + Vulkan 1.4 интеграция для ProjectV
// Документация: docs/architecture/flecs-vulkan-bridge.md
// Уровень: 🟡 Средний (требует знания Flecs и Vulkan basics)

// ============================================================================
// Демонстрация интеграции Flecs ECS с Vulkan 1.4 для воксельного движка:
// 1. ECS компоненты для Vulkan ресурсов
// 2. Observer-based resource lifecycle management
// 3. Multi-threaded command buffer recording
// 4. GPU-driven rendering с Flecs ECS
// 5. Bindless descriptor management через ECS
// 6. Async compute integration с timeline semaphores
// ============================================================================

#define NOMINMAX
#define VK_NO_PROTOTYPES
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

// Flecs ECS
#include "flecs.h"

// Tracy profiling (опционально)
// #define TRACY_ENABLE
#ifdef TRACY_ENABLE
#include "Tracy.hpp"
#include "TracyVulkan.hpp"
#endif

// GLM для математики
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

// ============================================================================
// Конфигурация
// ============================================================================

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t MAX_THREADS = 4;

// ============================================================================
// ECS компоненты для Vulkan ресурсов
// ============================================================================

// Базовый компонент для Vulkan ресурсов
struct VulkanResource {
	enum class Type { BUFFER, IMAGE, PIPELINE, DESCRIPTOR_SET };

	Type type;
	uint64_t last_used_frame = 0;
	bool needs_cleanup = false;
};

// Компонент для Vulkan буферов
struct VkBufferComponent {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkDeviceAddress device_address = 0;

	// Для sparse buffers
	bool is_sparse = false;
	size_t size = 0;
	VkBufferUsageFlags usage = 0;

	// Состояние
	bool needs_update = false;
	uint64_t last_update_frame = 0;

	// Методы жизненного цикла
	bool create(VmaAllocator allocator, const VkBufferCreateInfo &info, const VmaAllocationCreateInfo &alloc_info)
	{
		if (is_sparse) {
			// Для sparse buffers нужны специальные флаги
			// В реальном проекте здесь была бы полная реализация
		}

		VkResult result = vmaCreateBuffer(allocator, &info, &alloc_info, &buffer, &allocation, nullptr);
		return result == VK_SUCCESS;
	}

	void destroy(VmaAllocator allocator)
	{
		if (buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(allocator, buffer, allocation);
			buffer = VK_NULL_HANDLE;
			allocation = VK_NULL_HANDLE;
			device_address = 0;
		}
	}
};

// Компонент для Vulkan изображений
struct VkImageComponent {
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;

	// Свойства изображения
	VkFormat format = VK_FORMAT_UNDEFINED;
	uint32_t width = 0, height = 0;
	uint32_t mip_levels = 1;
	bool is_sparse = false;

	// Для texture atlas вокселей
	struct AtlasRegion {
		uint32_t x, y, width, height;
		uint32_t mip_level;
	};
	std::vector<AtlasRegion> atlas_regions;

	bool create(VmaAllocator allocator, const VkImageCreateInfo &info, const VmaAllocationCreateInfo &alloc_info)
	{
		VkResult result = vmaCreateImage(allocator, &info, &alloc_info, &image, &allocation, nullptr);
		if (result != VK_SUCCESS)
			return false;

		// Создание image view
		VkImageViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
										   .image = image,
										   .viewType = VK_IMAGE_VIEW_TYPE_2D,
										   .format = info.format,
										   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
																.baseMipLevel = 0,
																.levelCount = info.mipLevels,
																.baseArrayLayer = 0,
																.layerCount = 1}};

		return vkCreateImageView(vulkan_device, &view_info, nullptr, &view) == VK_SUCCESS;
	}

	void destroy(VkDevice device, VmaAllocator allocator)
	{
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(device, view, nullptr);
			view = VK_NULL_HANDLE;
		}
		if (image != VK_NULL_HANDLE) {
			vmaDestroyImage(allocator, image, allocation);
			image = VK_NULL_HANDLE;
			allocation = VK_NULL_HANDLE;
		}
	}
};

// Компонент для Vulkan пайплайнов
struct VkPipelineComponent {
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;

	// Modern Vulkan 1.4 features
	bool use_dynamic_rendering = true;
	bool use_mesh_shaders = false;

	// Для воксельного рендеринга
	enum class VoxelMode { GREEDY_MESHING, MARCHING_CUBES, MESH_SHADERS } voxel_mode = VoxelMode::GREEDY_MESHING;

	// Специализационные константы
	std::vector<uint32_t> specialization_constants;

	bool create(VkDevice device, const VkGraphicsPipelineCreateInfo &info)
	{
		if (use_dynamic_rendering) {
			// Dynamic rendering pipeline
			VkPipelineRenderingCreateInfo rendering_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
															.colorAttachmentCount = 1,
															.pColorAttachmentFormats = &swapchain_format,
															.depthAttachmentFormat = depth_format};

			VkGraphicsPipelineCreateInfo dynamic_info = info;
			dynamic_info.pNext = &rendering_info;
			dynamic_info.renderPass = VK_NULL_HANDLE; // No render pass for dynamic rendering

			return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &dynamic_info, nullptr, &pipeline) ==
				   VK_SUCCESS;
		}

		// Legacy render pass pipeline
		return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) == VK_SUCCESS;
	}

	void destroy(VkDevice device)
	{
		if (pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, pipeline, nullptr);
			pipeline = VK_NULL_HANDLE;
		}
		if (layout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(device, layout, nullptr);
			layout = VK_NULL_HANDLE;
		}
	}
};

// Компонент для descriptor buffers (bindless rendering)
struct VkDescriptorBufferComponent {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkDeviceAddress device_address = 0;

	// Bindless array management
	uint32_t descriptor_count = 0;
	uint32_t descriptor_size = 0;

	struct DescriptorSlot {
		bool allocated = false;
		uint32_t index = 0;
		uint64_t last_access_frame = 0;
		// В реальном проекте здесь была бы ссылка на ресурс
	};
	std::vector<DescriptorSlot> slots;

	// Методы управления
	bool allocate_slot(uint32_t &out_slot)
	{
		for (uint32_t i = 0; i < slots.size(); i++) {
			if (!slots[i].allocated) {
				slots[i].allocated = true;
				slots[i].last_access_frame = current_frame;
				out_slot = i;
				return true;
			}
		}

		// Расширение массива при необходимости
		if (descriptor_count < max_descriptors) {
			uint32_t new_slot = descriptor_count++;
			slots.resize(descriptor_count);
			slots[new_slot] = {true, new_slot, current_frame};
			out_slot = new_slot;

			// Расширение буфера
			resize_buffer(descriptor_count * descriptor_size);
			return true;
		}

		return false;
	}

	void free_slot(uint32_t slot)
	{
		if (slot < slots.size()) {
			slots[slot].allocated = false;
			// Опционально: дефрагментация
		}
	}
};

// Компонент для воксельного чанка
struct VoxelChunkComponent {
	// Ссылки на Vulkan ресурсы
	flecs::entity voxel_buffer_entity;
	flecs::entity indirect_buffer_entity;
	flecs::entity mesh_buffer_entity;

	// Состояние чанка
	glm::ivec3 chunk_position = {0, 0, 0};
	uint32_t lod_level = 0;
	bool is_visible = true;
	bool needs_remesh = false;

	// Compute dispatch параметры
	struct DispatchParams {
		uint32_t group_count_x = 0;
		uint32_t group_count_y = 0;
		uint32_t group_count_z = 0;
	} dispatch_params;

	// Метрики
	uint32_t triangle_count = 0;
	uint64_t last_render_time_ns = 0;

	void schedule_remesh(flecs::world &world)
	{
		needs_remesh = true;

		// Добавление задачи compute системы
		auto *compute_scheduler = world.get<ComputeScheduler>();
		if (compute_scheduler) {
			ComputeTask task = {.type = ComputeTaskType::VOXEL_MESH_GENERATION,
								.chunk_entity = entity_handle,
								.priority = lod_level == 0 ? ComputePriority::HIGH : ComputePriority::MEDIUM};
			compute_scheduler->add_task(task);
		}
	}
};

// Компонент для материала вокселей
struct VoxelMaterialComponent {
	// Material properties
	glm::vec4 base_color = {1.0f, 1.0f, 1.0f, 1.0f};
	float metallic = 0.0f;
	float roughness = 0.5f;
	float emission_strength = 0.0f;

	// Bindless descriptor индексы
	uint32_t albedo_descriptor_index = 0;
	uint32_t normal_descriptor_index = 0;
	uint32_t metallic_roughness_descriptor_index = 0;
	uint32_t emission_descriptor_index = 0;

	// Состояние материала
	bool is_transparent = false;
	bool casts_shadow = true;
	bool receives_shadow = true;
	bool needs_descriptor_update = false;

	// Для оптимизации рендеринга
	enum class RenderQueue { OPAQUE, TRANSPARENT, EMISSIVE } render_queue = RenderQueue::OPAQUE;

	void update_descriptor_indices(VkDescriptorBufferComponent &descriptor_buffer)
	{
		// В реальном проекте здесь было бы обновление индексов дескрипторов
		needs_descriptor_update = false;
	}
};

// Компонент для трансформации
struct Transform {
	glm::vec3 position = {0.0f, 0.0f, 0.0f};
	glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
	glm::vec3 scale = {1.0f, 1.0f, 1.0f};
	glm::mat4 world_matrix = glm::mat4(1.0f);
	bool dirty = true;

	void update_world_matrix()
	{
		if (dirty) {
			glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
			glm::mat4 rotation_mat = glm::mat4_cast(rotation);
			glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);
			world_matrix = translation * rotation_mat * scale_mat;
			dirty = false;
		}
	}
};

// Компонент для состояния рендеринга
struct RenderState {
	bool is_visible = true;
	uint32_t render_layer = 0;
	float lod_distance = 0.0f;
	uint64_t last_frame_rendered = 0;
};

// Компонент для compute задач
struct ComputeTask {
	enum class Type { VOXEL_MESH_GENERATION, TEXTURE_PROCESSING, PHYSICS_UPDATE, AI_PATHFINDING } type;

	enum class Priority { LOW, MEDIUM, HIGH, CRITICAL } priority = Priority::MEDIUM;

	flecs::entity target_entity;
	uint64_t estimated_workload = 0;
	uint64_t scheduled_frame = 0;
};

// Компонент для timeline semaphores
struct TimelineSemaphore {
	VkSemaphore semaphore = VK_NULL_HANDLE;
	std::atomic<uint64_t> current_value{0};
	std::string name;

	bool create(VkDevice device, const std::string &semaphore_name)
	{
		name = semaphore_name;

		VkSemaphoreTypeCreateInfo timeline_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
												   .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
												   .initialValue = 0};

		VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
												.pNext = &timeline_info};

		return vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore) == VK_SUCCESS;
	}

	void destroy(VkDevice device)
	{
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, semaphore, nullptr);
			semaphore = VK_NULL_HANDLE;
		}
	}

	uint64_t signal_next() { return ++current_value; }

	void wait_for_value(uint64_t value)
	{
		VkSemaphoreWaitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
										 .semaphoreCount = 1,
										 .pSemaphores = &semaphore,
										 .pValues = &value};

		vkWaitSemaphores(vulkan_device, &wait_info, UINT64_MAX);
	}
};

// ============================================================================
// Класс FlecsVulkanIntegration
// ============================================================================

class FlecsVulkanIntegration {
  public:
	FlecsVulkanIntegration()
	{
		init_window();
		init_vulkan();
		init_flecs();
		init_observers();
		init_systems();
		init_threads();
	}

	~FlecsVulkanIntegration() { cleanup(); }

	void run() { main_loop(); }

  private:
	// SDL и Vulkan
	SDL_Window *window_ = nullptr;
	VkInstance instance_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	VkQueue compute_queue_ = VK_NULL_HANDLE;
	VmaAllocator allocator_ = VK_NULL_HANDLE;

	// Flecs ECS
	flecs::world world_;

	// Timeline semaphores
	std::unique_ptr<TimelineSemaphore> graphics_semaphore_;
	std::unique_ptr<TimelineSemaphore> compute_semaphore_;

	// Thread management
	std::vector<std::thread> worker_threads_;
	std::atomic<bool> running_{true};

	// Frame data
	uint64_t current_frame_ = 0;
	std::chrono::high_resolution_clock::time_point start_time_;

	// Tracy контекст (опционально)
#ifdef TRACY_ENABLE
	TracyVkCtx tracy_context_ = nullptr;
#endif

	// ========================================================================
	// Инициализация
	// ========================================================================

	void init_window()
	{
		SDL_Init(SDL_INIT_VIDEO);
		window_ = SDL_CreateWindow("Flecs + Vulkan 1.4 Integration", WIDTH, HEIGHT, SDL_WINDOW_VULKAN);
	}

	void init_vulkan()
	{
		// В реальном проекте здесь была бы полная инициализация Vulkan
		// Для примера оставляем заглушку
	}

	void init_flecs()
	{
		// Регистрация компонентов
		world_.component<VkBufferComponent>("VkBufferComponent");
		world_.component<VkImageComponent>("VkImageComponent");
		world_.component<VkPipelineComponent>("VkPipelineComponent");
		world_.component<VkDescriptorBufferComponent>("VkDescriptorBufferComponent");
		world_.component<VoxelChunkComponent>("VoxelChunkComponent");
		world_.component<VoxelMaterialComponent>("VoxelMaterialComponent");
		world_.component<Transform>("Transform");
		world_.component<RenderState>("RenderState");
		world_.component<ComputeTask>("ComputeTask");

		// Создание синглтон-компонентов
		world_.set<TimelineSemaphore>({});

		// Инициализация timeline semaphores
		graphics_semaphore_ = std::make_unique<TimelineSemaphore>();
		compute_semaphore_ = std::make_unique<TimelineSemaphore>();

		if (device_ != VK_NULL_HANDLE) {
			graphics_semaphore_->create(device_, "GraphicsTimeline");
			compute_semaphore_->create(device_, "ComputeTimeline");
		}

		start_time_ = std::chrono::high_resolution_clock::now();
	}

	void init_observers()
	{
		// Observer для автоматического создания Vulkan буферов
		world_.observer<VkBufferComponent>("VkBufferCreationObserver")
			.event(flecs::OnAdd)
			.each([this](flecs::entity e, VkBufferComponent &buffer) {
				if (buffer.buffer == VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
					VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
													  .size = buffer.size,
													  .usage = buffer.usage,
													  .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

					VmaAllocationCreateInfo alloc_info = {
						.usage = VMA_MEMORY_USAGE_AUTO,
						.flags = buffer.is_sparse ? VMA_ALLOCATION_CREATE_SPARSE_BINDING_BIT : 0};

					if (buffer.create(allocator_, buffer_info, alloc_info)) {
						SDL_Log("Created Vulkan buffer for entity %s", e.name().c_str());
					} else {
						SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan buffer for entity %s",
									 e.name().c_str());
						e.remove<VkBufferComponent>();
					}
				}
			});

		// Observer для очистки Vulkan буферов
		world_.observer<VkBufferComponent>("VkBufferDestructionObserver")
			.event(flecs::OnRemove | flecs::OnDelete)
			.each([this](flecs::entity e, VkBufferComponent &buffer) {
				if (buffer.buffer != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
					buffer.destroy(allocator_);
					SDL_Log("Destroyed Vulkan buffer for entity %s", e.name().c_str());
				}
			});

		// Observer для обновления материалов
		world_.observer<VoxelMaterialComponent>("MaterialUpdateObserver")
			.event(flecs::OnSet)
			.each([](flecs::entity e, VoxelMaterialComponent &material) {
				material.needs_descriptor_update = true;

				// Помечаем связанные сущности для обновления
				// В реальном проекте здесь была бы логика обновления связанных объектов
			});

		// Observer для планирования remesh воксельных чанков
		world_.observer<VoxelChunkComponent>("ChunkRemeshObserver")
			.event(flecs::OnSet)
			.term<VoxelChunkComponent>()
			.with<NeedsRemesh>()
			.each([this](flecs::entity e, VoxelChunkComponent &chunk) { chunk.schedule_remesh(world_); });
	}

	void init_systems()
	{
		// Система обновления трансформаций
		world_.system<Transform>("TransformUpdateSystem")
			.kind(flecs::OnUpdate)
			.multi_threaded()
			.each([](Transform &transform) {
				if (transform.dirty) {
					transform.update_world_matrix();
				}
			});

		// Система обновления видимости (frustum culling)
		world_.system<Transform, RenderState>("VisibilitySystem")
			.kind(flecs::PreStore)
			.multi_threaded()
			.each([this](Transform &transform, RenderState &state) {
				// Простой frustum culling
				// В реальном проекте здесь была бы сложная логика
				state.is_visible = true; // Заглушка

				// Обновление LOD расстояния
				glm::vec3 camera_pos = {0.0f, 0.0f, 5.0f}; // Заглушка
				state.lod_distance = glm::distance(camera_pos, transform.position);
			});

		// Система обновления материалов
		world_.system<VoxelMaterialComponent>("MaterialUpdateSystem")
			.kind(flecs::OnUpdate)
			.each([](VoxelMaterialComponent &material) {
				if (material.needs_descriptor_update) {
					// В реальном проекте здесь было бы обновление дескрипторов
					material.needs_descriptor_update = false;
				}
			});

		// Система записи команд рендеринга (многопоточная)
		world_.system<const Transform, const RenderState>("CommandRecordingSystem")
			.kind(flecs::OnStore)
			.multi_threaded()
			.ctx([this](flecs::world &w) {
				// Получение thread-local контекста
				int thread_id = w.get_thread_index();
				return get_thread_command_context(thread_id);
			})
			.iter([this](flecs::iter &it, const Transform *transforms, const RenderState *states) {
				auto *thread_context = static_cast<ThreadCommandContext *>(it.ctx());
				if (!thread_context)
					return;

				VkCommandBuffer cmd = thread_context->get_command_buffer();

				for (int i = 0; i < it.count(); i++) {
					if (!states[i].is_visible)
						continue;

					// Запись команд рендеринга для видимой сущности
					// В реальном проекте здесь была бы bindless рендеринг с динамическими rendering
					// Для примера: запись draw команд с трансформацией

					// Устанавливаем push constants для трансформации
					glm::mat4 mvp = transforms[i].world_matrix; // В реальном проекте: view_proj * world
					vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

					// Draw call (в реальном проекте был бы indirect draw для вокселей)
					vkCmdDraw(cmd, 3, 1, 0, 0); // Треугольник-заглушка

				// Tracy profiling для каждого draw call
#ifdef TRACY_ENABLE
					TracyVkZone(tracy_context_, cmd);
					TracyVkZoneEnd(tracy_context_, cmd);
#endif
				}

			// Tracy profiling для всей системы
#ifdef TRACY_ENABLE
				TracyVkZone(tracy_context_, cmd);
				TracyVkZoneEnd(tracy_context_, cmd);
#endif
			});
	}

	// ========================================================================
	// Thread management и command context
	// ========================================================================

	struct ThreadCommandContext {
		VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
		uint32_t thread_id = 0;
		flecs::world *world = nullptr;

		VkCommandBuffer get_command_buffer() const { return cmd_buffer; }
	};

	void *get_thread_command_context(int thread_id)
	{
		// В реальном проекте здесь был бы thread-local storage
		// Для примера используем статический массив
		static std::array<ThreadCommandContext, MAX_THREADS> thread_contexts;

		if (thread_id >= 0 && thread_id < MAX_THREADS) {
			thread_contexts[thread_id].thread_id = thread_id;
			thread_contexts[thread_id].world = &world_;
			return &thread_contexts[thread_id];
		}
		return nullptr;
	}

	void init_threads()
	{
		// В реальном проекте здесь была бы инициализация пула воркеров
		worker_threads_.resize(MAX_THREADS - 1); // Главный поток уже есть

		for (size_t i = 0; i < worker_threads_.size(); i++) {
			worker_threads_[i] = std::thread([this, i]() {
				worker_thread_func(i + 1); // ID начинаются с 1 (0 - главный)
			});
		}
	}

	void worker_thread_func(int thread_id)
	{
		// Инициализация thread-local ресурсов
		auto *thread_context = static_cast<ThreadCommandContext *>(get_thread_command_context(thread_id));

		if (thread_context) {
			// В реальном проекте здесь было бы создание command buffer
			// thread_context->cmd_buffer = ...
		}

		while (running_) {
			// Выполнение систем на этом потоке
			world_.run_worker(thread_id);

			// Управление sleep/polling
			std::this_thread::yield();
		}
	}

	// ========================================================================
	// Основной цикл
	// ========================================================================

	void main_loop()
	{
		bool running = true;
		SDL_Event event;

		while (running && running_) {
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_EVENT_QUIT ||
					(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
					running = false;
					running_ = false;
				}
			}

			// Обновление текущего кадра
			current_frame_++;

			// Обновление систем (главный поток)
			world_.progress();

			// Синхронизация с timeline semaphores
			sync_with_timeline_semaphores();

			// Tracy profiling для кадра
#ifdef TRACY_ENABLE
			TracyVkZone(tracy_context_, nullptr);
			TracyVkZoneEnd(tracy_context_, nullptr);
#endif

			// Управление FPS (грубое)
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
	}

	void sync_with_timeline_semaphores()
	{
		// В реальном проекте здесь была бы синхронизация через timeline semaphores
		if (graphics_semaphore_) {
			uint64_t signal_value = graphics_semaphore_->signal_next();
			// Ждём предыдущее значение от compute
			if (compute_semaphore_) {
				compute_semaphore_->wait_for_value(signal_value - 1);
			}
		}
	}

	// ========================================================================
	// Очистка
	// ========================================================================

	void cleanup()
	{
		running_ = false;

		// Остановка воркер-потоков
		for (auto &thread : worker_threads_) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		// Очистка timeline semaphores
		if (device_ != VK_NULL_HANDLE) {
			graphics_semaphore_->destroy(device_);
			compute_semaphore_->destroy(device_);
		}

		// Очистка Vulkan
		if (allocator_ != VK_NULL_HANDLE) {
			vmaDestroyAllocator(allocator_);
		}

		if (device_ != VK_NULL_HANDLE) {
			vkDestroyDevice(device_, nullptr);
		}

		if (instance_ != VK_NULL_HANDLE) {
			vkDestroyInstance(instance_, nullptr);
		}

		// Очистка SDL
		SDL_DestroyWindow(window_);
		SDL_Quit();
	}

	// Для совместимости с кодом
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
	VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
	VkDevice vulkan_device = VK_NULL_HANDLE;
	uint64_t current_frame = 0;
	uint32_t max_descriptors = 65536;
	flecs::entity entity_handle;
	struct ComputeScheduler {};
	struct NeedsRemesh {};
	enum ComputeTaskType { VOXEL_MESH_GENERATION };
	enum ComputePriority { LOW, MEDIUM, HIGH };

	void resize_buffer(size_t new_size)
	{
		// Заглушка для resize буфера
	}
};

// ============================================================================
// Главная функция с примером создания сущностей
// ============================================================================

int main()
{
	SDL_Log("ProjectV Flecs + Vulkan 1.4 Integration Example");
	SDL_Log("==============================================");
	SDL_Log("");
	SDL_Log("Ключевые фичи интеграции:");
	SDL_Log("");
	SDL_Log("1. ECS компоненты для Vulkan ресурсов");
	SDL_Log("   - Автоматический lifecycle management через observers");
	SDL_Log("   - Компоненты для buffer, image, pipeline, descriptor buffer");
	SDL_Log("   - Ссылки на Vulkan ресурсы через flecs::entity");
	SDL_Log("");
	SDL_Log("2. Observer-based resource lifecycle");
	SDL_Log("   - Автоматическое создание при добавлении компонента");
	SDL_Log("   - Автоматическое уничтожение при удалении компонента");
	SDL_Log("   - Observer'ы для обновления материалов и чанков");
	SDL_Log("");
	SDL_Log("3. Multi-threaded command buffer recording");
	SDL_Log("   - Thread-local контексты для командных буферов");
	SDL_Log("   - Распределение систем по потокам");
	SDL_Log("   - Flecs worker threads для параллельного выполнения");
	SDL_Log("");
	SDL_Log("4. GPU-driven rendering с Flecs ECS");
	SDL_Log("   - Компоненты для воксельных чанков и материалов");
	SDL_Log("   - Compute задачи для mesh generation через ECS");
	SDL_Log("   - Bindless descriptor management");
	SDL_Log("");
	SDL_Log("5. Async compute integration");
	SDL_Log("   - Timeline semaphores для точной синхронизации");
	SDL_Log("   - Компоненты для compute задач");
	SDL_Log("   - Управление приоритетами compute dispatch");
	SDL_Log("");
	SDL_Log("См. документацию: docs/architecture/flecs-vulkan-bridge.md");
	SDL_Log("");
	SDL_Log("Запуск примера с созданием тестовых сущностей...");

	try {
		FlecsVulkanIntegration app;

		// Пример создания сущностей с компонентами
		flecs::world &world = app.world_;

		// Создаём несколько тестовых сущностей с трансформациями
		for (int i = 0; i < 10; i++) {
			flecs::entity entity =
				world.entity()
					.set<Transform>({{.position = glm::vec3(i * 2.0f, 0.0f, 0.0f),
									  .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
									  .scale = glm::vec3(1.0f, 1.0f, 1.0f),
									  .world_matrix = glm::mat4(1.0f),
									  .dirty = true}})
					.set<RenderState>(
						{{.is_visible = true, .render_layer = 0, .lod_distance = 0.0f, .last_frame_rendered = 0}});

			// Каждая третья сущность получает материал
			if (i % 3 == 0) {
				entity.set<VoxelMaterialComponent>({{.base_color = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f),
													 .metallic = 0.0f,
													 .roughness = 0.5f,
													 .emission_strength = 0.0f,
													 .albedo_descriptor_index = 0,
													 .normal_descriptor_index = 0,
													 .metallic_roughness_descriptor_index = 0,
													 .emission_descriptor_index = 0,
													 .is_transparent = false,
													 .casts_shadow = true,
													 .receives_shadow = true,
													 .needs_descriptor_update = false,
													 .render_queue = VoxelMaterialComponent::RenderQueue::OPAQUE}});
			}

			SDL_Log("Создана тестовая сущность %d", i);
		}

		// Создаём воксельный чанк
		flecs::entity voxel_chunk = world.entity().set<VoxelChunkComponent>({{.chunk_position = glm::ivec3(0, 0, 0),
																			  .lod_level = 0,
																			  .is_visible = true,
																			  .needs_remesh = false,
																			  .dispatch_params = {},
																			  .triangle_count = 0,
																			  .last_render_time_ns = 0}});

		SDL_Log("Создан воксельный чанк");

		// Добавляем тестовый Vulkan buffer
		flecs::entity buffer_entity =
			world.entity().set<VkBufferComponent>({{.buffer = VK_NULL_HANDLE,
													.allocation = VK_NULL_HANDLE,
													.device_address = 0,
													.is_sparse = false,
													.size = 1024 * 1024, // 1MB
													.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
													.needs_update = false,
													.last_update_frame = 0}});

		SDL_Log("Создана сущность с Vulkan buffer (observer создаст ресурс при наличии allocator)");

		// Запуск приложения
		app.run();

		SDL_Log("Приложение завершено");

	} catch (const std::exception &e) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Ошибка: %s", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
