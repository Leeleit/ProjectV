# SDL3 Integration Specification

**Статус:** Спецификация
**Версия SDL:** 3.x
**Дата:** 2026-02-22

---

## Обзор

Документ описывает инкапсуляцию SDL3 в архитектуре ProjectV. SDL3 используется для:

- Создания окна с Vulkan surface
- Обработки ввода (клавиатура, мышь, gamepad)
- Event loop интеграции с ECS

---

## 1. Module Interface

```cpp
// ProjectV.Core.Platform.SDL.cppm
module;

// Global Module Fragment — C headers
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_gamepad.h>

export module ProjectV.Core.Platform.SDL;

import std;
import glm;

export namespace projectv::platform {

// ============================================================================
// SDL Context (RAII Initializer)
// ============================================================================

/// SDL subsystem initialization flags
export enum class SDLSubsystem : uint32_t {
    Video     = SDL_INIT_VIDEO,
    Audio     = SDL_INIT_AUDIO,
    Gamepad   = SDL_INIT_GAMEPAD,
    Haptic    = SDL_INIT_HAPTIC,
    Events    = SDL_INIT_EVENTS,
    Everything = SDL_INIT_EVERYTHING
};

/// RAII wrapper for SDL initialization.
/// Must be created before any other SDL functions.
class SDLContext {
public:
    /// Initializes SDL with specified subsystems.
    [[nodiscard]] static auto init(
        SDLSubsystem flags = SDLSubsystem::Video
    ) noexcept -> std::expected<SDLContext, SDLError>;

    /// Shuts down SDL.
    ~SDLContext() noexcept;

    // Move-only
    SDLContext(SDLContext&&) noexcept;
    SDLContext& operator=(SDLContext&&) noexcept;
    SDLContext(const SDLContext&) = delete;
    SDLContext& operator=(const SDLContext&) = delete;

    [[nodiscard]] auto is_valid() const noexcept -> bool { return initialized_; }

private:
    SDLContext() = default;
    bool initialized_{false};
};

// ============================================================================
// Window (RAII Wrapper)
// ============================================================================

/// Window creation flags
export enum class WindowFlags : uint32_t {
    None         = 0,
    Fullscreen   = SDL_WINDOW_FULLSCREEN,
    Vulkan       = SDL_WINDOW_VULKAN,
    Resizable    = SDL_WINDOW_RESIZABLE,
    Borderless   = SDL_WINDOW_BORDERLESS,
    HighDPI      = SDL_WINDOW_HIGH_PIXEL_DENSITY,
    Hidden       = SDL_WINDOW_HIDDEN,
    Maximized    = SDL_WINDOW_MAXIMIZED,
    Minimized    = SDL_WINDOW_MINIMIZED
};

/// Window creation configuration
struct WindowConfig {
    std::string_view title{"ProjectV"};
    uint32_t width{1920};
    uint32_t height{1080};
    WindowFlags flags{WindowFlags::Vulkan | WindowFlags::Resizable | WindowFlags::HighDPI};
};

/// RAII wrapper for SDL_Window
class Window {
public:
    /// Creates a window with Vulkan support.
    [[nodiscard]] static auto create(
        WindowConfig const& config = {}
    ) noexcept -> std::expected<Window, WindowError>;

    /// Destroys window.
    ~Window() noexcept;

    // Move-only
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // --- Properties ---

    [[nodiscard]] auto size() const noexcept -> glm::uvec2;
    [[nodiscard]] auto drawable_size() const noexcept -> glm::uvec2;  // For HighDPI
    [[nodiscard]] auto position() const noexcept -> glm::ivec2;
    [[nodiscard]] auto id() const noexcept -> uint32_t;

    auto set_size(uint32_t width, uint32_t height) noexcept -> void;
    auto set_title(std::string_view title) noexcept -> void;
    auto set_fullscreen(bool fullscreen) noexcept -> void;
    auto show() noexcept -> void;
    auto hide() noexcept -> void;

    // --- Vulkan Integration ---

    /// Returns Vulkan instance extensions required by SDL.
    [[nodiscard]] auto vulkan_instance_extensions() const noexcept
        -> std::vector<const char*>;

    /// Creates Vulkan surface for this window.
    [[nodiscard]] auto create_vulkan_surface(
        VkInstance instance,
        VkAllocationCallbacks const* allocator = nullptr
    ) const noexcept -> std::expected<VkSurfaceKHR, SurfaceError>;

    // --- Native Access ---

    [[nodiscard]] auto native() const noexcept -> SDL_Window* { return window_; }
    [[nodiscard]] auto is_valid() const noexcept -> bool { return window_ != nullptr; }

private:
    explicit Window(SDL_Window* window) noexcept : window_(window) {}
    SDL_Window* window_{nullptr};
};

// ============================================================================
// Event Handling
// ============================================================================

/// Input event types
export enum class InputEventType : uint8_t {
    KeyDown,
    KeyUp,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel,
    GamepadAdded,
    GamepadRemoved,
    GamepadButton,
    GamepadAxis,
    WindowResize,
    WindowClose,
    Quit
};

/// Keyboard key codes
export enum class KeyCode : uint32_t {
    Unknown = SDLK_UNKNOWN,
    A = SDLK_A, B = SDLK_B, C = SDLK_C, D = SDLK_D, E = SDLK_E,
    F = SDLK_F, G = SDLK_G, H = SDLK_H, I = SDLK_I, J = SDLK_J,
    K = SDLK_K, L = SDLK_L, M = SDLK_M, N = SDLK_N, O = SDLK_O,
    P = SDLK_P, Q = SDLK_Q, R = SDLK_R, S = SDLK_S, T = SDLK_T,
    U = SDLK_U, V = SDLK_V, W = SDLK_W, X = SDLK_X, Y = SDLK_Y, Z = SDLK_Z,
    Space = SDLK_SPACE,
    Escape = SDLK_ESCAPE,
    Return = SDLK_RETURN,
    Tab = SDLK_TAB,
    Shift = SDLK_LSHIFT,
    Ctrl = SDLK_LCTRL,
    Alt = SDLK_LALT,
    F1 = SDLK_F1, F2 = SDLK_F2, F3 = SDLK_F3, F4 = SDLK_F4,
    F5 = SDLK_F5, F6 = SDLK_F6, F7 = SDLK_F7, F8 = SDLK_F8,
    F9 = SDLK_F9, F10 = SDLK_F10, F11 = SDLK_F11, F12 = SDLK_F12
};

/// Mouse button codes
export enum class MouseButton : uint8_t {
    Left = 1,
    Middle = 2,
    Right = 3,
    X1 = 4,
    X2 = 5
};

/// Input event data
struct InputEvent {
    InputEventType type;
    uint64_t timestamp_ns;

    // Keyboard
    KeyCode key_code{KeyCode::Unknown};
    bool key_repeat{false};
    uint16_t key_modifiers{0};

    // Mouse
    glm::vec2 mouse_position{0.0f, 0.0f};
    glm::vec2 mouse_relative{0.0f, 0.0f};
    glm::vec2 mouse_wheel{0.0f, 0.0f};
    MouseButton mouse_button{MouseButton::Left};

    // Gamepad
    uint32_t gamepad_id{0};
    uint8_t gamepad_button{0};
    float gamepad_axis_value{0.0f};

    // Window
    glm::uvec2 window_size{0, 0};
};

/// Event handler callback type
using EventHandler = std::move_only_function<bool(InputEvent const&)>;

/// Event system for polling and dispatching
class EventSystem {
public:
    EventSystem() = default;

    /// Sets handler for input events.
    /// Returns false from handler to stop further processing.
    auto set_handler(EventHandler handler) noexcept -> void;

    /// Polls all pending events and dispatches to handler.
    /// @returns false if quit requested
    [[nodiscard]] auto poll() noexcept -> bool;

    /// Gets current keyboard state.
    [[nodiscard]] auto is_key_down(KeyCode key) const noexcept -> bool;

    /// Gets current mouse state.
    [[nodiscard]] auto mouse_position() const noexcept -> glm::vec2;
    [[nodiscard]] auto mouse_buttons() const noexcept -> uint32_t;

private:
    EventHandler handler_;
    uint8_t const* keyboard_state_{nullptr};
    uint32_t mouse_button_state_{0};
    glm::vec2 mouse_position_{0.0f, 0.0f};
};

// ============================================================================
// Gamepad Support
// ============================================================================

/// Gamepad axis codes
export enum class GamepadAxis : uint8_t {
    LeftX = SDL_GAMEPAD_AXIS_LEFTX,
    LeftY = SDL_GAMEPAD_AXIS_LEFTY,
    RightX = SDL_GAMEPAD_AXIS_RIGHTX,
    RightY = SDL_GAMEPAD_AXIS_RIGHTY,
    LeftTrigger = SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    RightTrigger = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};

/// Gamepad button codes
export enum class GamepadButton : uint8_t {
    A = SDL_GAMEPAD_BUTTON_SOUTH,
    B = SDL_GAMEPAD_BUTTON_EAST,
    X = SDL_GAMEPAD_BUTTON_WEST,
    Y = SDL_GAMEPAD_BUTTON_NORTH,
    Back = SDL_GAMEPAD_BUTTON_BACK,
    Start = SDL_GAMEPAD_BUTTON_START,
    LeftStick = SDL_GAMEPAD_BUTTON_LEFT_STICK,
    RightStick = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    LeftShoulder = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    RightShoulder = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    DPadUp = SDL_GAMEPAD_BUTTON_DPAD_UP,
    DPadDown = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    DPadLeft = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    DPadRight = SDL_GAMEPAD_BUTTON_DPAD_RIGHT
};

/// RAII wrapper for SDL_Gamepad
class Gamepad {
public:
    /// Opens gamepad by device index.
    [[nodiscard]] static auto open(uint32_t device_index) noexcept
        -> std::expected<Gamepad, GamepadError>;

    /// Closes gamepad.
    ~Gamepad() noexcept;

    // Move-only
    Gamepad(Gamepad&&) noexcept;
    Gamepad& operator=(Gamepad&&) noexcept;
    Gamepad(const Gamepad&) = delete;
    Gamepad& operator=(const Gamepad&) = delete;

    /// Gets axis value (-1.0 to 1.0).
    [[nodiscard]] auto axis(GamepadAxis axis) const noexcept -> float;

    /// Checks if button is pressed.
    [[nodiscard]] auto button(GamepadButton button) const noexcept -> bool;

    /// Gets gamepad ID.
    [[nodiscard]] auto id() const noexcept -> uint32_t;

    /// Gets gamepad name.
    [[nodiscard]] auto name() const noexcept -> std::string_view;

    /// Rumble effect.
    auto rumble(float low_freq_intensity, float high_freq_intensity, uint32_t duration_ms) noexcept -> void;

    [[nodiscard]] auto native() const noexcept -> SDL_Gamepad* { return gamepad_; }

private:
    explicit Gamepad(SDL_Gamepad* gamepad) noexcept : gamepad_(gamepad) {}
    SDL_Gamepad* gamepad_{nullptr};
};

// ============================================================================
// Error Codes
// ============================================================================

export enum class SDLError : uint8_t {
    InitializationFailed,
    SubsystemNotAvailable,
    Unknown
};

export enum class WindowError : uint8_t {
    CreationFailed,
    InvalidFlags,
    VulkanNotSupported
};

export enum class SurfaceError : uint8_t {
    CreationFailed,
    InvalidInstance,
    WindowNotValid
};

export enum class GamepadError : uint8_t {
    DeviceNotFound,
    OpenFailed
};

} // namespace projectv::platform
```

