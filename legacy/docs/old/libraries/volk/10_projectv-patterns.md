# Паттерны использования volk в ProjectV

**🟡 Уровень 2: Средний**

Рекомендуемые практики и архитектурные решения.

---

## Паттерн 1: Единая точка инициализации

### Проблема

Vulkan функции должны быть загружены до использования, но не раньше создания instance/device.

### Решение

Инкапсуляция инициализации в одном классе:

```cpp
class VulkanContext {
public:
    static VulkanContext& get() {
        static VulkanContext instance;
        return instance;
    }

    bool initialize(SDL_Window* window) {
        if (m_initialized) return true;

        if (!initVolk()) return false;
        if (!createInstance(window)) return false;
        if (!createDevice()) return false;

        m_initialized = true;
        return true;
    }

    VkInstance instance() const { return m_instance; }
    VkDevice device() const { return m_device; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }

private:
    VulkanContext() = default;
    bool m_initialized = false;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
};

// Использование
VulkanContext::get().initialize(window);
vkCmdDraw(...);  // Функции уже загружены
```

---

## Паттерн 2: RAII для Vulkan ресурсов

### Проблема

Ручное управление destroy-функциями ведёт к утечкам.

### Решение

```cpp
template <typename Handle, typename DestroyFunc>
class VulkanHandle {
public:
    VulkanHandle() = default;
    VulkanHandle(Handle handle, DestroyFunc destroy, VkDevice device)
        : m_handle(handle), m_destroy(destroy), m_device(device) {}

    ~VulkanHandle() {
        if (m_handle != VK_NULL_HANDLE && m_destroy) {
            m_destroy(m_device, m_handle, nullptr);
        }
    }

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    VulkanHandle(VulkanHandle&& other) noexcept
        : m_handle(other.m_handle), m_destroy(other.m_destroy), m_device(other.m_device) {
        other.m_handle = VK_NULL_HANDLE;
    }

    VulkanHandle& operator=(VulkanHandle&& other) noexcept {
        if (this != &other) {
            if (m_handle != VK_NULL_HANDLE) {
                m_destroy(m_device, m_handle, nullptr);
            }
            m_handle = other.m_handle;
            m_destroy = other.m_destroy;
            m_device = other.m_device;
            other.m_handle = VK_NULL_HANDLE;
        }
        return *this;
    }

    Handle get() const { return m_handle; }
    operator Handle() const { return m_handle; }

private:
    Handle m_handle = VK_NULL_HANDLE;
    DestroyFunc m_destroy = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
};

// Использование
using BufferHandle = VulkanHandle<VkBuffer, PFN_vkDestroyBuffer>;

BufferHandle createBuffer(VkDevice device, const VkBufferCreateInfo& info) {
    VkBuffer buffer;
    vkCreateBuffer(device, &info, nullptr, &buffer);
    return BufferHandle(buffer, vkDestroyBuffer, device);
}
```

---

## Паттерн 3: Множественные устройства

### Проблема

`volkLoadDevice` перезаписывает глобальные указатели, что не работает с несколькими GPU.

### Решение

Использование `VolkDeviceTable`:

```cpp
class VulkanDevice {
public:
    VulkanDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo& createInfo) {
        vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_device);
        volkLoadDeviceTable(&m_table, m_device);
    }

    ~VulkanDevice() {
        if (m_device != VK_NULL_HANDLE) {
            m_table.vkDestroyDevice(m_device, nullptr);
        }
    }

    void submit(VkQueue queue, const VkSubmitInfo& submit) {
        m_table.vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    }

    void draw(VkCommandBuffer cmd, uint32_t vertexCount) {
        m_table.vkCmdDraw(cmd, vertexCount, 1, 0, 0);
    }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VolkDeviceTable m_table;
};
```

---

## Паттерн 4: Deferred initialization

### Проблема

Некоторые компоненты требуют Vulkan, но инициализируются до создания device.

### Решение

Колбэк-система:

