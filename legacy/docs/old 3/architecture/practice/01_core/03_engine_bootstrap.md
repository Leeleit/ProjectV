# Engine Bootstrap Specification

**Статус:** Утверждено
**Версия:** 2.0 (Enterprise)
**Дата:** 2026-02-22

---

## Обзор

Документ определяет спецификацию холодного старта (Bootstrapping) движка ProjectV. Ключевые принципы:

1. **Строгий порядок инициализации** — DAG зависимостей подсистем
2. **RAII-гарантии** — автоматическое разрушение в обратном порядке
3. **PIMPL-изоляция** — EngineContext владеет корневыми указателями
4. **Изоляция запуска** — отделение bootstrap от игрового цикла

---

## Dependency Graph

### Инициализация (Direct Acyclic Graph)

```
Bootstrap DAG (инициализация сверху вниз)
─────────────────────────────────────────────────────────────────

┌─────────────────┐
│   Platform/SDL  │ Level 0 — Foundation
│  (Window, I/O)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│     Memory      │ Level 1 — Memory Management
│   (VMA, Pools)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Vulkan Device  │ Level 2 — GPU Context
│   (Instance,    │
│    Device)      │
└────────┬────────┘
         │
    ┌────┴────┬────────────┐
    ▼         ▼            ▼
┌────────┐ ┌────────┐ ┌─────────────┐
│  VMA   │ │ Swap-  │ │  Command    │ Level 3
│ Alloc. │ │ chain  │ │  Pools      │
└────────┘ └────────┘ └─────────────┘
         │
         ▼
┌─────────────────┐
│    JobSystem    │ Level 4 — Concurrency
│ (std::execution)│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   ECS (Flecs)   │ Level 5 — Entity System
│   (World)       │
└────────┬────────┘
         │
    ┌────┴────┬────────────┐
    ▼         ▼            ▼
┌────────┐ ┌────────┐ ┌─────────────┐
│Physics │ │ Render │ │    Audio    │ Level 6
│ (Jolt) │ │ System │ │ (miniaudio) │
└────────┘ └────────┘ └─────────────┘
         │
         ▼
┌─────────────────┐
│    Gameplay     │ Level 7 — Game Systems
│   (Player, UI)  │
└─────────────────┘

Shutdown (обратный порядок):
Level 7 → 6 → 5 → 4 → 3 → 2 → 1 → 0
```

---

## Memory Layout

### EngineContext