---

## 2. ECS Integration

```cpp
// ProjectV.ECS.InputComponents.cppm
export module ProjectV.ECS.InputComponents;

import std;
import glm;
import ProjectV.Core.Platform.SDL;

export namespace projectv::ecs {

/// Input state component (singleton)
struct InputStateComponent {
    glm::vec2 mouse_position{0.0f, 0.0f};
    glm::vec2 mouse_delta{0.0f, 0.0f};
    glm::vec2 mouse_wheel{0.0f, 0.0f};

    bool key_w{false};
    bool key_a{false};
    bool key_s{false};
    bool key_d{false};
    bool key_space{false};
    bool key_shift{false};
    bool key_ctrl{false};
    bool key_escape{false};

    bool mouse_left{false};
    bool mouse_right{false};
    bool mouse_middle{false};

    bool quit_requested{false};
};

/// Player input component (attached to player entity)
struct PlayerInputComponent {
    glm::vec3 movement{0.0f, 0.0f, 0.0f};
    glm::vec2 look{0.0f, 0.0f};
    bool jump{false};
    bool sprint{false};
    bool crouch{false};
    bool attack{false};
    bool interact{false};
};

/// System that processes SDL events and updates InputStateComponent
class InputSystem {
public:
    static auto register_system(flecs::world& world, platform::Window& window) -> void {
        // Create singleton for input state
        world.set<InputStateComponent>({});

        // System to poll events
        world.system("ProcessInputEvents")
            .kind(flecs::PreUpdate)
            .iter([&window](flecs::iter& it) {
                auto* input = it.world().get_mut<InputStateComponent>();
                if (!input) return;

                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    switch (event.type) {
                        case SDL_EVENT_QUIT:
                            input->quit_requested = true;
                            break;

                        case SDL_EVENT_KEY_DOWN:
                        case SDL_EVENT_KEY_UP:
                            process_key_event(*input, event.key);
                            break;

                        case SDL_EVENT_MOUSE_MOTION:
                            input->mouse_position = {event.motion.x, event.motion.y};
                            input->mouse_delta = {event.motion.xrel, event.motion.yrel};
                            break;

                        case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        case SDL_EVENT_MOUSE_BUTTON_UP:
                            process_mouse_button(*input, event.button);
                            break;

                        case SDL_EVENT_MOUSE_WHEEL:
                            input->mouse_wheel = {event.wheel.x, event.wheel.y};
                            break;

                        default:
                            break;
                    }
                }
            });

        // System to map input to player actions
        world.system<PlayerInputComponent, PlayerComponent>("MapPlayerInput")
            .kind(flecs::OnUpdate)
            .each([](PlayerInputComponent& player_input, PlayerComponent const& player) {
                auto* input = world.get<InputStateComponent>();
                if (!input) return;

                // Movement
                player_input.movement = {
                    (input->key_d ? 1.0f : 0.0f) - (input->key_a ? 1.0f : 0.0f),
                    0.0f,
                    (input->key_s ? 1.0f : 0.0f) - (input->key_w ? 1.0f : 0.0f)
                };

                // Actions
                player_input.jump = input->key_space;
                player_input.sprint = input->key_shift;
                player_input.crouch = input->key_ctrl;
            });
    }

private:
    static auto process_key_event(InputStateComponent& input, SDL_KeyboardEvent const& event) -> void {
        bool pressed = event.down;
        switch (event.keysym.sym) {
            case SDLK_W: input.key_w = pressed; break;
            case SDLK_A: input.key_a = pressed; break;
            case SDLK_S: input.key_s = pressed; break;
            case SDLK_D: input.key_d = pressed; break;
            case SDLK_SPACE: input.key_space = pressed; break;
            case SDLK_LSHIFT: input.key_shift = pressed; break;
            case SDLK_LCTRL: input.key_ctrl = pressed; break;
            case SDLK_ESCAPE: input.key_escape = pressed; break;
        }
    }

    static auto process_mouse_button(InputStateComponent& input, SDL_MouseButtonEvent const& event) -> void {
        bool pressed = event.down;
        switch (event.button) {
            case 1: input.mouse_left = pressed; break;
            case 2: input.mouse_middle = pressed; break;
            case 3: input.mouse_right = pressed; break;
        }
    }
};

} // namespace projectv::ecs
```

