# Безопасность и Обработка ошибок (Error Handling)

**🟢🟡🔴 Уровни сложности:**

- 🟢 Уровень 1: Основы безопасности, `assert`, `std::optional`, избегание UB
- 🟡 Уровень 2: `std::expected`, контракты, санитайзеры, безопасные обёртки
- 🔴 Уровень 3: Система ошибок для Vulkan/ECS, мониторинг в реальном времени, восстановление состояния

В ProjectV мы стремимся к максимальной стабильности. Ошибки в игровом движке могут быть как "ожидаемыми" (файл не
найден), так и "критическими" (доступ по нулевому адресу). Правильная обработка ошибок критична для воксельного движка,
который работает с большими объёмами данных.

> **Связь с философией:** Обработка ошибок без исключений — это следствие нашего подхода к предсказуемости.
> См. [08_anti-patterns.md](../../philosophy/08_anti-patterns.md) — исключения в hot paths считаются анти-паттерном.

## Уровень 1: Основы безопасности и предотвращение UB

### Undefined Behavior (UB) — Самый опасный враг

В C++ есть действия, которые "не определены" стандартом. Компилятор может сделать все что угодно: от удаления кода до
форматирования диска.

**Как избегать в ProjectV:**

```cpp
// ❌ Опасный код
void processVoxel(Voxel* voxels, int count) {
    for (int i = 0; i <= count; i++) {  // UB: выход за пределы при i == count
        voxels[i].process();
    }
}

// ✅ Безопасный код
void processVoxel(std::span<Voxel> voxels) {  // C++20: безопасный доступ к массиву
    for (auto& voxel : voxels) {
        voxel.process();
    }
}

// Инициализация воксельных данных
class VoxelChunk {
    std::array<Voxel, CHUNK_VOLUME> voxels_{};  // {} инициализирует все элементы
public:
    VoxelChunk() {
        // Гарантированная инициализация
        std::fill(voxels_.begin(), voxels_.end(), Voxel{VoxelType::AIR, 0});
    }
};
```

### Основные правила безопасности:

1. **Используйте ссылки**: Они не могут быть `null`
2. **Используйте `std::span` (C++20)**: Передавайте массивы безопасно
3. **Инициализируйте всё**: Никогда не оставляйте переменную без значения
4. **Используйте умные указатели**: `std::unique_ptr`, `std::shared_ptr`

### Ассерты для отладки

```cpp
// Ассерты для проверки "невозможных" условий в Debug-версии
void setVoxel(VoxelChunk& chunk, int x, int y, int z, VoxelType type) {
    assert(x >= 0 && x < CHUNK_SIZE && "X coordinate out of bounds");
    assert(y >= 0 && y < CHUNK_SIZE && "Y coordinate out of bounds");
    assert(z >= 0 && z < CHUNK_SIZE && "Z coordinate out of bounds");

    chunk.setVoxel(x, y, z, type);
}

// Ассерт для проверки инвариантов ECS
void addComponent(flecs::entity entity, const Transform& transform) {
    assert(entity.is_alive() && "Cannot add component to dead entity");
    assert(!entity.has<Transform>() && "Entity already has Transform component");

    entity.set(transform);
}
```

## Уровень 2: Современная обработка ошибок

### Исключения в игровых движках

В большинстве игровых движков (ProjectV не исключение) исключения (`try-catch`) отключены в компиляторе.

**Причины:**

1. **Накладные расходы**: Каждая функция становится медленнее
2. **Скрытая логика**: Исключение может "выстрелить" в любой момент
3. **Сложность восстановления**: В реальном времени сложно откатить состояние

### `std::optional` для опциональных значений

```cpp
#include <optional>

// Загрузка ресурсов в ProjectV
std::optional<Texture> loadTexture(const std::string& name) {
    if (!std::filesystem::exists("assets/textures/" + name)) {
        return std::nullopt;  // Файл не найден
    }

    try {
        return Texture("assets/textures/" + name);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load texture " << name << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

// Использование с structured binding
if (auto texture = loadTexture("brick.png")) {
    texture->bind();  // Гарантированно валидный texture
} else {
    // Используем fallback текстуру
    loadTexture("default.png")->bind();
}
```

### `std::expected` для детализированных ошибок (C++23)