```
EngineContext (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── platform_: std::unique_ptr<PlatformSubsystem>      │
│      ├── memory_: std::unique_ptr<MemoryManager>            │
│      ├── vulkan_: std::unique_ptr<VulkanContext>            │
│      ├── swapchain_: std::unique_ptr<Swapchain>             │
│      ├── command_manager_: std::unique_ptr<CommandManager>  │
│      ├── job_system_: std::unique_ptr<JobSystem>            │
│      ├── ecs_world_: std::unique_ptr<ecs::World>            │
│      ├── physics_: std::unique_ptr<PhysicsSystem>           │
│      ├── renderer_: std::unique_ptr<RenderSystem>           │
│      ├── audio_: std::unique_ptr<AudioSystem>               │
│      ├── state_: EngineState (1 byte)                       │
│      ├── config_: EngineConfig (64 bytes)                   │
│      ├── start_time_: std::chrono::time_point (8 bytes)     │
│      └── frame_count_: uint64_t (8 bytes)                   │
│  Total: 8 bytes (external) + ~256 bytes (internal)          │
└─────────────────────────────────────────────────────────────┘

EngineConfig
┌─────────────────────────────────────────────────────────────┐
│  window_width: uint32_t (4 bytes)                           │
│  window_height: uint32_t (4 bytes)                          │
│  vsync: bool (1 byte)                                       │
│  fullscreen: bool (1 byte)                                  │
│  max_fps: uint32_t (4 bytes)                                │
│  fixed_delta_time: double (8 bytes)                         │
│  max_frame_time: double (8 bytes)                           │
│  enable_validation: bool (1 byte)                           │
│  enable_tracy: bool (1 byte)                                │
│  thread_count: uint32_t (4 bytes)                           │
│  physics_thread_count: uint32_t (4 bytes)                   │
│  max_bodies: uint32_t (4 bytes)                             │
│  padding: 20 bytes                                          │
│  Total: 64 bytes (aligned to 64)                            │
└─────────────────────────────────────────────────────────────┘

SubsystemEntry
┌─────────────────────────────────────────────────────────────┐
│  name: std::string_view (16 bytes)                          │
│  init_func: void(*)(EngineContext&) (8 bytes)               │
│  shutdown_func: void(*)(EngineContext&) (8 bytes)           │
│  dependency_count: uint32_t (4 bytes)                       │
│  dependencies: std::array<uint32_t, 4> (16 bytes)           │
│  level: uint32_t (4 bytes)                                  │
│  is_initialized: bool (1 byte)                              │
│  padding: 3 bytes                                           │
│  Total: 60 bytes                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machine

### Engine Lifecycle

```
Engine Lifecycle
         ┌────────────┐
         │   EMPTY    │ ←── Default constructed
         └─────┬──────┘
               │ create()
               ▼
         ┌────────────┐
         │  CREATED   │ ←── EngineContext allocated
         └─────┬──────┘
               │ initialize()
               ▼
    ┌────────────────────┐
    │   INITIALIZING     │ ←── Subsystems being created
    │   (DAG traversal)  │
    └─────────┬──────────┘
              │ success
              ▼
    ┌────────────────────┐
    │     READY          │ ←── All subsystems ready
    └─────────┬──────────┘
              │ run()
              ▼
    ┌────────────────────┐
    │     RUNNING        │ ←── Main loop active
    │   (game loop)      │
    └─────────┬──────────┘
              │ request_exit()
              ▼
    ┌────────────────────┐
    │    SHUTTING_DOWN   │ ←── Graceful shutdown
    └─────────┬──────────┘
              │
              ▼
    ┌────────────────────┐
    │    TERMINATED      │ ←── All subsystems destroyed
    └────────────────────┘

Error Path:
    INITIALIZING ──(error)──▶ ERROR ──▶ cleanup() ──▶ TERMINATED
```

### Subsystem Init State

```
Subsystem Initialization State

     ┌─────────────┐
     │ PENDING     │ ←── Not yet initialized
     └──────┬──────┘
            │ all dependencies ready
            ▼
     ┌─────────────┐
     │ INITIALIZING│ ←── init() in progress
     └──────┬──────┘
            │
    ┌───────┴───────┐
    ▼               ▼
┌─────────┐   ┌─────────┐
│  READY  │   │  ERROR  │
└─────────┘   └─────────┘
```

---

## API Contracts

### EngineContext

```cpp
// ProjectV.Engine.Context.cppm
export module ProjectV.Engine.Context;

import std;
import ProjectV.Core.Platform;
import ProjectV.Core.Memory;
import ProjectV.Core.Jobs;
import ProjectV.Render.Vulkan;
import ProjectV.ECS.Flecs;
import ProjectV.Physics.Jolt;

