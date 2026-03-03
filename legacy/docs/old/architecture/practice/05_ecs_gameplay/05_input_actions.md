# Input Actions: Action Mapping

Абстракция ввода для поддержки клавиатуры, мыши и геймпада.

---

## Концепция

### Проблема

Прямая проверка клавиш в коде игры:

```cpp
// Плохо: жёсткая привязка к конкретным клавишам
if (key == SDLK_W) moveForward();
if (key == SDLK_SPACE) jump();
if (button == SDL_BUTTON_LEFT) attack();
```

**Проблемы:**

- Не работает с геймпадом
- Невозможно переназначить клавиши
- Сложно поддерживать разные раскладки (QWERTY, AZERTY)
- Код игры зависит от конкретного API ввода

### Решение: Action Mapping

```cpp
// Хорошо: абстракция через Actions
if (input.isActionPressed(Action::MoveForward)) moveForward();
if (input.isActionPressed(Action::Jump)) jump();
if (input.isActionPressed(Action::Attack)) attack();
```

**Преимущества:**

- Поддержка клавиатуры, мыши и геймпада через единый интерфейс
- Пользовательское переназначение клавиш
- Независимость игровой логики от API ввода
- Dead zones, sensitivity, axis mapping

---

## Архитектура

### Слои абстракции

```
[Game Logic] → [Action System] → [Input Bindings] → [SDL3 / Gamepad]
                      ↑
               [Config JSON]
```

### Компоненты

| Компонент        | Назначение                                           |
|------------------|------------------------------------------------------|
| **Action**       | Логическое действие (MoveForward, Jump, Attack)      |
| **Binding**      | Связь Action с физическим вводом (Key, Button, Axis) |
| **InputManager** | Центральный менеджер, обрабатывает ввод              |
| **Config**       | JSON файл с настройками bindings                     |

---

## Определения

### Actions

```cpp
// Определение всех действий в игре
enum class Action {
    // Movement
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Jump,
    Crouch,
    Sprint,

    // Combat
    Attack,
    Block,
    Aim,

    // Interaction
    Interact,
    Inventory,
    Pause,

    // UI
    MenuUp,
    MenuDown,
    MenuLeft,
    MenuRight,
    MenuSelect,
    MenuBack,

    // Debug
    DebugToggle,
    DebugReload,

    Count
};

// Типы ввода для Actions
enum class ActionType {
    Button,    // Мгновенное нажатие (Jump, Attack)
    Axis1D,    // Одномерная ось (MoveForward/Backward)
    Axis2D     // Двумерная ось (Movement, Camera)
};
```

### Bindings

```cpp
// Источник ввода
enum class InputSource {
    Keyboard,
    Mouse,
    Gamepad
};

// Физическая кнопка/ось
struct InputBinding {
    InputSource source;
    union {
        SDL_Keycode key;           // Для клавиатуры
        uint8_t mouseButton;       // Для мыши
        SDL_GamepadButton button;  // Для геймпада
        SDL_GamepadAxis axis;      // Для осей геймпада
    };

    // Для осей: направление
    float axisScale = 1.0f;        // 1.0 или -1.0 для инверсии
    float deadZone = 0.15f;        // Мёртвая зона для осей
};

// Связка Action → Bindings
struct ActionBinding {
    Action action;
    ActionType type;
    std::vector<InputBinding> bindings;  // Несколько bindings на один action
};
```

---

## Input Manager

### Интерфейс