---

## 3. Usage Pattern

```cpp
// Application initialization pattern
import std;
import ProjectV.Core.Platform.SDL;
import ProjectV.ECS.InputComponents;
import ProjectV.Render.Vulkan;

auto main() -> int {
    using namespace projectv;

    // 1. Initialize SDL
    auto sdl_context = platform::SDLContext::init(platform::SDLSubsystem::Video);
    if (!sdl_context) {
        std::println(stderr, "SDL init failed");
        return 1;
    }

    // 2. Create window
    auto window = platform::Window::create({
        .title = "ProjectV",
        .width = 1920,
        .height = 1080
    });
    if (!window) {
        std::println(stderr, "Window creation failed");
        return 1;
    }

    // 3. Get Vulkan extensions
    auto extensions = window->vulkan_instance_extensions();

    // 4. Create Vulkan instance
    auto instance = render::vulkan::Instance::create(extensions);
    if (!instance) {
        std::println(stderr, "Vulkan instance failed");
        return 1;
    }

    // 5. Create surface
    auto surface = window->create_vulkan_surface(instance->native());
    if (!surface) {
        std::println(stderr, "Surface creation failed");
        return 1;
    }

    // 6. Initialize ECS
    flecs::world ecs;
    ecs::InputSystem::register_system(ecs, *window);

    // 7. Main loop
    while (true) {
        auto* input = ecs.get<ecs::InputStateComponent>();
        if (input && input->quit_requested) {
            break;
        }

        ecs.progress(0.016f);
    }

    return 0;
}
```

---

## Статус

| Компонент       | Статус         | Приоритет |
|-----------------|----------------|-----------|
| SDLContext      | Специфицирован | P0        |
| Window          | Специфицирован | P0        |
| EventSystem     | Специфицирован | P0        |
| Gamepad         | Специфицирован | P1        |
| ECS Integration | Специфицирован | P0        |
| InputComponents | Специфицирован | P0        |

---

## Ссылки

- [ADR-0001: Vulkan Renderer](../../architecture/adr/0001-vulkan-renderer.md)
- [ADR-0003: ECS Architecture](../../architecture/adr/0003-ecs-architecture.md)
- [ADR-0004: Build System & Modules](../../architecture/adr/0004-build-and-modules-spec.md)