```cpp
class VulkanInitializer {
public:
    using InitCallback = std::function<bool(VkInstance, VkDevice)>;

    void registerCallback(const std::string& name, InitCallback callback) {
        m_callbacks[name] = std::move(callback);
    }

    bool initialize(SDL_Window* window) {
        // ... создание instance, device

        for (auto& [name, callback] : m_callbacks) {
            if (!callback(m_instance, m_device)) {
                fprintf(stderr, "Callback %s failed\n", name.c_str());
                return false;
            }
        }
        return true;
    }

private:
    std::unordered_map<std::string, InitCallback> m_callbacks;
};

// Использование в других модулях
VulkanInitializer::get().registerCallback("ImGui", [](VkInstance inst, VkDevice dev) {
    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = inst;
    info.Device = dev;
    return ImGui_ImplVulkan_Init(&info, nullptr);
});
```

---

## Паттерн 5: Debug/Release конфигурация

### Проблема

Debug требует validation layers, Release — максимальной производительности.

### Решение

```cpp
class VulkanConfig {
public:
    static constexpr bool isDebug() {
#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }

    static std::vector<const char*> getLayers() {
        if (isDebug()) {
            return {"VK_LAYER_KHRONOS_validation"};
        }
        return {};
    }

    static VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerInfo() {
        VkDebugUtilsMessengerCreateInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        info.pfnUserCallback = debugCallback;
        return info;
    }

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* userData) {
        fprintf(stderr, "Validation: %s\n", data->pMessage);
        return VK_FALSE;
    }
};
```

---

## Паттерн 6: Error handling

### Проблема

Vulkan функции возвращают VkResult, ошибки нужно обрабатывать централизованно.

### Решение

```cpp
#define VK_CHECK(call) \
    do { \
        VkResult result = call; \
        if (result != VK_SUCCESS) { \
            fprintf(stderr, "Vulkan error %d at %s:%d: %s\n", \
                    result, __FILE__, __LINE__, #call); \
            return false; \
        } \
    } while(0)

// Использование
bool createBuffer() {
    VK_CHECK(vkCreateBuffer(device, &info, nullptr, &buffer));
    return true;
}
```

С исключениями (требует noexcept(false)):

```cpp
class VulkanException : public std::runtime_error {
public:
    VulkanException(VkResult result, const char* message)
        : std::runtime_error(formatMessage(result, message)), m_result(result) {}

    VkResult result() const { return m_result; }

private:
    static std::string formatMessage(VkResult result, const char* message) {
        return std::string(message) + " (VkResult: " + std::to_string(result) + ")";
    }

    VkResult m_result;
};

#define VK_THROW(call) \
    do { \
        VkResult result = call; \
        if (result != VK_SUCCESS) { \
            throw VulkanException(result, #call); \
        } \
    } while(0)
```

---

## Паттерн 7: Resource pools

### Проблема

Частое создание/удаление ресурсов снижает производительность.

### Решение

```cpp
template <typename Handle, typename CreateFunc, typename DestroyFunc>
class ResourcePool {
public:
    ResourcePool(CreateFunc create, DestroyFunc destroy, VkDevice device)
        : m_create(create), m_destroy(destroy), m_device(device) {}

    Handle acquire(const typename CreateFunc::argument_type& createInfo) {
        if (!m_free.empty()) {
            Handle handle = m_free.back();
            m_free.pop_back();
            return handle;
        }

        Handle handle;
        m_create(m_device, &createInfo, nullptr, &handle);
        return handle;
    }

    void release(Handle handle) {
        m_free.push_back(handle);
    }

    ~ResourcePool() {
        for (Handle handle : m_free) {
            m_destroy(m_device, handle, nullptr);
        }
    }

private:
    CreateFunc m_create;
    DestroyFunc m_destroy;
    VkDevice m_device;
    std::vector<Handle> m_free;
};
```

---

## Рекомендации

| Паттерн                    | Когда использовать                |
|----------------------------|-----------------------------------|
| Единая точка инициализации | Большинство проектов              |
| RAII                       | Все проекты                       |
| Множественные устройства   | Multi-GPU системы                 |
| Deferred initialization    | Сложные приложения                |
| Debug/Release конфигурация | Все проекты                       |
| Error handling             | Все проекты                       |
| Resource pools             | Высокопроизводительные приложения |
