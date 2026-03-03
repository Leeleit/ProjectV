# Управление Памятью и RAII (Memory Management)

**🟢 Уровень 1: Начинающий** — Безопасное управление памятью для воксельного движка ProjectV.

В ProjectV мы не используем `new` и `delete` напрямую. Это небезопасно и ведет к фрагментации памяти и утечкам. Мы
опираемся на идиому **RAII** и умные указатели из Modern C++.

---

## 1. RAII (Resource Acquisition Is Initialization)

Это сердце C++. Идея проста: ресурс (память, файл, мьютекс, Vulkan объект) захватывается в конструкторе объекта и
**всегда** освобождается в его деструкторе.

```cpp
class Texture {
public:
    Texture(const char* path) {
        // Загрузка файла, выделение памяти на GPU
        m_handle = vkCreateImage(...);
    }

    ~Texture() {
        // ОСВОБОЖДЕНИЕ: вызовется автоматически при выходе из области видимости
        vkDestroyImage(m_handle);
    }

    // Запрещаем копирование, чтобы не удалить одну и ту же текстуру дважды!
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Разрешаем перемещение
    Texture(Texture&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = VK_NULL_HANDLE;
    }

    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (m_handle != VK_NULL_HANDLE) {
                vkDestroyImage(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = VK_NULL_HANDLE;
        }
        return *this;
    }

private:
    VkImage m_handle{VK_NULL_HANDLE};
};

void render() {
    Texture t("grass.png"); // Выделение
    // ... рендеринг ...
} // <--- t.~Texture() вызовется САМ. Память освобождена. Никаких утечек.
```

**Для ProjectV:** Все Vulkan объекты должны быть обернуты в RAII классы. Это гарантирует отсутствие утечек даже при
исключениях.

---

## 2. Умные указатели (Smart Pointers)

Вместо сырых указателей используйте обертки из `<memory>`.

### std::unique_ptr (Единоличное владение)

* Самый быстрый и эффективный (размер как у сырого указателя).
* Только **один** владелец. Нельзя копировать, можно только передавать владение (`std::move`).

```cpp
// unique_ptr для Vulkan буфера с кастомным делетером
struct BufferDeleter {
    VkDevice device;
    void operator()(VkBuffer buffer) const {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
    }
};

using UniqueBuffer = std::unique_ptr<VkBuffer_T, BufferDeleter>;

UniqueBuffer createVertexBuffer(VkDevice device, const std::vector<Vertex>& vertices) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = sizeof(Vertex) * vertices.size();
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkBuffer buffer;
    vkCreateBuffer(device, &info, nullptr, &buffer);
    return UniqueBuffer(buffer, BufferDeleter{device});
}
```

### std::shared_ptr (Совместное владение)

* Несколько владельцев. Счетчик ссылок.
* Объект удаляется, когда счетчик станет равен 0.
* **Используйте осторожно**: атомарный счетчик — это накладные расходы. В ECS почти не нужен.

```cpp
// shared_ptr для текстур, используемых несколькими мешами
auto grassTexture = std::make_shared<Texture>("textures/grass.png");
auto mesh1 = Mesh{grassTexture};
auto mesh2 = Mesh{grassTexture}; // Обе меши используют одну текстуру
```

### std::weak_ptr (Слабая ссылка)

* Смотрит на `shared_ptr`, но не увеличивает счетчик.
* Позволяет проверить, "жив" ли еще объект. Полезно для кэшей.

```cpp
class TextureCache {
    std::unordered_map<std::string, std::weak_ptr<Texture>> cache;

public:
    std::shared_ptr<Texture> get(const std::string& path) {
        auto it = cache.find(path);
        if (it != cache.end()) {
            if (auto texture = it->second.lock()) {
                return texture; // Текстура еще в памяти
            }
        }

        // Загружаем новую текстуру
        auto texture = std::make_shared<Texture>(path);
        cache[path] = texture;
        return texture;
    }
};
```

---

## 3. Стек vs Куча (Stack vs Heap)

**Стек** — ваш лучший друг. Он невероятно быстр. По возможности используйте его.

```cpp
// Плохо: куча, аллокация, медленно (для маленьких массивов)
std::vector<float> positions(3);

// Хорошо: стек, 0 накладных расходов
std::array<float, 3> positions;
float pos[3];

// Используйте std::vector только когда размер НЕИЗВЕСТЕН заранее
```

**Для ProjectV:** Компоненты ECS (Transform, Velocity и т.д.) должны быть простыми структурами, которые можно размещать
на стеке или в плотных массивах flecs.

