# Input System: Слои ввода для ProjectV [🟡 Уровень 2]

Архитектура системы обработки ввода с поддержкой слоёв для предотвращения конфликтов между интерфейсом, отладкой и
игровым управлением.

## Проблема

Конфликты ввода в игровом движке:

- Вы открываете консоль (ImGui), нажимаете 'W' чтобы написать команду, а камера летит вперёд
- Debug UI перехватывает клики, которые должны попасть в игровой мир
- Несколько систем одновременно обрабатывают одни и те же события

## Решение: Стек слоёв ввода

### Архитектура

```
┌─────────────────────────────────────────┐
│           Input Event Stack             │
├─────────────────────────────────────────┤
│  Layer 4: ImGui (Debug UI)             │ ← Top priority
│  Layer 3: Debug Console                │
│  Layer 2: Game UI (меню, HUD)          │
│  Layer 1: Player Controller            │ ← Gameplay
│  Layer 0: System Events (quit, resize) │ ← Always processed
└─────────────────────────────────────────┘
```

**Принцип работы**: Событие идёт сверху вниз. Если слой обработал событие (вернул `true`), оно не идёт дальше.

## Реализация

### Базовые классы

```cpp
// Типы событий ввода
enum class InputEventType {
    KeyDown,
    KeyUp,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    TextInput,
    ControllerButton,
    ControllerAxis,
    Touch,
    Gesture
};

// Базовое событие
struct InputEvent {
    InputEventType type;
    uint32_t timestamp;
    bool handled = false;

    // Общие данные
    union {
        struct {
            SDL_Keycode key;
            SDL_Keymod mod;
            bool repeat;
        } keyboard;

        struct {
            int x, y;
            int relX, relY;
            uint8_t button;
        } mouse;

        struct {
            const char* text;
        } text;

        struct {
            float x, y;
        } wheel;
    };
};

// Интерфейс слоя ввода
class InputLayer {
public:
    virtual ~InputLayer() = default;

    // Обработка события
    virtual bool handleEvent(const InputEvent& event) = 0;

    // Обновление состояния (для аналогового ввода)
    virtual void update(float deltaTime) {}

    // Приоритет слоя (чем выше, тем раньше обрабатывается)
    virtual int getPriority() const = 0;

    // Активность слоя
    virtual bool isActive() const { return true; }

    // Имя слоя для отладки
    virtual const char* getName() const = 0;
};

// Менеджер слоёв ввода
class InputLayerManager {
public:
    void addLayer(std::unique_ptr<InputLayer> layer) {
        layers_.push_back(std::move(layer));
        // Сортировка по приоритету (от высокого к низкому)
        std::sort(layers_.begin(), layers_.end(),
             {
                return a->getPriority() > b->getPriority();
            });
    }

    void removeLayer(const char* name) {
        layers_.erase(
            std::remove_if(layers_.begin(), layers_.end(),
                [name](const auto& layer) {
                    return std::strcmp(layer->getName(), name) == 0;
                }),
            layers_.end()
        );
    }

    // Обработка события через стек слоёв
    bool processEvent(const InputEvent& event) {
        // Создаём копию события для обработки
        InputEvent processedEvent = event;

        for (const auto& layer : layers_) {
            if (!layer->isActive()) continue;

            if (layer->handleEvent(processedEvent)) {
                // Слой обработал событие - останавливаем распространение
                return true;
            }

            // Если событие было помечено как обработанное, останавливаемся
            if (processedEvent.handled) {
                return true;
            }
        }

        return false; // Ни один слой не обработал событие
    }

    // Обновление всех слоёв
    void update(float deltaTime) {
        for (const auto& layer : layers_) {
            if (layer->isActive()) {
                layer->update(deltaTime);
            }
        }
    }

    // Поиск слоя по имени
    InputLayer* findLayer(const char* name) {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [name](const auto& layer) {
                return std::strcmp(layer->getName(), name) == 0;
            });

        return it != layers_.end() ? it->get() : nullptr;
    }

private:
    std::vector<std::unique_ptr<InputLayer>> layers_;
};
```

### Слой ImGui (высший приоритет)