```cpp
#include <expected>
#include <system_error>

enum class VoxelError {
    OutOfBounds,
    InvalidType,
    ChunkNotLoaded,
    MemoryAllocationFailed
};

std::expected<Voxel, VoxelError> getVoxel(const VoxelChunk& chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return std::unexpected(VoxelError::OutOfBounds);
    }

    if (!chunk.isLoaded()) {
        return std::unexpected(VoxelError::ChunkNotLoaded);
    }

    return chunk.getVoxel(x, y, z);
}

// Использование с цепочкой операций
auto result = getVoxel(chunk, 10, 10, 10)
    .and_then( -> std::expected<RenderData, VoxelError> {
        if (v.type == VoxelType::AIR) {
            return std::unexpected(VoxelError::InvalidType);
        }
        return v.toRenderData();
    })
    .transform_error( {
        return "Voxel error: " + std::to_string(static_cast<int>(error));
    });
```

### Безопасные обёртки для Vulkan

```cpp
// Обёртка для Vulkan функций с проверкой результата
template<typename T>
class VulkanResult {
    T value_;
    VkResult result_;

public:
    VulkanResult(VkResult result, T value = {})
        : result_(result), value_(std::move(value)) {}

    bool success() const { return result_ == VK_SUCCESS; }
    T&& unwrap() && {
        if (!success()) {
            throw std::runtime_error("Vulkan operation failed");
        }
        return std::move(value_);
    }

    T value_or(T&& fallback) && {
        return success() ? std::move(value_) : std::move(fallback);
    }
};

// Использование
auto buffer = VulkanResult<VkBuffer>(
    vkCreateBuffer(device, &createInfo, nullptr, &buffer),
    buffer
).unwrap();  // Выбросит исключение при ошибке
```

## Уровень 3: Система ошибок для Vulkan/ECS

### Мониторинг ошибок в реальном времени

```cpp
// Система для мониторинга и восстановления ошибок
class ErrorMonitoringSystem {
    struct ErrorEvent {
        std::string message;
        std::source_location location;
        std::chrono::system_clock::time_point timestamp;
        ErrorSeverity severity;
    };

    std::vector<ErrorEvent> recentErrors_;
    std::atomic<bool> recoveryInProgress_{false};

public:
    void reportError(std::string message, ErrorSeverity severity,
                     std::source_location loc = std::source_location::current()) {
        ErrorEvent event{
            std::move(message),
            loc,
            std::chrono::system_clock::now(),
            severity
        };

        recentErrors_.push_back(std::move(event));

        if (severity == ErrorSeverity::Critical) {
            initiateRecovery();
        }
    }

    void initiateRecovery() {
        if (recoveryInProgress_.exchange(true)) {
            return;  // Восстановление уже идёт
        }

        // Сохраняем состояние для восстановления
        saveGameState();

        // Перезапускаем критические системы
        restartGraphicsSubsystem();
        restartPhysicsSubsystem();

        recoveryInProgress_ = false;
    }
};
```

### Обработка ошибок в ECS системах

```cpp
// Система с обработкой ошибок
class SafePhysicsSystem {
    ErrorMonitoringSystem& errorMonitor_;

public:
    void update(flecs::world& world, float dt) {
        world.each([this, dt](flecs::entity entity, Transform& transform, RigidBody& body) {
            try {
                // Попытка обновления физики
                updatePhysics(transform, body, dt);
            } catch (const std::exception& e) {
                // Отчёт об ошибке без краша
                errorMonitor_.reportError(
                    fmt::format("Physics update failed for entity {}: {}",
                               entity.name(), e.what()),
                    ErrorSeverity::Warning
                );

                // Безопасное восстановление
                body.velocity = glm::vec3(0);
                body.angularVelocity = glm::vec3(0);
            }
        });
    }
};
```

### Восстановление состояния Vulkan

```cpp
// Менеджер восстановления Vulkan ресурсов
class VulkanRecoveryManager {
    struct ResourceSnapshot {
        std::vector<VkBuffer> buffers;
        std::vector<VkImage> images;
        std::vector<VkPipeline> pipelines;
        // ... другие ресурсы
    };

    ResourceSnapshot lastGoodState_;
    VkDevice device_;

public:
    void saveState() {
        // Сохраняем текущее состояние всех ресурсов
        lastGoodState_ = captureCurrentState();
    }

    void recoverFromError(VkResult error) {
        // Уничтожаем повреждённые ресурсы
        cleanupFailedResources();

        // Восстанавливаем из snapshot
        restoreFromSnapshot(lastGoodState_);

        // Пересоздаём зависимости
        rebuildDescriptorSets();
        rebuildCommandBuffers();
    }

private:
    ResourceSnapshot captureCurrentState() {
        // Захват текущего состояния GPU ресурсов
        ResourceSnapshot snapshot;
        // ... реализация захвата
        return snapshot;
    }
};
```