---

## 4. Move Semantics (Перемещение)

В C++20 объекты не нужно копировать (медленно), их можно **перемещать** (быстро).

```cpp
std::vector<Voxel> chunkA = generateChunk();
// Перемещаем данные из chunkA в chunkB (указатель просто перекидывается)
std::vector<Voxel> chunkB = std::move(chunkA);
// chunkA теперь пуст. Данные в chunkB без копирования миллионов вокселей.
```

Для тяжелых компонентов (Меши, Текстуры, Чанки) всегда реализуйте **Move Constructor** и используйте `std::move`.

---

## 5. Cache Alignment и False Sharing

> **Метафора:** Cache line (64 байта на x86-64) — это "страница книги", которую процессор читает за один раз. Когда один
> поток пишет в переменную, он "пачкает" всю страницу. Если другой поток работает с переменной на той же странице — он
> вынужден ждать, пока страница обновится в кэше другого ядра. Это называется **False Sharing**: потоки не делят данные
> логически, но делят физическую cache line.

### Проблема False Sharing

```cpp
// ПЛОХО: Счетчики в одной cache line
struct Counters {
    std::atomic<int> counter1;  // Поток 1 пишет
    std::atomic<int> counter2;  // Поток 2 пишет
    // Оба в одной cache line (64 байта) — false sharing!
};
// Результат: каждое обновление инвалидирует кэш у другого потока

// ХОРОШО: Каждый счетчик на своей cache line
struct alignas(64) CacheAlignedCounter {
    std::atomic<int> value;
    char padding[64 - sizeof(std::atomic<int>)];
};

struct Counters {
    CacheAlignedCounter counter1;  // Своя cache line
    CacheAlignedCounter counter2;  // Своя cache line
};
```

> **Для понимания:** False Sharing — это скрытый враг многопоточности. Код выглядит правильно, потоки работают с разными
> переменными, но производительность падает в разы. Всегда выравнивайте данные, к которым обращаются разные потоки, на
> границу cache line (64 байта).

### Выравнивание для SIMD

```cpp
// Выравнивание критично для SIMD операций
struct alignas(32) SIMDData {  // 32 байта для AVX2
    float values[8];  // __m256 требует выравнивание на 32 байта
};

// C++17: aligned_new для динамического выделения
auto data = std::make_unique_for_overwrite<SIMDData[]>(100);
```

**Связь с философией:** Это прямое продолжение принципов DOD
из [03_dod-philosophy.md](../../philosophy/03_dod-philosophy.md). Данные должны лежать так, чтобы минимизировать cache
misses и конфликты между потоками.

---

## 6. Small Buffer Optimization (SBO)

> **Для понимания:** `std::string` и `std::function` — это не просто указатели. Внутри них есть небольшой буфер (обычно
> 15-23 байта для string, 16-32 байта для function). Если данные влезают в этот буфер — аллокации кучи не происходит.
> Это
> называется Small Buffer Optimization (SBO). Но если данные больше — происходит дорогая аллокация на куче.

### std::function и SBO

```cpp
#include <functional>

// Лямбда без захвата — влезает в SBO
auto lambda1 = []() { return 42; };
std::function<int()> func1 = lambda1;  // SBO, нет аллокации

// Лямбда с большим захватом — НЕ влезает в SBO
std::array<int, 100> bigData;
auto lambda2 = [bigData]() { return bigData[0]; };
std::function<int()> func2 = lambda2;  // АЛЛОКАЦИЯ на куче!

// В hot path это убийца производительности
void processVoxels(std::function<void(Voxel&)> processor);  // ПЛОХО!

// Лучше использовать шаблон:
template<typename Func>
void processVoxels(Func&& processor);  // ХОРОШО: инлайн, нет аллокации
```

### Альтернативы std::function для hot paths

```cpp
// Вариант 1: Шаблон (бинарный раздутие, но быстро)
template<typename Func>
void forEachVoxel(Func&& func) {
    for (auto& voxel : voxels) {
        func(voxel);
    }
}

// Вариант 2: Функциональный указатель (нет SBO, но нет аллокации)
using VoxelProcessor = void(*)(Voxel&);
void forEachVoxel(VoxelProcessor func);

// Вариант 3: C++23 std::function_ref (нет аллокации, ссылка на callable)
#if __cpp_lib_function_ref >= 202306L
#include <functional>
void forEachVoxel(std::function_ref<void(Voxel&)> func);  // C++26
#endif
```