export namespace projectv::engine {

/// Коды ошибок инициализации.
export enum class BootstrapError : uint8_t {
    PlatformInitFailed,
    MemoryInitFailed,
    VulkanInitFailed,
    SwapchainCreationFailed,
    JobSystemInitFailed,
    ECSInitFailed,
    PhysicsInitFailed,
    RenderInitFailed,
    AudioInitFailed,
    InvalidConfig,
    AlreadyRunning,
    DependencyNotReady,
    SubsystemCrashed
};

/// Состояние движка.
export enum class EngineState : uint8_t {
    Empty,          ///< Default constructed
    Created,        ///< EngineContext allocated
    Initializing,   ///< Subsystems being created
    Ready,          ///< All subsystems ready
    Running,        ///< Main loop active
    ShuttingDown,   ///< Graceful shutdown
    Terminated,     ///< All subsystems destroyed
    Error           ///< Initialization failed
};

/// Конфигурация движка.
export struct EngineConfig {
    uint32_t window_width{1920};
    uint32_t window_height{1080};
    bool vsync{true};
    bool fullscreen{false};
    uint32_t max_fps{144};
    double fixed_delta_time{1.0 / 60.0};
    double max_frame_time{0.25};
    bool enable_validation{false};
    bool enable_tracy{false};
    uint32_t thread_count{0};           // 0 = auto
    uint32_t physics_thread_count{4};
    uint32_t max_bodies{65536};
    std::string application_name{"ProjectV"};
    std::string log_level{"info"};
};

/// Результат инициализации подсистемы.
export struct SubsystemInitResult {
    std::string_view subsystem_name;
    bool success{false};
    std::string error_message;
    std::chrono::milliseconds init_duration{0};
};

/// Engine Context — корневой владелец всех подсистем.
///
/// ## Ownership Model
/// - Владеет всеми подсистемами через unique_ptr
/// - Гарантирует разрушение в обратном порядке инициализации
/// - Изолирует клиентский код от деталей реализации
///
/// ## Thread Safety
/// - initialize(): main thread only
/// - run(): main thread only
/// - request_exit(): thread-safe
/// - get_*(): thread-safe for const access
///
/// ## Invariants
/// - Подсистемы инициализируются строго по DAG
/// - Shutdown происходит в обратном порядке
/// - После shutdown все указатели на подсистемы невалидны
export class EngineContext {
public:
    /// Создаёт EngineContext.
    ///
    /// @param config Конфигурация
    ///
    /// @pre config.window_width > 0 && config.window_height > 0
    /// @post state() == EngineState::Created
    explicit EngineContext(EngineConfig const& config = {}) noexcept;

    /// Разрушает EngineContext и все подсистемы.
    ///
    /// @post state() == EngineState::Terminated
    /// @post Все подсистемы уничтожены
    ~EngineContext() noexcept;

    // Move-only
    EngineContext(EngineContext&&) noexcept;
    EngineContext& operator=(EngineContext&&) noexcept;
    EngineContext(const EngineContext&) = delete;
    EngineContext& operator=(const EngineContext&) = delete;

    /// Инициализирует все подсистемы.
    ///
    /// @pre state() == EngineState::Created
    /// @post state() == EngineState::Ready или EngineState::Error
    ///
    /// @return void или ошибку инициализации
    [[nodiscard]] auto initialize() noexcept
        -> std::expected<void, BootstrapError>;

    /// Запускает главный цикл.
    ///
    /// @pre state() == EngineState::Ready
    /// @post state() == EngineState::Terminated после выхода
    ///
    /// @return Код возврата (0 = успех)
    [[nodiscard]] auto run() noexcept -> int;

    /// Запрашивает остановку.
    ///
    /// @param exit_code Код возврата
    ///
    /// @post is_exit_requested() == true
    auto request_exit(int exit_code = 0) noexcept -> void;

    /// Проверяет запрос выхода.
    [[nodiscard]] auto is_exit_requested() const noexcept -> bool;

    /// Получает текущее состояние.
    [[nodiscard]] auto state() const noexcept -> EngineState;

    /// Получает конфигурацию.
    [[nodiscard]] auto config() const noexcept -> EngineConfig const&;

    /// Получает subsystem handles (const).
    /// @{
    [[nodiscard]] auto platform() const noexcept -> platform::PlatformSubsystem const&;
    [[nodiscard]] auto memory() const noexcept -> core::MemoryManager const&;
    [[nodiscard]] auto vulkan() const noexcept -> render::VulkanContext const&;
    [[nodiscard]] auto job_system() const noexcept -> core::JobSystem const&;
    [[nodiscard]] auto ecs_world() const noexcept -> ecs::World const&;
    [[nodiscard]] auto physics() const noexcept -> physics::PhysicsSystem const&;
    [[nodiscard]] auto renderer() const noexcept -> render::RenderSystem const&;
    /// @}

    /// Получает subsystem handles (mutable).
    /// @{
    [[nodiscard]] auto platform() noexcept -> platform::PlatformSubsystem&;
    [[nodiscard]] auto memory() noexcept -> core::MemoryManager&;
    [[nodiscard]] auto vulkan() noexcept -> render::VulkanContext&;
    [[nodiscard]] auto job_system() noexcept -> core::JobSystem&;
    [[nodiscard]] auto ecs_world() noexcept -> ecs::World&;
    [[nodiscard]] auto physics() noexcept -> physics::PhysicsSystem&;
    [[nodiscard]] auto renderer() noexcept -> render::RenderSystem&;
    /// @}