```cpp
class ImGuiInputLayer : public InputLayer {
public:
    ImGuiInputLayer(SDL_Window* window) : window_(window) {
        // Инициализация ImGui IO
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Настройка отображения клавиш
        io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = SDLK_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = SDLK_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = SDLK_UP;
        io.KeyMap[ImGuiKey_DownArrow] = SDLK_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = SDLK_PAGEUP;
        io.KeyMap[ImGuiKey_PageDown] = SDLK_PAGEDOWN;
        io.KeyMap[ImGuiKey_Home] = SDLK_HOME;
        io.KeyMap[ImGuiKey_End] = SDLK_END;
        io.KeyMap[ImGuiKey_Insert] = SDLK_INSERT;
        io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
        io.KeyMap[ImGuiKey_Space] = SDLK_SPACE;
        io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
        io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
        io.KeyMap[ImGuiKey_A] = SDLK_a;
        io.KeyMap[ImGuiKey_C] = SDLK_c;
        io.KeyMap[ImGuiKey_V] = SDLK_v;
        io.KeyMap[ImGuiKey_X] = SDLK_x;
        io.KeyMap[ImGuiKey_Y] = SDLK_y;
        io.KeyMap[ImGuiKey_Z] = SDLK_z;
    }

    bool handleEvent(const InputEvent& event) override {
        auto& io = ImGui::GetIO();

        switch (event.type) {
            case InputEventType::KeyDown:
                io.KeysDown[event.keyboard.key] = true;
                io.KeyCtrl = (event.keyboard.mod & KMOD_CTRL) != 0;
                io.KeyShift = (event.keyboard.mod & KMOD_SHIFT) != 0;
                io.KeyAlt = (event.keyboard.mod & KMOD_ALT) != 0;
                io.KeySuper = (event.keyboard.mod & KMOD_GUI) != 0;
                return io.WantCaptureKeyboard;

            case InputEventType::KeyUp:
                io.KeysDown[event.keyboard.key] = false;
                io.KeyCtrl = (event.keyboard.mod & KMOD_CTRL) != 0;
                io.KeyShift = (event.keyboard.mod & KMOD_SHIFT) != 0;
                io.KeyAlt = (event.keyboard.mod & KMOD_ALT) != 0;
                io.KeySuper = (event.keyboard.mod & KMOD_GUI) != 0;
                return io.WantCaptureKeyboard;

            case InputEventType::TextInput:
                io.AddInputCharactersUTF8(event.text.text);
                return io.WantTextInput;

            case InputEventType::MouseMove:
                io.MousePos = ImVec2(
                    static_cast<float>(event.mouse.x),
                    static_cast<float>(event.mouse.y)
                );
                return io.WantCaptureMouse;

            case InputEventType::MouseDown:
                if (event.mouse.button == SDL_BUTTON_LEFT) io.MouseDown[0] = true;
                if (event.mouse.button == SDL_BUTTON_RIGHT) io.MouseDown[1] = true;
                if (event.mouse.button == SDL_BUTTON_MIDDLE) io.MouseDown[2] = true;
                return io.WantCaptureMouse;

            case InputEventType::MouseUp:
                if (event.mouse.button == SDL_BUTTON_LEFT) io.MouseDown[0] = false;
                if (event.mouse.button == SDL_BUTTON_RIGHT) io.MouseDown[1] = false;
                if (event.mouse.button == SDL_BUTTON_MIDDLE) io.MouseDown[2] = false;
                return io.WantCaptureMouse;

            case InputEventType::MouseWheel:
                io.MouseWheelH += event.wheel.x;
                io.MouseWheel += event.wheel.y;
                return io.WantCaptureMouse;

            default:
                return false;
        }
    }

    void update(float deltaTime) override {
        auto& io = ImGui::GetIO();
        io.DeltaTime = deltaTime;

        // Обновление состояния мыши
        int mouseX, mouseY;
        Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
        io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
        io.MouseDown[0] = (mouseButtons & SDL_BUTTON_LMASK) != 0;
        io.MouseDown[1] = (mouseButtons & SDL_BUTTON_RMASK) != 0;
        io.MouseDown[2] = (mouseButtons & SDL_BUTTON_MMASK) != 0;

        // Обновление состояния клавиатуры
        const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
        io.KeyCtrl = keyboardState[SDL_SCANCODE_LCTRL] || keyboardState[SDL_SCANCODE_RCTRL];
        io.KeyShift = keyboardState[SDL_SCANCODE_LSHIFT] || keyboardState[SDL_SCANCODE_RSHIFT];
        io.KeyAlt = keyboardState[SDL_SCANCODE_LALT] || keyboardState[SDL_SCANCODE_RALT];
        io.KeySuper = keyboardState[SDL_SCANCODE_LGUI] || keyboardState[SDL_SCANCODE_RGUI];
    }

    int getPriority() const override { return 100; } // Высший приоритет
    const char* getName() const override { return "ImGui"; }

private:
    SDL_Window* window_;
};
```