> **Метафора:** `std::function` — это "универсальный контейнер для функций". Он как рюкзак: можно положить что угодно.
> Но если рюкзак маленький (SBO), а груз большой — придётся нанять грузовик (аллокация кучи). Для горячих циклов лучше
> использовать шаблоны или указатели: они как руки — ничего не прячут, всё видно компилятору.

---

## 7. Примеры для ProjectV

### RAII для Vulkan объектов

```cpp
class VulkanBuffer {
public:
    VulkanBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage)
        : m_device(device) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = usage;
        vkCreateBuffer(device, &info, nullptr, &m_buffer);

        // Выделение памяти
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &m_memory);
        vkBindBufferMemory(device, m_buffer, m_memory, 0);
    }

    ~VulkanBuffer() {
        if (m_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_memory, nullptr);
        }
        if (m_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_buffer, nullptr);
        }
    }

    // Запрещаем копирование
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // Разрешаем перемещение
    VulkanBuffer(VulkanBuffer&& other) noexcept
        : m_device(other.m_device), m_buffer(other.m_buffer), m_memory(other.m_memory) {
        other.m_buffer = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
    }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
};
```

### Умные указатели в ECS

```cpp
// Компонент с shared_ptr на текстуру
struct MeshRenderer {
    std::shared_ptr<Texture> texture;
    std::shared_ptr<Material> material;
    std::unique_ptr<VertexBuffer> vertexBuffer; // unique_ptr для эксклюзивного владения
};

// Система рендеринга
void renderSystem(flecs::iter& it, const Transform* transforms,
                  const MeshRenderer* renderers) {
    for (auto i : it) {
        if (renderers[i].texture) {
            // Используем shared_ptr без копирования
            bindTexture(*renderers[i].texture);
        }

        if (renderers[i].vertexBuffer) {
            // Используем unique_ptr (передача владения не нужна)
            bindVertexBuffer(*renderers[i].vertexBuffer);
        }
    }
}
```

### Move semantics для воксельных данных

```cpp
class VoxelChunk {
    std::vector<uint8_t> m_types;
    std::vector<glm::vec3> m_positions;

public:
    // Move constructor
    VoxelChunk(VoxelChunk&& other) noexcept
        : m_types(std::move(other.m_types)),
          m_positions(std::move(other.m_positions)) {
    }

    // Move assignment
    VoxelChunk& operator=(VoxelChunk&& other) noexcept {
        if (this != &other) {
            m_types = std::move(other.m_types);
            m_positions = std::move(other.m_positions);
        }
        return *this;
    }

    // Фабричный метод с возвратом по значению (NRVO/move)
    static VoxelChunk generateChunk(const glm::ivec3& position) {
        VoxelChunk chunk;
        // Генерация данных...
        return chunk; // Компилятор использует NRVO или move
    }
};
```

---

## 8. Распространенные ошибки и решения

### Ошибка: Утечка памяти Vulkan объектов

**Проблема:** Vulkan объекты создаются, но не уничтожаются.
**Решение:** Всегда используйте RAII обертки. Никогда не храните `VkBuffer`, `VkImage` как сырые указатели.

### Ошибка: Циклические ссылки с shared_ptr

**Проблема:** Два объекта держат друг друга через shared_ptr, счетчик никогда не становится 0.
**Решение:** Используйте weak_ptr для одной из ссылок или перепроектируйте архитектуру.

### Ошибка: Неправильное использование unique_ptr

**Проблема:** Попытка скопировать unique_ptr.
**Решение:** unique_ptr нельзя копировать, только перемещать. Используйте `std::move()` для передачи владения.

### Ошибка: Фрагментация памяти

**Проблема:** Много мелких аллокаций на куче.
**Решение:** Используйте memory pools или arena allocators для часто создаваемых объектов.

### Ошибка: Stack overflow

**Проблема:** Слишком большие объекты на стеке.
**Решение:** Большие данные (текстуры, меши) размещайте в куче через умные указатели.

### Ошибка: False Sharing в многопоточном коде

**Проблема:** Потоки работают с разными данными, но производительность низкая.
**Решение:** Выравнивайте данные разных потоков на границу cache line (64 байта) через `alignas(64)`.

### Ошибка: std::function в hot path

**Проблема:** Аллокации кучи в горячем цикле из-за SBO overflow.
**Решение:** Используйте шаблоны или указатели вместо `std::function` в критичных по производительности местах.