## Для ProjectV

### Обработка ошибок воксельного движка

```cpp
// Специализированная система ошибок для вокселей
class VoxelErrorHandler {
public:
    enum class Error {
        ChunkOutOfMemory,
        MeshGenerationFailed,
        LightingCalculationOverflow,
        SaveFileCorrupted
    };

    std::expected<void, Error> loadChunk(VoxelChunk& chunk, const std::string& filename) {
        auto file = std::ifstream(filename, std::ios::binary);
        if (!file) {
            return std::unexpected(Error::SaveFileCorrupted);
        }

        try {
            chunk.loadFromStream(file);
        } catch (const std::bad_alloc&) {
            return std::unexpected(Error::ChunkOutOfMemory);
        } catch (const std::runtime_error&) {
            return std::unexpected(Error::MeshGenerationFailed);
        }

        return {};
    }

    void handleError(Error error, VoxelChunk& chunk) {
        switch (error) {
            case Error::ChunkOutOfMemory:
                chunk.clear();  // Освобождаем память
                chunk.setAll(VoxelType::AIR);  // Устанавливаем пустой чанк
                break;

            case Error::MeshGenerationFailed:
                chunk.markForRegeneration();  // Помечаем для перегенерации
                break;

            case Error::SaveFileCorrupted:
                chunk.generateProcedural();  // Генерируем процедурно
                break;
        }
    }
};
```

### Интеграция с Tracy для мониторинга

```cpp
// Мониторинг ошибок через Tracy
class TracyErrorMonitor {
public:
    void reportErrorWithContext(const std::string& error,
                                const std::source_location& loc) {
        ZoneScopedN("ErrorReporting");

        TracyMessage(error.c_str(), error.size());
        TracyPlot("ErrorCount", 1);

        // Логирование с контекстом
        TracyLog("Error at %s:%d: %s",
                 loc.file_name(), loc.line(), error.c_str());
    }

    void monitorVulkanCalls() {
        static TracyVkCtx ctx = TracyVkContext(device, queue, commandBuffer);

        TracyVkZone(ctx, "VulkanOperation");
        VkResult result = vkSomeOperation(...);

        if (result != VK_SUCCESS) {
            reportErrorWithContext(
                fmt::format("Vulkan error: {}", static_cast<int>(result)),
                std::source_location::current()
            );
        }
    }
};
```

## Распространённые ошибки и решения

1. **Игнорирование возвращаемых значений**

- ❌ `vkCreateBuffer(...);` без проверки результата
- ✅ Всегда проверяйте возвращаемые значения Vulkan функций

1. **Утечки памяти при ошибках**

- ❌ Выделение памяти без освобождения в случае ошибки
- ✅ Используйте RAII и умные указатели

1. **Отсутствие восстановления после ошибок**

- ❌ Игра крашится при первой же ошибке
- ✅ Реализуйте graceful degradation и восстановление состояния

1. **Слишком много ассертов в релизной сборке**

- ❌ `assert()` замедляет релизную версию
- ✅ Используйте `NDEBUG` для отключения ассертов в релизе

1. **Недостаточная информация об ошибках**

- ❌ "Ошибка загрузки текстуры"
- ✅ "Не удалось загрузить texture.png: файл не найден по пути assets/textures/"

1. **Глобальные обработчики ошибок**

- ❌ Глобальный `try-catch` скрывает источник ошибок
- ✅ Обрабатывайте ошибки локально, рядом с местом возникновения

## Быстрые ссылки по задачам

- **Проверить результат Vulkan операции**: Используйте `VulkanResult<T>` обёртку
- **Обработать опциональное значение**: Используйте `std::optional` с проверкой `has_value()`
- **Детализированная обработка ошибок**: Используйте `std::expected` с цепочками `and_then()`
- **Отладка UB и утечек памяти**: Включите AddressSanitizer и ThreadSanitizer
- **Мониторинг ошибок в реальном времени**: Интегрируйте с Tracy для профилирования
- **Восстановление после критических ошибок**: Реализуйте систему snapshot и восстановления