### Слой Debug Console

```cpp
class DebugConsoleInputLayer : public InputLayer {
public:
    DebugConsoleInputLayer() : visible_(false) {}

    bool handleEvent(const InputEvent& event) override {
        if (!visible_) return false;

        switch (event.type) {
            case InputEventType::KeyDown:
                // Перехват всех клавиш, когда консоль видима
                if (event.keyboard.key == SDLK_BACKQUOTE) {
                    // ` - скрыть консоль
                    visible_ = false;
                    return true;
                }

                // Обработка ввода в консоль
                return handleConsoleInput(event);

            case InputEventType::TextInput:
                // Добавление текста в консоль
                consoleBuffer_ += event.text.text;
                return true;

            case InputEventType::KeyUp:
                // Игнорируем KeyUp когда консоль открыта
                return true;

            default:
                return false;
        }
    }

    void update(float deltaTime) override {
        // Обновление автодополнения, истории и т.д.
        if (visible_) {
            updateConsole(deltaTime);
        }
    }

    int getPriority() const override { return 90; } // Ниже ImGui
    const char* getName() const override { return "DebugConsole"; }

    bool isActive() const override { return visible_; }

    void toggle() { visible_ = !visible_; }
    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool isVisible() const { return visible_; }

private:
    bool visible_;
    std::string consoleBuffer_;
    std::vector<std::string> commandHistory_;
    size_t historyIndex_ = 0;

    bool handleConsoleInput(const InputEvent& event) {
        switch (event.keyboard.key) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                executeCommand(consoleBuffer_);
                consoleBuffer_.clear();
                return true;

            case SDLK_BACKSPACE:
                if (!consoleBuffer_.empty()) {
                    consoleBuffer_.pop_back();
                }
                return true;

            case SDLK_UP:
                if (!commandHistory_.empty()) {
                    historyIndex_ = (historyIndex_ + 1) % commandHistory_.size();
                    consoleBuffer_ = commandHistory_[historyIndex_];
                }
                return true;

            case SDLK_DOWN:
                if (!commandHistory_.empty()) {
                    historyIndex_ = (historyIndex_ - 1) % commandHistory_.size();
                    consoleBuffer_ = commandHistory_[historyIndex_];
                }
                return true;

            case SDLK_TAB:
                // Автодополнение
                autoComplete();
                return true;

            default:
                return true; // Перехватываем все остальные клавиши
        }
    }

    void executeCommand(const std::string& command) {
        commandHistory_.push_back(command);
        historyIndex_ = commandHistory_.size();

        // Обработка команды
        // ...
    }

    void autoComplete() {
        // Реализация автодополнения
        // ...
    }

    void updateConsole(float deltaTime) {
        // Обновление мигающего курсора и т.д.
        // ...
    }
};
```

### Слой Game UI (меню, HUD)

```cpp
class GameUIInputLayer : public InputLayer {
public:
    GameUIInputLayer() : menuVisible_(false), hudVisible_(true) {}

    bool handleEvent(const InputEvent& event) override {
        // Проверяем, активен ли какой-либо UI элемент
        if (!hasActiveUI()) return false;

        switch (event.type) {
            case InputEventType::KeyDown:
                // Обработка навигации по меню
                if (menuVisible_) {
                    return handleMenuNavigation(event);
                }
                break;

            case InputEventType::MouseMove:
                // Проверка hover UI элементов
                return checkUIHover(event.mouse.x, event.mouse.y);

            case InputEventType::MouseDown:
                // Клик по UI элементам
                return handleUIClick(event.mouse.x, event.mouse.y, event.mouse.button);

            default:
                break;
        }

        return false;
    }