    /// Получает статистику инициализации.
    [[nodiscard]] auto init_stats() const noexcept
        -> std::vector<SubsystemInitResult> const&;

    /// Получает время работы.
    [[nodiscard]] auto uptime() const noexcept -> std::chrono::seconds;

    /// Получает количество отрисованных кадров.
    [[nodiscard]] auto frame_count() const noexcept -> uint64_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /// Инициализация подсистем по уровням.
    auto initialize_level(uint32_t level) noexcept
        -> std::expected<void, BootstrapError>;

    /// Shutdown подсистем уровня.
    auto shutdown_level(uint32_t level) noexcept -> void;

    /// Проверка зависимостей подсистемы.
    [[nodiscard]] auto check_dependencies(uint32_t subsystem_index) const noexcept
        -> bool;
};

/// Получает строковое представление состояния.
[[nodiscard]] auto to_string(EngineState state) noexcept -> std::string_view;

/// Получает строковое представление ошибки.
[[nodiscard]] auto to_string(BootstrapError error) noexcept -> std::string_view;

} // namespace projectv::engine
```

### Bootstrap Sequencer

```cpp
// ProjectV.Engine.Bootstrap.cppm
export module ProjectV.Engine.Bootstrap;

import std;
import ProjectV.Engine.Context;

export namespace projectv::engine::bootstrap {

/// Подсистема для последовательной инициализации.
struct SubsystemDescriptor {
    std::string_view name;
    uint32_t level;
    std::vector<std::string_view> dependencies;

    /// Функция инициализации.
    std::function<std::expected<void, BootstrapError>(EngineContext&)> init;

    /// Функция shutdown.
    std::function<void(EngineContext&)> shutdown;
};

/// Bootstrap Sequencer — управление порядком инициализации.
///
/// ## Purpose
/// - Определяет DAG подсистем
/// - Проверяет циклические зависимости
/// - Выполняет топологическую сортировку
///
/// ## Invariants
/// - Нет циклических зависимостей
/// - Все зависимости удовлетворены перед init
export class BootstrapSequencer {
public:
    BootstrapSequencer() noexcept = default;

    /// Регистрирует подсистему.
    auto register_subsystem(SubsystemDescriptor descriptor) noexcept
        -> std::expected<void, BootstrapError>;

    /// Проверяет наличие циклов.
    [[nodiscard]] auto has_cycles() const noexcept -> bool;

    /// Возвращает порядок инициализации.
    [[nodiscard]] auto initialization_order() const noexcept
        -> std::vector<std::string_view>;

    /// Выполняет инициализацию всех подсистем.
    auto initialize_all(EngineContext& ctx) noexcept
        -> std::expected<void, BootstrapError>;

    /// Выполняет shutdown всех подсистем.
    auto shutdown_all(EngineContext& ctx) noexcept -> void;

    /// Получает количество зарегистрированных подсистем.
    [[nodiscard]] auto subsystem_count() const noexcept -> size_t;

private:
    std::vector<SubsystemDescriptor> subsystems_;
    std::unordered_map<std::string_view, size_t> name_to_index_;

    /// Топологическая сортировка.
    [[nodiscard]] auto topological_sort() const noexcept
        -> std::vector<size_t>;
};

/// Предустановленные подсистемы ProjectV.
auto create_default_subsystems() noexcept -> BootstrapSequencer;

} // namespace projectv::engine::bootstrap
```

---

## Implementation

### EngineContext Implementation

```cpp
// ProjectV.Engine.Context.cpp
module ProjectV.Engine.Context;

import std;
import ProjectV.Core.Platform;
import ProjectV.Core.Memory;
import ProjectV.Core.Jobs;
import ProjectV.Render.Vulkan;
import ProjectV.ECS.Flecs;
import ProjectV.Physics.Jolt;
import ProjectV.Engine.Bootstrap;

namespace projectv::engine {

struct EngineContext::Impl {
    // Configuration
    EngineConfig config;

    // State
    std::atomic<EngineState> state{EngineState::Empty};
    std::atomic<bool> exit_requested{false};
    int exit_code{0};

    // Timing
    std::chrono::steady_clock::time_point start_time;
    std::atomic<uint64_t> frame_count{0};