```cpp
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Инициализация
    bool initialize();
    void shutdown();

    // Загрузка конфигурации
    bool loadConfig(const std::string& path);
    bool saveConfig(const std::string& path);

    // Обработка событий (вызывать каждый кадр)
    void processEvent(const SDL_Event& event);
    void update(float deltaTime);

    // Проверка Actions
    bool isActionPressed(Action action) const;
    bool isActionHeld(Action action) const;
    bool isActionReleased(Action action) const;

    // Получение значений осей
    float getActionAxis1D(Action action) const;
    glm::vec2 getActionAxis2D(Action actionX, Action actionY) const;

    // Прямой доступ к состоянию устройств
    bool isKeyPressed(SDL_Keycode key) const;
    bool isGamepadButtonPressed(SDL_GamepadButton button) const;
    float getGamepadAxis(SDL_GamepadAxis axis) const;
    glm::vec2 getMouseDelta() const;

    // Управление bindings
    void bindAction(Action action, const InputBinding& binding);
    void unbindAction(Action action, const InputBinding& binding);
    void clearBindings(Action action);

    // Геймпад
    bool isGamepadConnected() const;
    void rumble(float intensity, float duration);

private:
    // Состояние клавиатуры
    std::array<bool, SDL_NUM_SCANCODES> keyState_{};
    std::array<bool, SDL_NUM_SCANCODES> keyPrevState_{};

    // Состояние мыши
    glm::vec2 mousePos_{};
    glm::vec2 mouseDelta_{};
    std::array<bool, 5> mouseButtonState_{};
    std::array<bool, 5> mouseButtonPrevState_{};

    // Состояние геймпада
    SDL_Gamepad* gamepad_ = nullptr;
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> gamepadAxis_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> gamepadButtonState_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> gamepadButtonPrevState_{};

    // Action bindings
    std::unordered_map<Action, ActionBinding> actionBindings_;

    // Action state (вычисляется из bindings)
    std::array<bool, static_cast<size_t>(Action::Count)> actionPressed_{};
    std::array<bool, static_cast<size_t>(Action::Count)> actionHeld_{};
    std::array<bool, static_cast<size_t>(Action::Count)> actionReleased_{};
    std::array<float, static_cast<size_t>(Action::Count)> actionAxis_{};
};
```

### Реализация