    void update(float deltaTime) override {
        // Обновление анимаций UI
        updateUIAnimations(deltaTime);
    }

    int getPriority() const override { return 80; }
    const char* getName() const override { return "GameUI"; }

    bool isActive() const override {
        return menuVisible_ || hudVisible_;
    }

    void showMenu() { menuVisible_ = true; }
    void hideMenu() { menuVisible_ = false; }
    void toggleHUD() { hudVisible_ = !hudVisible_; }

private:
    bool menuVisible_;
    bool hudVisible_;
    std::vector<UIElement*> activeElements_;

    bool hasActiveUI() const {
        return menuVisible_ || !activeElements_.empty();
    }

    bool handleMenuNavigation(const InputEvent& event) {
        switch (event.keyboard.key) {
            case SDLK_UP:
            case SDLK_DOWN:
            case SDLK_LEFT:
            case SDLK_RIGHT:
            case SDLK_RETURN:
            case SDLK_ESCAPE:
                // Навигация по меню
                navigateMenu(event.keyboard.key);
                return true;

            default:
                return false;
        }
    }

    bool checkUIHover(int x, int y) {
        for (auto* element : activeElements_) {
            if (element->contains(x, y)) {
                element->setHover(true);
                return true;
            } else {
                element->setHover(false);
            }
        }
        return false;
    }

    bool handleUIClick(int x, int y, uint8_t button) {
        for (auto* element : activeElements_) {
            if (element->contains(x, y)) {
                element->onClick(button);
                return true;
            }
        }
        return false;
    }

    void navigateMenu(SDL_Keycode key) {
        // Логика навигации по меню
        // ...
    }

    void updateUIAnimations(float deltaTime) {
        // Обновление анимаций UI элементов
        // ...
    }
};
```

### Слой Player Controller (игровое управление)

```cpp
class PlayerControllerInputLayer : public InputLayer {
public:
    PlayerControllerInputLayer() : camera_(nullptr), movementSpeed_(5.0f), mouseSensitivity_(0.1f) {}

    bool handleEvent(const InputEvent& event) override {
        switch (event.type) {
            case InputEventType::KeyDown:
                return handleKeyDown(event.keyboard.key);

            case InputEventType::KeyUp:
                return handleKeyUp(event.keyboard.key);

            case InputEventType::MouseMove:
                return handleMouseMove(event.mouse.relX, event.mouse.relY);

            case InputEventType::MouseDown:
                return handleMouseDown(event.mouse.button);

            case InputEventType::MouseUp:
                return handleMouseUp(event.mouse.button);

            case InputEventType::MouseWheel:
                return handleMouseWheel(event.wheel.y);

            default:
                return false;
        }
    }

    void update(float deltaTime) override {
        // Обновление движения на основе текущего состояния клавиш
        updateMovement(deltaTime);

        // Обновление камеры
        if (camera_) {
            camera_->update(deltaTime);
        }
    }

    int getPriority() const override { return 70; }
    const char* getName() const override { return "PlayerController"; }

    void setCamera(Camera* camera) { camera_ = camera; }
    void setMovementSpeed(float speed) { movementSpeed_ = speed; }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

    const glm::vec3& getMovementDirection() const { return movementDirection_; }
    bool isMoving() const { return glm::length(movementDirection_) > 0.0f; }

private:
    Camera* camera_;
    float movementSpeed_;
    float mouseSensitivity_;
    glm::vec3 movementDirection_{0.0f};
    std::unordered_set<SDL_Keycode> pressedKeys_;
    bool mouseCaptured_ = false;

    bool handleKeyDown(SDL_Keycode key) {
        pressedKeys_.insert(key);

        // Обработка специальных клавиш
        switch (key) {
            case SDLK_ESCAPE:
                // Выход из захвата мыши
                if (mouseCaptured_) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    mouseCaptured_ = false;
                    return true;
                }
                break;

            case SDLK_F1:
                // Переключение захвата мыши
                mouseCaptured_ = !mouseCaptured_;
                SDL_SetRelativeMouseMode(mouseCaptured_ ? SDL_TRUE : SDL_FALSE);
                return true;
        }