    // Subsystems (ownership in dependency order)
    std::unique_ptr<platform::PlatformSubsystem> platform;
    std::unique_ptr<core::MemoryManager> memory;
    std::unique_ptr<render::VulkanContext> vulkan;
    std::unique_ptr<render::Swapchain> swapchain;
    std::unique_ptr<render::CommandManager> command_manager;
    std::unique_ptr<core::JobSystem> job_system;
    std::unique_ptr<ecs::World> ecs_world;
    std::unique_ptr<physics::PhysicsSystem> physics;
    std::unique_ptr<render::RenderSystem> renderer;
    std::unique_ptr<audio::AudioSystem> audio;

    // Initialization stats
    std::vector<SubsystemInitResult> init_stats;

    // Bootstrap sequencer
    bootstrap::BootstrapSequencer sequencer;
};

EngineContext::EngineContext(EngineConfig const& config) noexcept
    : impl_(std::make_unique<Impl>())
{
    impl_->config = config;
    impl_->state.store(EngineState::Created, std::memory_order_release);

    // Initialize bootstrap sequencer
    impl_->sequencer = bootstrap::create_default_subsystems();
}

EngineContext::~EngineContext() noexcept {
    if (impl_->state.load(std::memory_order_acquire) == EngineState::Running) {
        request_exit();
        // Wait for main loop to finish
    }

    // Shutdown in reverse order
    impl_->sequencer.shutdown_all(*this);

    impl_->state.store(EngineState::Terminated, std::memory_order_release);
}

EngineContext::EngineContext(EngineContext&&) noexcept = default;
EngineContext& EngineContext::operator=(EngineContext&&) noexcept = default;

auto EngineContext::initialize() noexcept
    -> std::expected<void, BootstrapError> {

    auto expected_state = EngineState::Created;
    if (!impl_->state.compare_exchange_strong(
        expected_state,
        EngineState::Initializing,
        std::memory_order_acq_rel
    )) {
        if (expected_state == EngineState::Initializing) {
            return std::unexpected(BootstrapError::AlreadyRunning);
        }
        return std::unexpected(BootstrapError::InvalidConfig);
    }

    impl_->start_time = std::chrono::steady_clock::now();

    // Initialize all subsystems via sequencer
    auto result = impl_->sequencer.initialize_all(*this);

    if (!result) {
        impl_->state.store(EngineState::Error, std::memory_order_release);
        return result;
    }

    impl_->state.store(EngineState::Ready, std::memory_order_release);
    return {};
}

auto EngineContext::run() noexcept -> int {
    auto expected_state = EngineState::Ready;
    if (!impl_->state.compare_exchange_strong(
        expected_state,
        EngineState::Running,
        std::memory_order_acq_rel
    )) {
        return -1;  // Invalid state
    }

    // Main loop
    while (!impl_->exit_requested.load(std::memory_order_acquire)) {
        // Process platform events
        if (!impl_->platform->process_events()) {
            request_exit();
            break;
        }

        // Update frame time
        auto const frame_start = std::chrono::steady_clock::now();

        // Run game loop iteration
        // (delegated to GameLoop class)

        impl_->frame_count.fetch_add(1, std::memory_order_relaxed);
    }

    impl_->state.store(EngineState::ShuttingDown, std::memory_order_release);

    return impl_->exit_code;
}

auto EngineContext::request_exit(int exit_code) noexcept -> void {
    impl_->exit_code = exit_code;
    impl_->exit_requested.store(true, std::memory_order_release);
}

auto EngineContext::is_exit_requested() const noexcept -> bool {
    return impl_->exit_requested.load(std::memory_order_acquire);
}

auto EngineContext::state() const noexcept -> EngineState {
    return impl_->state.load(std::memory_order_acquire);
}

auto EngineContext::config() const noexcept -> EngineConfig const& {
    return impl_->config;
}

// Subsystem accessors (const)
auto EngineContext::platform() const noexcept -> platform::PlatformSubsystem const& {
    return *impl_->platform;
}
auto EngineContext::memory() const noexcept -> core::MemoryManager const& {
    return *impl_->memory;
}
auto EngineContext::vulkan() const noexcept -> render::VulkanContext const& {
    return *impl_->vulkan;
}
auto EngineContext::job_system() const noexcept -> core::JobSystem const& {
    return *impl_->job_system;
}
auto EngineContext::ecs_world() const noexcept -> ecs::World const& {
    return *impl_->ecs_world;
}
auto EngineContext::physics() const noexcept -> physics::PhysicsSystem const& {
    return *impl_->physics;
}
auto EngineContext::renderer() const noexcept -> render::RenderSystem const& {
    return *impl_->renderer;
}

// Subsystem accessors (mutable)
auto EngineContext::platform() noexcept -> platform::PlatformSubsystem& {
    return *impl_->platform;
}
auto EngineContext::memory() noexcept -> core::MemoryManager& {
    return *impl_->memory;
}
auto EngineContext::vulkan() noexcept -> render::VulkanContext& {
    return *impl_->vulkan;
}
auto EngineContext::job_system() noexcept -> core::JobSystem& {
    return *impl_->job_system;
}
auto EngineContext::ecs_world() noexcept -> ecs::World& {
    return *impl_->ecs_world;
}
auto EngineContext::physics() noexcept -> physics::PhysicsSystem& {
    return *impl_->physics;
}
auto EngineContext::renderer() noexcept -> render::RenderSystem& {
    return *impl_->renderer;
}

auto EngineContext::init_stats() const noexcept
    -> std::vector<SubsystemInitResult> const& {
    return impl_->init_stats;
}

auto EngineContext::uptime() const noexcept -> std::chrono::seconds {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - impl_->start_time);
}