```cpp
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

InputManager::InputManager() {
    // Инициализация состояний
    keyState_.fill(false);
    keyPrevState_.fill(false);
    mouseButtonState_.fill(false);
    mouseButtonPrevState_.fill(false);
    gamepadAxis_.fill(0.0f);
    gamepadButtonState_.fill(false);
    gamepadButtonPrevState_.fill(false);
    actionPressed_.fill(false);
    actionHeld_.fill(false);
    actionReleased_.fill(false);
    actionAxis_.fill(0.0f);
}

bool InputManager::initialize() {
    // Инициализация SDL input
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        SDL_Log("Failed to initialize gamepad subsystem: %s", SDL_GetError());
        return false;
    }

    // Подключение первого доступного геймпада
    int numGamepads = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&numGamepads);
    if (numGamepads > 0) {
        gamepad_ = SDL_OpenGamepad(gamepads[0]);
        if (gamepad_) {
            SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(gamepad_));
        }
    }
    SDL_free(gamepads);

    // Загрузка default bindings
    loadDefaultBindings();

    return true;
}

void InputManager::processEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            auto scancode = event.key.keysym.scancode;
            if (scancode < SDL_NUM_SCANCODES) {
                keyState_[scancode] = (event.type == SDL_EVENT_KEY_DOWN);
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            uint8_t button = event.button.button;
            if (button < mouseButtonState_.size()) {
                mouseButtonState_[button] = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            mouseDelta_.x += event.motion.xrel;
            mouseDelta_.y += event.motion.yrel;
            mousePos_.x = event.motion.x;
            mousePos_.y = event.motion.y;
            break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            auto button = event.gbutton.button;
            if (button < gamepadButtonState_.size()) {
                gamepadButtonState_[button] = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            }
            break;
        }

        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            auto axis = event.gaxis.axis;
            if (axis < gamepadAxis_.size()) {
                // Нормализация [-32768, 32767] → [-1.0, 1.0]
                float value = event.gaxis.value / 32768.0f;
                gamepadAxis_[axis] = value;
            }
            break;
        }

        case SDL_EVENT_GAMEPAD_ADDED: {
            if (!gamepad_) {
                gamepad_ = SDL_OpenGamepad(event.gdevice.which);
                if (gamepad_) {
                    SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(gamepad_));
                }
            }
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED: {
            if (gamepad_ && SDL_GetGamepadID(gamepad_) == event.gdevice.which) {
                SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
                SDL_Log("Gamepad disconnected");
            }
            break;
        }
    }
}

void InputManager::update(float deltaTime) {
    // Очистка мгновенных состояний
    actionPressed_.fill(false);
    actionReleased_.fill(false);
    mouseDelta_ = glm::vec2(0.0f);

    // Обновление action states на основе bindings
    for (const auto& [action, binding] : actionBindings_) {
        float value = 0.0f;
        bool pressed = false;

        for (const auto& input : binding.bindings) {
            switch (input.source) {
                case InputSource::Keyboard: {
                    auto scancode = SDL_GetScancodeFromKey(input.key);
                    if (scancode < SDL_NUM_SCANCODES && keyState_[scancode]) {
                        pressed = true;
                        value = 1.0f;
                    }
                    break;
                }

                case InputSource::Mouse: {
                    if (input.mouseButton < mouseButtonState_.size() &&
                        mouseButtonState_[input.mouseButton]) {
                        pressed = true;
                        value = 1.0f;
                    }
                    break;
                }

                case InputSource::Gamepad: {
                    if (gamepad_) {
                        if (binding.type == ActionType::Button) {
                            if (input.button < gamepadButtonState_.size() &&
                                gamepadButtonState_[input.button]) {
                                pressed = true;
                                value = 1.0f;
                            }
                        } else {
                            // Axis
                            if (input.axis < gamepadAxis_.size()) {
                                float axisValue = gamepadAxis_[input.axis] * input.axisScale;
                                // Применение dead zone
                                if (std::abs(axisValue) > input.deadZone) {
                                    value = axisValue;
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        auto actionIdx = static_cast<size_t>(action);
        bool wasHeld = actionHeld_[actionIdx];

        actionHeld_[actionIdx] = pressed || (std::abs(value) > 0.0f);
        actionPressed_[actionIdx] = actionHeld_[actionIdx] && !wasHeld;
        actionReleased_[actionIdx] = !actionHeld_[actionIdx] && wasHeld;
        actionAxis_[actionIdx] = value;
    }

    // Сохранение предыдущего состояния для next frame
    keyPrevState_ = keyState_;
    mouseButtonPrevState_ = mouseButtonState_;
    gamepadButtonPrevState_ = gamepadButtonState_;
}

bool InputManager::isActionPressed(Action action) const {
    return actionPressed_[static_cast<size_t>(action)];
}

bool InputManager::isActionHeld(Action action) const {
    return actionHeld_[static_cast<size_t>(action)];
}

bool InputManager::isActionReleased(Action action) const {
    return actionReleased_[static_cast<size_t>(action)];
}

float InputManager::getActionAxis1D(Action action) const {
    return actionAxis_[static_cast<size_t>(action)];
}

glm::vec2 InputManager::getActionAxis2D(Action actionX, Action actionY) const {
    return glm::vec2(
        getActionAxis1D(actionX),
        getActionAxis1D(actionY)
    );
}

void InputManager::rumble(float intensity, float duration) {
    if (gamepad_) {
        Uint16 lowFreq = static_cast<Uint16>(intensity * 65535);
        Uint16 highFreq = static_cast<Uint16>(intensity * 65535);
        SDL_RumbleGamepad(gamepad_, lowFreq, highFreq,
                         static_cast<Uint32>(duration * 1000));
    }
}
```

---

## Default Bindings