        return false;
    }

    bool handleKeyUp(SDL_Keycode key) {
        pressedKeys_.erase(key);
        return false;
    }

    bool handleMouseMove(int relX, int relY) {
        if (!mouseCaptured_ || !camera_) return false;

        // Поворот камеры
        float yaw = static_cast<float>(relX) * mouseSensitivity_;
        float pitch = static_cast<float>(relY) * mouseSensitivity_;

        camera_->rotate(yaw, -pitch); // Инвертируем pitch для естественного управления

        return true;
    }

    bool handleMouseDown(uint8_t button) {
        if (button == SDL_BUTTON_LEFT && mouseCaptured_) {
            // Выстрел или взаимодействие
            // ...
            return true;
        }
        return false;
    }

    bool handleMouseUp(uint8_t button) {
        return false;
    }

    bool handleMouseWheel(float y) {
        if (camera_) {
            // Изменение FOV или скорости движения
            camera_->adjustFOV(y * 2.0f);
            return true;
        }
        return false;
    }

    void updateMovement(float deltaTime) {
        movementDirection_ = glm::vec3(0.0f);

        // WASD движение
        if (pressedKeys_.count(SDLK_w)) movementDirection_.z -= 1.0f;
        if (pressedKeys_.count(SDLK_s)) movementDirection_.z += 1.0f;
        if (pressedKeys_.count(SDLK_a)) movementDirection_.x -= 1.0f;
        if (pressedKeys_.count(SDLK_d)) movementDirection_.x += 1.0f;

        // Space/Shift для вертикального движения
        if (pressedKeys_.count(SDLK_SPACE)) movementDirection_.y += 1.0f;
        if (pressedKeys_.count(SDLK_LSHIFT) || pressedKeys_.count(SDLK_RSHIFT)) movementDirection_.y -= 1.0f;

        // Нормализация и применение скорости
        if (glm::length(movementDirection_) > 0.0f) {
            movementDirection_ = glm::normalize(movementDirection_) * movementSpeed_ * deltaTime;

            if (camera_) {
                camera_->move(movementDirection_);
            }
        }
    }
};
```

### Слой System Events (системные события)

```cpp
class SystemEventsInputLayer : public InputLayer {
public:
    SystemEventsInputLayer(bool& running) : running_(running) {}

    bool handleEvent(const InputEvent& event) override {
        switch (event.type) {
            case InputEventType::KeyDown:
                // Alt+F4 или другие системные комбинации
                if (event.keyboard.key == SDLK_F4 &&
                    (event.keyboard.mod & KMOD_ALT)) {
                    running_ = false;
                    return true;
                }
                break;

            // Обработка SDL_QUIT и других системных событий
            // ...
        }

        return false;
    }

    void update(float deltaTime) override {
        // Проверка системных состояний
        // ...
    }

    int getPriority() const override { return 0; } // Самый низкий приоритет
    const char* getName() const override { return "SystemEvents"; }

private:
    bool& running_;
};
```

## Интеграция с SDL

```cpp
class SDLInputAdapter {
public:
    SDLInputAdapter(InputLayerManager& manager, bool& running)
        : manager_(manager), running_(running) {

        // Создание и добавление слоёв
        manager_.addLayer(std::make_unique<SystemEventsInputLayer>(running_));
        manager_.addLayer(std::make_unique<PlayerControllerInputLayer>());
        manager_.addLayer(std::make_unique<GameUIInputLayer>());
        manager_.addLayer(std::make_unique<DebugConsoleInputLayer>());

        // ImGui слой добавляется позже, после инициализации окна
    }

    void setImGuiLayer(SDL_Window* window) {
        manager_.addLayer(std::make_unique<ImGuiInputLayer>(window));
    }

    void processSDLEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            InputEvent inputEvent = convertSDLEvent(event);
            if (inputEvent.type != InputEventType::Unknown) {
                manager_.processEvent(inputEvent);
            }
        }
    }

    void update(float deltaTime) {
        manager_.update(deltaTime);
    }