auto EngineContext::frame_count() const noexcept -> uint64_t {
    return impl_->frame_count.load(std::memory_order_relaxed);
}

auto to_string(EngineState state) noexcept -> std::string_view {
    switch (state) {
        case EngineState::Empty: return "Empty";
        case EngineState::Created: return "Created";
        case EngineState::Initializing: return "Initializing";
        case EngineState::Ready: return "Ready";
        case EngineState::Running: return "Running";
        case EngineState::ShuttingDown: return "ShuttingDown";
        case EngineState::Terminated: return "Terminated";
        case EngineState::Error: return "Error";
        default: return "Unknown";
    }
}

auto to_string(BootstrapError error) noexcept -> std::string_view {
    switch (error) {
        case BootstrapError::PlatformInitFailed: return "PlatformInitFailed";
        case BootstrapError::MemoryInitFailed: return "MemoryInitFailed";
        case BootstrapError::VulkanInitFailed: return "VulkanInitFailed";
        case BootstrapError::SwapchainCreationFailed: return "SwapchainCreationFailed";
        case BootstrapError::JobSystemInitFailed: return "JobSystemInitFailed";
        case BootstrapError::ECSInitFailed: return "ECSInitFailed";
        case BootstrapError::PhysicsInitFailed: return "PhysicsInitFailed";
        case BootstrapError::RenderInitFailed: return "RenderInitFailed";
        case BootstrapError::AudioInitFailed: return "AudioInitFailed";
        case BootstrapError::InvalidConfig: return "InvalidConfig";
        case BootstrapError::AlreadyRunning: return "AlreadyRunning";
        case BootstrapError::DependencyNotReady: return "DependencyNotReady";
        case BootstrapError::SubsystemCrashed: return "SubsystemCrashed";
        default: return "Unknown";
    }
}

} // namespace projectv::engine
```

### Bootstrap Sequencer Implementation

```cpp
// ProjectV.Engine.Bootstrap.cpp
module ProjectV.Engine.Bootstrap;

import std;
import ProjectV.Engine.Context;