```cpp
void InputManager::loadDefaultBindings() {
    // Movement
    actionBindings_[Action::MoveForward] = {
        .action = Action::MoveForward,
        .type = ActionType::Axis1D,
        .bindings = {
            {InputSource::Keyboard, .key = SDLK_W},
            {InputSource::Keyboard, .key = SDLK_UP},
            {InputSource::Gamepad, .axis = SDL_GAMEPAD_AXIS_LEFTY, .axisScale = -1.0f}
        }
    };

    actionBindings_[Action::MoveBackward] = {
        .action = Action::MoveBackward,
        .type = ActionType::Axis1D,
        .bindings = {
            {InputSource::Keyboard, .key = SDLK_S},
            {InputSource::Keyboard, .key = SDLK_DOWN},
            {InputSource::Gamepad, .axis = SDL_GAMEPAD_AXIS_LEFTY, .axisScale = 1.0f}
        }
    };

    actionBindings_[Action::MoveLeft] = {
        .action = Action::MoveLeft,
        .type = ActionType::Axis1D,
        .bindings = {
            {InputSource::Keyboard, .key = SDLK_A},
            {InputSource::Keyboard, .key = SDLK_LEFT},
            {InputSource::Gamepad, .axis = SDL_GAMEPAD_AXIS_LEFTX, .axisScale = -1.0f}
        }
    };

    actionBindings_[Action::MoveRight] = {
        .action = Action::MoveRight,
        .type = ActionType::Axis1D,
        .bindings = {
            {InputSource::Keyboard, .key = SDLK_D},
            {InputSource::Keyboard, .key = SDLK_RIGHT},
            {InputSource::Gamepad, .axis = SDL_GAMEPAD_AXIS_LEFTX, .axisScale = 1.0f}
        }
    };

    actionBindings_[Action::Jump] = {
        .action = Action::Jump,
        .type = ActionType::Button,
        .bindings = {
            {InputSource::Keyboard, .key = SDLK_SPACE},
            {InputSource::Gamepad, .button = SDL_GAMEPAD_BUTTON_SOUTH}  // A on Xbox
        }
    };

    actionBindings_[Action::Attack] = {
        .action = Action::Attack,
        .type = ActionType::Button,
        .bindings = {
            {InputSource::Mouse, .mouseButton = SDL_BUTTON_LEFT},
            {InputSource::Gamepad, .button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}  // RB
        }
    };

    actionBindings_[Action::Pause] = {
        .action = Action::Pause,
        .type = ActionType::Button,
        .bindings = {
            {InputSource::Keyboard, .key = SDLK_ESCAPE},
            {InputSource::Gamepad, .button = SDL_GAMEPAD_BUTTON_START}
        }
    };

    // ... остальные bindings
}
```

---

## Config File

### Формат

```json
{
    "version": 1,
    "bindings": {
        "MoveForward": [
            {"source": "keyboard", "key": "W"},
            {"source": "keyboard", "key": "Up"},
            {"source": "gamepad", "axis": "LeftY", "scale": -1.0, "deadZone": 0.15}
        ],
        "MoveBackward": [
            {"source": "keyboard", "key": "S"},
            {"source": "keyboard", "key": "Down"},
            {"source": "gamepad", "axis": "LeftY", "scale": 1.0, "deadZone": 0.15}
        ],
        "Jump": [
            {"source": "keyboard", "key": "Space"},
            {"source": "gamepad", "button": "A"}
        ],
        "Attack": [
            {"source": "mouse", "button": "Left"},
            {"source": "gamepad", "button": "RB"}
        ]
    }
}
```

### Загрузка

```cpp
bool InputManager::loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("Failed to open input config: %s", path.c_str());
        return false;
    }

    nlohmann::json json;
    file >> json;

    for (const auto& [actionName, bindings] : json["bindings"].items()) {
        Action action = stringToAction(actionName);
        if (action == Action::Count) continue;

        ActionBinding binding;
        binding.action = action;
        binding.type = ActionType::Button;  // Default

        for (const auto& b : bindings) {
            InputBinding input;

            std::string source = b["source"];
            if (source == "keyboard") {
                input.source = InputSource::Keyboard;
                input.key = stringToKey(b["key"]);
            } else if (source == "mouse") {
                input.source = InputSource::Mouse;
                input.mouseButton = stringToMouseButton(b["button"]);
            } else if (source == "gamepad") {
                input.source = InputSource::Gamepad;
                if (b.contains("button")) {
                    input.button = stringToGamepadButton(b["button"]);
                } else if (b.contains("axis")) {
                    input.axis = stringToGamepadAxis(b["axis"]);
                    input.axisScale = b.value("scale", 1.0f);
                    input.deadZone = b.value("deadZone", 0.15f);
                    binding.type = ActionType::Axis1D;
                }
            }

            binding.bindings.push_back(input);
        }

        actionBindings_[action] = binding;
    }

    return true;
}
```

---

## Использование в игре

### Интеграция с ECS