private:
    InputLayerManager& manager_;
    bool& running_;

    InputEvent convertSDLEvent(const SDL_Event& event) {
        InputEvent inputEvent{};
        inputEvent.timestamp = event.common.timestamp;

        switch (event.type) {
            case SDL_KEYDOWN:
                inputEvent.type = InputEventType::KeyDown;
                inputEvent.keyboard.key = event.key.keysym.sym;
                inputEvent.keyboard.mod = static_cast<SDL_Keymod>(event.key.keysym.mod);
                inputEvent.keyboard.repeat = event.key.repeat != 0;
                break;

            case SDL_KEYUP:
                inputEvent.type = InputEventType::KeyUp;
                inputEvent.keyboard.key = event.key.keysym.sym;
                inputEvent.keyboard.mod = static_cast<SDL_Keymod>(event.key.keysym.mod);
                break;

            case SDL_TEXTINPUT:
                inputEvent.type = InputEventType::TextInput;
                inputEvent.text.text = event.text.text;
                break;

            case SDL_MOUSEMOTION:
                inputEvent.type = InputEventType::MouseMove;
                inputEvent.mouse.x = event.motion.x;
                inputEvent.mouse.y = event.motion.y;
                inputEvent.mouse.relX = event.motion.xrel;
                inputEvent.mouse.relY = event.motion.yrel;
                break;

            case SDL_MOUSEBUTTONDOWN:
                inputEvent.type = InputEventType::MouseDown;
                inputEvent.mouse.x = event.button.x;
                inputEvent.mouse.y = event.button.y;
                inputEvent.mouse.button = event.button.button;
                break;

            case SDL_MOUSEBUTTONUP:
                inputEvent.type = InputEventType::MouseUp;
                inputEvent.mouse.x = event.button.x;
                inputEvent.mouse.y = event.button.y;
                inputEvent.mouse.button = event.button.button;
                break;

            case SDL_MOUSEWHEEL:
                inputEvent.type = InputEventType::MouseWheel;
                inputEvent.wheel.x = static_cast<float>(event.wheel.x);
                inputEvent.wheel.y = static_cast<float>(event.wheel.y);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
                inputEvent.type = InputEventType::ControllerButton;
                // ...
                break;

            case SDL_CONTROLLERBUTTONUP:
                inputEvent.type = InputEventType::ControllerButton;
                // ...
                break;

            case SDL_CONTROLLERAXISMOTION:
                inputEvent.type = InputEventType::ControllerAxis;
                // ...
                break;

            case SDL_QUIT:
                running_ = false;
                break;

            default:
                inputEvent.type = InputEventType::Unknown;
                break;
        }

        return inputEvent;
    }
};
```

## Конфигурация и настройка

### Конфигурационный файл

```yaml
# input_config.yaml
layers:
  - name: "ImGui"
    priority: 100
    enabled: true

  - name: "DebugConsole"
    priority: 90
    enabled: true
    toggle_key: "`"

  - name: "GameUI"
    priority: 80
    enabled: true

  - name: "PlayerController"
    priority: 70
    enabled: true
    movement_speed: 5.0
    mouse_sensitivity: 0.1
    mouse_capture_key: "F1"

  - name: "SystemEvents"
    priority: 0
    enabled: true

key_bindings:
  move_forward: "W"
  move_backward: "S"
  move_left: "A"
  move_right: "D"
  jump: "SPACE"
  crouch: "LSHIFT"
  interact: "E"
  toggle_menu: "ESCAPE"
  toggle_console: "`"
  toggle_mouse_capture: "F1"
```

### Динамическая конфигурация

```cpp
class InputConfig {
public:
    void loadFromFile(const std::string& filename) {
        // Загрузка из YAML/JSON
        // ...
    }

    void saveToFile(const std::string& filename) {
        // Сохранение в YAML/JSON
        // ...
    }

    void applyToManager(InputLayerManager& manager) {
        // Применение конфигурации к менеджеру слоёв
        // ...
    }