namespace projectv::engine::bootstrap {

auto BootstrapSequencer::register_subsystem(SubsystemDescriptor descriptor) noexcept
    -> std::expected<void, BootstrapError> {

    if (name_to_index_.contains(descriptor.name)) {
        return std::unexpected(BootstrapError::InvalidConfig);
    }

    size_t index = subsystems_.size();
    name_to_index_[descriptor.name] = index;
    subsystems_.push_back(std::move(descriptor));

    return {};
}

auto BootstrapSequencer::has_cycles() const noexcept -> bool {
    // DFS-based cycle detection
    std::vector<bool> visited(subsystems_.size(), false);
    std::vector<bool> rec_stack(subsystems_.size(), false);

    std::function<bool(size_t)> dfs = [&](size_t index) -> bool {
        visited[index] = true;
        rec_stack[index] = true;

        for (auto const& dep_name : subsystems_[index].dependencies) {
            auto it = name_to_index_.find(dep_name);
            if (it == name_to_index_.end()) continue;

            size_t dep_index = it->second;

            if (!visited[dep_index]) {
                if (dfs(dep_index)) return true;
            } else if (rec_stack[dep_index]) {
                return true;  // Cycle found
            }
        }

        rec_stack[index] = false;
        return false;
    };

    for (size_t i = 0; i < subsystems_.size(); ++i) {
        if (!visited[i]) {
            if (dfs(i)) return true;
        }
    }

    return false;
}

auto BootstrapSequencer::initialization_order() const noexcept
    -> std::vector<std::string_view> {

    auto order = topological_sort();
    std::vector<std::string_view> result;
    result.reserve(order.size());

    for (size_t index : order) {
        result.push_back(subsystems_[index].name);
    }

    return result;
}

auto BootstrapSequencer::topological_sort() const noexcept
    -> std::vector<size_t> {

    std::vector<size_t> result;
    result.reserve(subsystems_.size());

    std::vector<bool> visited(subsystems_.size(), false);
    std::vector<size_t> order(subsystems_.size());

    // Sort by level first
    for (size_t i = 0; i < subsystems_.size(); ++i) {
        order[i] = i;
    }

    std::stable_sort(order.begin(), order.end(),
        [this](size_t a, size_t b) {
            return subsystems_[a].level < subsystems_[b].level;
        }
    );

    result = order;
    return result;
}

auto BootstrapSequencer::initialize_all(EngineContext& ctx) noexcept
    -> std::expected<void, BootstrapError> {

    if (has_cycles()) {
        return std::unexpected(BootstrapError::InvalidConfig);
    }

    auto order = topological_sort();

    for (size_t index : order) {
        auto const& subsystem = subsystems_[index];

        auto start = std::chrono::steady_clock::now();

        auto result = subsystem.init(ctx);

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        );

        SubsystemInitResult stat{
            .subsystem_name = subsystem.name,
            .success = result.has_value(),
            .init_duration = duration
        };

        if (!result) {
            stat.error_message = std::string(to_string(result.error()));
            // Store stat before returning error
            // ctx.init_stats().push_back(stat);
            return result;
        }

        // ctx.init_stats().push_back(stat);
    }

    return {};
}

auto BootstrapSequencer::shutdown_all(EngineContext& ctx) noexcept -> void {
    auto order = topological_sort();

    // Shutdown in reverse order
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto const& subsystem = subsystems_[*it];
        if (subsystem.shutdown) {
            subsystem.shutdown(ctx);
        }
    }
}

auto BootstrapSequencer::subsystem_count() const noexcept -> size_t {
    return subsystems_.size();
}