```cpp
// Компонент для ввода
struct PlayerInput {
    glm::vec2 moveDirection;
    bool jumpPressed;
    bool attackPressed;
    bool aimHeld;
};

// Система обработки ввода
void PlayerInputSystem(flecs::iter& it) {
    auto* inputs = it.field<PlayerInput>(0);
    InputManager& input = *static_cast<InputManager*>(it.ctx);

    for (auto i : it) {
        // Получение значений через action abstraction
        inputs[i].moveDirection = input.getActionAxis2D(
            Action::MoveRight,
            Action::MoveForward  // Примечание: Y инвертирован для movement
        );

        // Нормализация диагонального движения
        if (glm::length(inputs[i].moveDirection) > 1.0f) {
            inputs[i].moveDirection = glm::normalize(inputs[i].moveDirection);
        }

        inputs[i].jumpPressed = input.isActionPressed(Action::Jump);
        inputs[i].attackPressed = input.isActionPressed(Action::Attack);
        inputs[i].aimHeld = input.isActionHeld(Action::Aim);
    }
}
```

### Main Loop

```cpp
int main() {
    InputManager input;
    input.initialize();
    input.loadConfig("config/input_bindings.json");

    flecs::world ecs;
    ecs.set_ctx(&input);
    ecs.system<PlayerInput>("PlayerInputSystem")
       .kind(flecs::PreUpdate)
       .iter(PlayerInputSystem);

    SDL_Event event;
    bool running = true;

    while (running) {
        // Обработка событий
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            input.processEvent(event);
        }

        // Обновление input state
        input.update(deltaTime);

        // ECS update (включая PlayerInputSystem)
        ecs.progress(deltaTime);

        // Рендеринг...
    }

    input.shutdown();
    return 0;
}
```

---

## UI для переназначения клавиш

```cpp
class InputRemappingUI {
public:
    void drawImGui(InputManager& input) {
        ImGui::Begin("Input Settings");

        for (const auto& [action, binding] : input.getAllBindings()) {
            ImGui::Text("%s", actionToString(action));

            for (size_t i = 0; i < binding.bindings.size(); i++) {
                ImGui::PushID(i);

                std::string label = bindingToString(binding.bindings[i]);
                if (ImGui::Button(label.c_str(), ImVec2(100, 0))) {
                    waitingForInput_ = true;
                    remappingAction_ = action;
                    remappingIndex_ = i;
                }

                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    input.unbindAction(action, binding.bindings[i]);
                }

                ImGui::PopID();
            }

            ImGui::Separator();
        }

        ImGui::End();
    }

    void handleRebind(const SDL_Event& event, InputManager& input) {
        if (!waitingForInput_) return;

        InputBinding newBinding;

        if (event.type == SDL_EVENT_KEY_DOWN) {
            newBinding.source = InputSource::Keyboard;
            newBinding.key = event.key.keysym.sym;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            newBinding.source = InputSource::Mouse;
            newBinding.mouseButton = event.button.button;
        } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            newBinding.source = InputSource::Gamepad;
            newBinding.button = event.gbutton.button;
        } else {
            return;  // Не тот тип события
        }

        input.bindAction(remappingAction_, newBinding);
        waitingForInput_ = false;
    }

private:
    bool waitingForInput_ = false;
    Action remappingAction_;
    size_t remappingIndex_;
};
```

---

## Рекомендации

### Best Practices

1. **Разделяйте логику и ввод:** Игровая логика не должна знать о SDL_Keycode
2. **Поддерживайте несколько bindings:** Клавиатура + геймпад одновременно
3. **Dead zones:** Всегда применяйте для осей геймпада
4. **Сохранение настроек:** Позвольте игроку сохранить свою раскладку

### Axis Handling

```cpp
// Правильное получение 2D направления движения
glm::vec2 getMovementDirection(InputManager& input) {
    glm::vec2 dir;
    dir.x = input.getActionAxis1D(Action::MoveRight) -
            input.getActionAxis1D(Action::MoveLeft);
    dir.y = input.getActionAxis1D(Action::MoveForward) -
            input.getActionAxis1D(Action::MoveBackward);

    // Нормализация для консистентной скорости по диагонали
    if (glm::length(dir) > 1.0f) {
        dir = glm::normalize(dir);
    }

    return dir;
}
```

---

## Резюме

**Принцип:** Игровая логика работает с Actions, не с клавишами.

**Выгода:**

- Поддержка клавиатуры, мыши и геймпада
- Пользовательское переназначение
- Чистый код игры
- Лёгкое тестирование

**Затраты:** Один класс InputManager + config JSON.