    // Геттеры/сеттеры для динамического изменения
    float getMovementSpeed() const { return movementSpeed_; }
    void setMovementSpeed(float speed) { movementSpeed_ = speed; }

    float getMouseSensitivity() const { return mouseSensitivity_; }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

    // ...
};
```

## Заключительные рекомендации для ProjectV

### 1. Приоритеты для воксельного движка

1. **ImGui слой** — критически важен для отладки воксельного рендеринга
2. **Player Controller** — должен поддерживать плавное движение по воксельному миру
3. **System Events** — обработка изменения размера окна при рендеринге вокселей

### 2. Оптимизации для ProjectV

```cpp
// Специализированный слой для воксельного редактора
class VoxelEditorInputLayer : public InputLayer {
public:
    bool handleEvent(const InputEvent& event) override {
        // Быстрая обработка горячих клавиш редактора
        if (event.type == InputEventType::KeyDown) {
            switch (event.keyboard.key) {
                case SDLK_1: setBrushType(BrushType::Add); return true;
                case SDLK_2: setBrushType(BrushType::Remove); return true;
                case SDLK_3: setBrushType(BrushType::Paint); return true;
                case SDLK_EQUALS: increaseBrushSize(); return true;
                case SDLK_MINUS: decreaseBrushSize(); return true;
                // ...
            }
        }
        return false;
    }

    // ...
};
```

### 3. Интеграция с ECS (flecs)

```cpp
// Компонент для хранения состояния ввода
struct InputState {
    glm::vec3 movement{0.0f};
    glm::vec2 lookDelta{0.0f};
    bool primaryAction = false;
    bool secondaryAction = false;
    // ...
};

// Система обработки ввода в ECS
void InputSystem(flecs::iter& it, InputState* states) {
    auto* inputManager = it.world().get<InputLayerManager>();

    for (auto i : it) {
        // Обновление состояния на основе InputLayerManager
        // ...
    }
}
```

### 4. Профилирование с Tracy

```cpp
class ProfiledInputLayer : public InputLayer {
public:
    bool handleEvent(const InputEvent& event) override {
        ZoneScopedN("InputLayer::handleEvent");

        // Обработка события
        // ...

        return result;
    }

    void update(float deltaTime) override {
        ZoneScopedN("InputLayer::update");

        // Обновление состояния
        // ...
    }
};
```

## Примеры использования

### Базовый пример

```cpp
int main() {
    // Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* window = SDL_CreateWindow(...);

    bool running = true;

    // Создание менеджера слоёв
    InputLayerManager manager;

    // Создание адаптера SDL
    SDLInputAdapter adapter(manager, running);
    adapter.setImGuiLayer(window);

    // Игровой цикл
    while (running) {
        float deltaTime = calculateDeltaTime();

        // Обработка событий SDL
        adapter.processSDLEvents();

        // Обновление слоёв
        adapter.update(deltaTime);

        // Рендеринг
        // ...
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

### Расширенный пример с конфигурацией

```cpp
class Game {
public:
    Game() : config_(), manager_(), adapter_(manager_, running_) {
        // Загрузка конфигурации
        config_.loadFromFile("input_config.yaml");
        config_.applyToManager(manager_);

        // Настройка камеры
        auto* playerLayer = dynamic_cast<PlayerControllerInputLayer*>(
            manager_.findLayer("PlayerController"));
        if (playerLayer) {
            playerLayer->setCamera(&camera_);
            playerLayer->setMovementSpeed(config_.getMovementSpeed());
            playerLayer->setMouseSensitivity(config_.getMouseSensitivity());
        }
    }

    void run() {
        while (running_) {
            float deltaTime = timer_.tick();

            // Обработка ввода
            adapter_.processSDLEvents();
            adapter_.update(deltaTime);

            // Обновление игровой логики
            update(deltaTime);

            // Рендеринг
            render();
        }
    }

private:
    InputConfig config_;
    InputLayerManager manager_;
    SDLInputAdapter adapter_;
    Camera camera_;
    Timer timer_;
    bool running_ = true;

    void update(float deltaTime) {
        // Обновление игрового состояния
        // ...
    }

    void render() {
        // Рендеринг сцены
        // ...
    }
};
```