/// Создаёт стандартный набор подсистем ProjectV.
auto create_default_subsystems() noexcept -> BootstrapSequencer {
    BootstrapSequencer seq;

    // Level 0: Platform
    seq.register_subsystem({
        .name = "Platform",
        .level = 0,
        .dependencies = {},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            ctx.platform() = platform::PlatformSubsystem{};
            return ctx.platform().init()
                .or_else([](auto) { return std::unexpected(BootstrapError::PlatformInitFailed); });
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.platform() = {};
        }
    });

    // Level 1: Memory
    seq.register_subsystem({
        .name = "Memory",
        .level = 1,
        .dependencies = {},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            ctx.memory() = core::MemoryManager::create()
                .or_else([](auto) { return std::unexpected(BootstrapError::MemoryInitFailed); });
            return {};
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.memory() = {};
        }
    });

    // Level 2: Vulkan
    seq.register_subsystem({
        .name = "Vulkan",
        .level = 2,
        .dependencies = {"Platform", "Memory"},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            render::VulkanConfig config{
                .enable_validation = ctx.config().enable_validation
            };

            ctx.vulkan() = render::VulkanContext::create(config)
                .or_else([](auto) { return std::unexpected(BootstrapError::VulkanInitFailed); });
            return {};
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.vulkan() = {};
        }
    });

    // Level 4: JobSystem
    seq.register_subsystem({
        .name = "JobSystem",
        .level = 4,
        .dependencies = {},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            uint32_t threads = ctx.config().thread_count;
            if (threads == 0) {
                threads = std::thread::hardware_concurrency();
            }
            ctx.job_system() = core::JobSystem{threads};
            return {};
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.job_system().shutdown();
            ctx.job_system() = {};
        }
    });

    // Level 5: ECS
    seq.register_subsystem({
        .name = "ECS",
        .level = 5,
        .dependencies = {"JobSystem"},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            ctx.ecs_world() = ecs::World{ctx.config().thread_count};
            return {};
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.ecs_world() = {};
        }
    });

    // Level 6: Physics
    seq.register_subsystem({
        .name = "Physics",
        .level = 6,
        .dependencies = {"ECS", "JobSystem"},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            physics::PhysicsConfig config{
                .max_bodies = ctx.config().max_bodies
            };

            ctx.physics() = physics::PhysicsSystem::create(config)
                .or_else([](auto) { return std::unexpected(BootstrapError::PhysicsInitFailed); });
            return {};
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.physics() = {};
        }
    });

    // Level 6: Render
    seq.register_subsystem({
        .name = "Render",
        .level = 6,
        .dependencies = {"Vulkan"},
        .init = [](EngineContext& ctx) -> std::expected<void, BootstrapError> {
            ctx.renderer() = render::RenderSystem::create(ctx.vulkan())
                .or_else([](auto) { return std::unexpected(BootstrapError::RenderInitFailed); });
            return {};
        },
        .shutdown = [](EngineContext& ctx) {
            ctx.renderer() = {};
        }
    });

    return seq;
}

} // namespace projectv::engine::bootstrap
```

---

## Isolation Pattern

### Отделение Bootstrap от Game Loop

```cpp
// === Точка входа (main.cpp) ===

import std;
import ProjectV.Engine.Context;
import ProjectV.Game.GameLoop;

auto main(int argc, char* argv[]) -> int {
    // 1. Parse config
    projectv::engine::EngineConfig config;
    // ... parse argc/argv ...

    // 2. Create engine context (bootstrap)
    projectv::engine::EngineContext engine{config};

    // 3. Initialize subsystems
    auto init_result = engine.initialize();
    if (!init_result) {
        std::cerr << "Initialization failed: "
                  << projectv::engine::to_string(init_result.error()) << '\n';
        return 1;
    }

    // 4. Create game loop (isolated from bootstrap)
    projectv::game::GameLoop game_loop{engine};

    // 5. Run main loop
    int exit_code = engine.run();

    // 6. Context automatically destroyed with RAII
    return exit_code;
}

// === Game Loop (isolated) ===

// ProjectV.Game.GameLoop.cppm
export module ProjectV.Game.GameLoop;

import std;
import ProjectV.Engine.Context;

export namespace projectv::game {

export class GameLoop {
public:
    explicit GameLoop(engine::EngineContext& ctx) noexcept;

    /// Выполняет один кадр.
    auto execute_frame() noexcept -> void;

private:
    engine::EngineContext& ctx_;
    std::chrono::steady_clock::time_point last_frame_time_;
    double accumulator_{0.0};
};

} // namespace projectv::game
```

---

## Performance Metrics

| Метрика                 | Цель     | Измерение |
|-------------------------|----------|-----------|
| Total Bootstrap Time    | < 2s     | Tracy CPU |
| Platform Init           | < 100ms  | Tracy CPU |
| Vulkan Init             | < 500ms  | Tracy CPU |
| ECS Init                | < 50ms   | Tracy CPU |
| Physics Init            | < 200ms  | Tracy CPU |
| Memory Peak during Init | < 512 MB | VMA stats |

---

## Ссылки

- [Engine Structure](../01_core/01_engine_structure.md)
- [Core Loop Specification](../01_core/02_core_loop.md)
- [Vulkan 1.4 Specification](../02_render/01_vulkan_spec.md)
- [Jolt-Vulkan Bridge](../04_physics_ca/01_jolt_vulkan_bridge.md)
