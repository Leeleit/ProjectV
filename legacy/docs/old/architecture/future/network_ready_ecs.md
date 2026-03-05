# Network-Ready ECS Specification

---

## Обзор

Документ описывает архитектуру **Network-Ready ECS** для будущей реализации мультиплеера. Компоненты проектируются с
учётом:

- Репликации состояния между сервером и клиентами
- Rollback/Prediction для плавного геймплея
- Истории состояний для синхронизации
- Минимального bandwidth

---

## 1. Core Network Types

### 1.1 Network ID & Tick

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.NetworkTypes;

import std;
import glm;

export namespace projectv::ecs::net {

/// Уникальный идентификатор для репликации
struct NetworkID {
    uint64_t value{0};

    [[nodiscard]] auto is_valid() const noexcept -> bool { return value != 0; }
    [[nodiscard]] auto is_server_owned() const noexcept -> bool {
        return (value & SERVER_OWNED_BIT) != 0;
    }
    [[nodiscard]] auto is_client_owned() const noexcept -> bool {
        return (value & SERVER_OWNED_BIT) == 0;
    }
    [[nodiscard]] auto owner_id() const noexcept -> uint32_t {
        return static_cast<uint32_t>((value >> 32) & 0x7FFFFFFF);
    }
    [[nodiscard]] auto local_id() const noexcept -> uint32_t {
        return static_cast<uint32_t>(value & 0xFFFFFFFF);
    }

    static constexpr uint64_t SERVER_OWNED_BIT = 1ULL << 63;

    /// Создаёт серверный ID
    [[nodiscard]] static auto server(uint32_t id) noexcept -> NetworkID {
        return {SERVER_OWNED_BIT | (static_cast<uint64_t>(id) << 32)};
    }

    /// Создаёт клиентский ID
    [[nodiscard]] static auto client(uint32_t client_id, uint32_t local_id) noexcept -> NetworkID {
        return {(static_cast<uint64_t>(client_id) << 32) | local_id};
    }

    auto operator<=>(NetworkID const&) const = default;
};

/// Тик симуляции для синхронизации
struct SimulationTick {
    uint32_t value{0};
    float timestamp{0.0f};

    [[nodiscard]] auto next() const noexcept -> SimulationTick {
        return {value + 1, timestamp};
    }

    [[nodiscard]] auto is_newer_than(SimulationTick const& other) const noexcept -> bool {
        // Handle wrap-around
        return (value > other.value && value - other.value < 0x80000000) ||
               (value < other.value && other.value - value > 0x80000000);
    }

    auto operator<=>(SimulationTick const&) const = default;
};

/// Результат сравнения тиков
export enum class TickComparison : int8_t {
    Older = -1,
    Same = 0,
    Newer = 1,
    Invalid = -2  // Wrap-around boundary
};

/// Тип репликации
export enum class ReplicationMode : uint8_t {
    None = 0,           ///< Не реплицируется
    ServerToClient,     ///< Только сервер → клиент
    ClientToServer,     ///< Только клиент → сервер (input)
    Bidirectional       ///< Оба направления
};

/// Приоритет репликации
export enum class ReplicationPriority : uint8_t {
    Low = 0,            ///< Редко обновляется (статичные объекты)
    Normal = 1,         ///< Стандартная частота
    High = 2,           ///< Частые обновления (игроки)
    Critical = 3        ///< Каждый кадр (controlled entity)
};

} // namespace projectv::ecs::net
```

### 1.2 History Buffer for Rollback

```cpp
export namespace projectv::ecs::net {

/// Кольцевой буфер истории состояний для rollback
template<typename T, size_t Capacity = 64>
class HistoryBuffer {
public:
    static_assert(std::has_single_bit(Capacity), "Capacity must be power of 2");
    static constexpr size_t MASK = Capacity - 1;

    HistoryBuffer() = default;

    /// Добавляет состояние для тика.
    auto push(SimulationTick tick, T const& state) noexcept -> void {
        size_t idx = tick.value & MASK;
        states_[idx] = state;
        ticks_[idx] = tick.value;

        if (head_ == UINT32_MAX || tick.is_newer_than(SimulationTick{head_, 0.0f})) {
            head_ = tick.value;
        }
    }

    /// Получает состояние для тика.
    [[nodiscard]] auto get(SimulationTick tick) const noexcept -> std::optional<std::reference_wrapper<T const>> {
        size_t idx = tick.value & MASK;
        if (ticks_[idx] == tick.value) {
            return states_[idx];
        }
        return std::nullopt;
    }

    /// Получает состояние для тика (mutable, для rollback).
    [[nodiscard]] auto get_mut(SimulationTick tick) noexcept -> std::optional<std::reference_wrapper<T>> {
        size_t idx = tick.value & MASK;
        if (ticks_[idx] == tick.value) {
            return states_[idx];
        }
        return std::nullopt;
    }

    /// Проверяет наличие состояния для тика.
    [[nodiscard]] auto contains(SimulationTick tick) const noexcept -> bool {
        return ticks_[tick.value & MASK] == tick.value;
    }

    /// Возвращает последний тик.
    [[nodiscard]] auto latest_tick() const noexcept -> SimulationTick {
        return {head_, 0.0f};
    }

    /// Очищает буфер.
    auto clear() noexcept -> void {
        std::fill_n(ticks_.data(), Capacity, UINT32_MAX);
        head_ = UINT32_MAX;
    }

private:
    std::array<T, Capacity> states_{};
    std::array<uint32_t, Capacity> ticks_{};  // tick values for validation
    uint32_t head_{UINT32_MAX};
};

/// Специализация для Transform
using TransformHistory = HistoryBuffer<glm::vec3, 32>;

/// Специализация для Input
struct PlayerInputState {
    glm::vec3 movement{0.0f};
    glm::vec2 look{0.0f, 0.0f};
    uint16_t buttons{0};  // Bitfield: jump, attack, interact, etc.

    auto button_pressed(uint8_t index) const noexcept -> bool {
        return (buttons & (1 << index)) != 0;
    }
};

using InputHistory = HistoryBuffer<PlayerInputState, 64>;

} // namespace projectv::ecs::net
```

---

## 2. Network-Aware Components

### 2.1 Network Identity Component

```cpp
export module ProjectV.ECS.NetworkComponents;

import std;
import glm;
import glaze;
import ProjectV.ECS.NetworkTypes;
import ProjectV.ECS.Components;  // TransformComponent, etc.

export namespace projectv::ecs {

/// Компонент для идентификации в сети
struct NetworkIdentityComponent {
    net::NetworkID net_id;
    net::ReplicationMode replication_mode{net::ReplicationMode::ServerToClient};
    net::ReplicationPriority priority{net::ReplicationPriority::Normal};

    /// Владелец (client_id или 0 для сервера)
    uint32_t owner_client_id{0};

    /// Тик последнего обновления
    net::SimulationTick last_update_tick;

    /// Является ли этот entity локально управляемым
    [[nodiscard]] auto is_locally_controlled(uint32_t local_client_id) const noexcept -> bool {
        return owner_client_id == local_client_id;
    }

    /// Является ли этот entity серверным
    [[nodiscard]] auto is_server_entity() const noexcept -> bool {
        return net_id.is_server_owned();
    }

    struct glaze {
        using T = NetworkIdentityComponent;
        static constexpr auto value = glz::object(
            "net_id", &T::net_id,
            "replication_mode", &T::replication_mode,
            "priority", &T::priority,
            "owner_client_id", &T::owner_client_id
        );
    };
};

/// Расширенный Transform с поддержкой сети
struct NetworkTransformComponent {
    net::NetworkID net_id;

    // Текущее состояние
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Для интерполяции
    glm::vec3 prev_position{0.0f, 0.0f, 0.0f};
    glm::quat prev_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float interp_alpha{0.0f};

    // Для предсказания
    net::TransformHistory position_history;
    net::SimulationTick last_acknowledged_tick;

    // Флаги
    bool is_dirty{true};
    bool needs_reconciliation{false};

    /// Интерполирует между предыдущим и текущим состоянием.
    [[nodiscard]] auto interpolated_position() const noexcept -> glm::vec3 {
        return glm::mix(prev_position, position, interp_alpha);
    }

    [[nodiscard]] auto interpolated_rotation() const noexcept -> glm::quat {
        return glm::slerp(prev_rotation, rotation, interp_alpha);
    }

    /// Сохраняет текущее состояние как предыдущее.
    auto snapshot_previous() noexcept -> void {
        prev_position = position;
        prev_rotation = rotation;
        interp_alpha = 0.0f;
    }

    /// Вычисляет ошибку между локальным и серверным состоянием.
    [[nodiscard]] auto position_error(glm::vec3 server_position) const noexcept -> float {
        return glm::distance(position, server_position);
    }
};

} // namespace projectv::ecs
```

### 2.2 Input Component with History

```cpp
export namespace projectv::ecs {

/// Сетевой ввод игрока
struct NetworkInputComponent {
    net::NetworkID net_id;

    /// История ввода для предсказания
    net::InputHistory input_history;

    /// Текущий ввод
    net::PlayerInputState current_input;

    /// Номер последовательности
    uint32_t sequence_number{0};

    /// Тик, до которого сервер подтвердил ввод
    net::SimulationTick last_acknowledged_tick;

    /// Добавляет ввод в историю.
    auto push_input(net::SimulationTick tick, net::PlayerInputState const& input) noexcept -> void {
        input_history.push(tick, input);
        current_input = input;
        ++sequence_number;
    }

    /// Получает неподтверждённые вводы для повторной отправки.
    [[nodiscard]] auto get_unacknowledged_inputs(
        net::SimulationTick from_tick,
        net::SimulationTick to_tick
    ) const noexcept -> std::vector<net::PlayerInputState> {
        std::vector<net::PlayerInputState> result;
        for (uint32_t t = from_tick.value; t <= to_tick.value; ++t) {
            auto input = input_history.get(net::SimulationTick{t, 0.0f});
            if (input) {
                result.push_back(input->get());
            }
        }
        return result;
    }
};

} // namespace projectv::ecs
```

### 2.3 Replication Flags Component

```cpp
export namespace projectv::ecs {

/// Флаги для выборочной репликации полей компонента
struct ReplicationFlagsComponent {
    uint32_t transform_flags{0};
    uint32_t velocity_flags{0};
    uint32_t custom_flags{0};

    // Transform flags
    static constexpr uint32_t POSITION_CHANGED = 1 << 0;
    static constexpr uint32_t ROTATION_CHANGED = 1 << 1;
    static constexpr uint32_t SCALE_CHANGED = 1 << 2;

    /// Пометить transform как изменённый.
    auto mark_transform_dirty() noexcept -> void {
        transform_flags |= POSITION_CHANGED | ROTATION_CHANGED | SCALE_CHANGED;
    }

    /// Очистить флаги после отправки.
    auto clear_flags() noexcept -> void {
        transform_flags = 0;
        velocity_flags = 0;
        custom_flags = 0;
    }

    /// Проверить, есть ли изменения.
    [[nodiscard]] auto has_changes() const noexcept -> bool {
        return transform_flags != 0 || velocity_flags != 0 || custom_flags != 0;
    }
};

} // namespace projectv::ecs
```

---

## 3. Prediction & Reconciliation

### 3.1 Client Prediction System

```cpp
export module ProjectV.ECS.NetworkSystems;

import std;
import flecs;
import ProjectV.ECS.NetworkTypes;
import ProjectV.ECS.NetworkComponents;

export namespace projectv::ecs::net {

/// Система предсказания на клиенте
class ClientPredictionSystem {
public:
    /// Порог для reconciliation (в метрах)
    static constexpr float RECONCILIATION_THRESHOLD = 0.5f;

    static auto register_system(flecs::world& world, uint32_t local_client_id) -> void {
        // Система предсказания движения
        world.system<NetworkIdentityComponent, NetworkTransformComponent, NetworkInputComponent>
            ("ClientPrediction")
            .kind(flecs::OnUpdate)
            .iter([local_client_id](flecs::iter& it,
                 NetworkIdentityComponent* identity,
                 NetworkTransformComponent* transform,
                 NetworkInputComponent* input) {

                for (auto i : it) {
                    // Только для локально управляемых entity
                    if (!identity[i].is_locally_controlled(local_client_id)) {
                        continue;
                    }

                    // Сохраняем текущую позицию в историю
                    auto current_tick = get_current_tick(it.world());
                    input[i].input_history.push(current_tick, input[i].current_input);
                    transform[i].position_history.push(current_tick, transform[i].position);

                    // Предсказываем новую позицию
                    predict_movement(transform[i], input[i].current_input, it.delta_time());
                }
            });

        // Система reconciliation при получении серверного обновления
        world.system<NetworkIdentityComponent, NetworkTransformComponent>
            ("ClientReconciliation")
            .kind(flecs::PostUpdate)
            .iter([local_client_id](flecs::iter& it,
                 NetworkIdentityComponent* identity,
                 NetworkTransformComponent* transform) {

                for (auto i : it) {
                    if (!identity[i].is_locally_controlled(local_client_id)) {
                        continue;
                    }

                    if (!transform[i].needs_reconciliation) {
                        continue;
                    }

                    // Сравниваем предсказание с сервером
                    auto server_state = get_last_server_state(identity[i].net_id);
                    if (!server_state) continue;

                    float error = transform[i].position_error(server_state->position);

                    if (error > RECONCILIATION_THRESHOLD) {
                        // Откат к серверному состоянию
                        transform[i].position = server_state->position;
                        transform[i].rotation = server_state->rotation;

                        // Переигрываем неподтверждённые вводы
                        replay_inputs(transform[i], transform[i].last_acknowledged_tick);
                    }

                    transform[i].needs_reconciliation = false;
                }
            });
    }

private:
    static auto predict_movement(
        NetworkTransformComponent& transform,
        PlayerInputState const& input,
        float delta_time
    ) noexcept -> void {
        // Простое движение (без физики для примера)
        float speed = 5.0f;  // TODO: get from component

        transform.position.x += input.movement.x * speed * delta_time;
        transform.position.z += input.movement.z * speed * delta_time;

        // Прыжок
        if (input.button_pressed(0)) {  // Jump button
            transform.position.y += 0.1f;  // Simplified
        }
    }

    static auto replay_inputs(
        NetworkTransformComponent& transform,
        net::SimulationTick from_tick
    ) noexcept -> void {
        // Переигрываем все вводы после from_tick
        // TODO: implement with input history
    }
};

} // namespace projectv::ecs::net
```

### 3.2 Server Authority System

```cpp
export namespace projectv::ecs::net {

/// Система серверной авторитарности
class ServerAuthoritySystem {
public:
    static auto register_system(flecs::world& world) -> void {
        // Обработка клиентского ввода
        world.system<NetworkIdentityComponent, NetworkInputComponent, NetworkTransformComponent>
            ("ProcessClientInput")
            .kind(flecs::PreUpdate)
            .iter([](flecs::iter& it,
                 NetworkIdentityComponent* identity,
                 NetworkInputComponent* input,
                 NetworkTransformComponent* transform) {

                for (auto i : it) {
                    // Только для клиентских entity
                    if (identity[i].is_server_entity()) continue;

                    // Валидация ввода
                    if (!validate_input(input[i].current_input)) {
                        // Подозрительный ввод — логируем
                        continue;
                    }

                    // Применяем ввод к серверному состоянию
                    apply_input(transform[i], input[i].current_input, it.delta_time());
                }
            });

        // Отправка обновлений клиентам
        world.system<>("BroadcastEntityUpdates")
            .kind(flecs::PostUpdate)
            .iter([](flecs::iter& it) {
                auto* net_manager = it.world().ctx<NetworkManager>();
                if (!net_manager) return;

                // Собираем изменённые entity
                it.world().query<NetworkIdentityComponent, NetworkTransformComponent, ReplicationFlagsComponent>()
                    .iter([&net_manager](flecs::iter& qit,
                         NetworkIdentityComponent* identity,
                         NetworkTransformComponent* transform,
                         ReplicationFlagsComponent* flags) {

                        for (auto i : qit) {
                            if (!flags[i].has_changes()) continue;

                            // Отправляем дельту
                            net_manager->broadcast_entity_update(
                                identity[i].net_id,
                                transform[i],
                                flags[i]
                            );

                            flags[i].clear_flags();
                        }
                    });
            });
    }

private:
    static auto validate_input(PlayerInputState const& input) noexcept -> bool {
        // Проверка скорости движения
        float movement_len = glm::length(input.movement);
        if (movement_len > 1.0f + EPSILON) {
            return false;  // Speed hack
        }

        // Дополнительные проверки...
        return true;
    }

    static constexpr float EPSILON = 0.01f;

    static auto apply_input(
        NetworkTransformComponent& transform,
        PlayerInputState const& input,
        float delta_time
    ) noexcept -> void {
        // Серверное применение ввода
        float speed = 5.0f;
        transform.position += input.movement * speed * delta_time;
    }
};

} // namespace projectv::ecs::net
```

---

## 4. Serialization & Bandwidth

### 4.1 Delta Compression

```cpp
export namespace projectv::ecs::net {

/// Сериализатор дельт для минимального bandwidth
class DeltaSerializer {
public:
    /// Сериализует дельту transform.
    /// Использует variable-length encoding для позиций.
    [[nodiscard]] static auto serialize_transform_delta(
        NetworkTransformComponent const& current,
        NetworkTransformComponent const& baseline,
        ReplicationFlagsComponent const& flags
    ) noexcept -> std::vector<uint8_t> {

        std::vector<uint8_t> result;

        // Header: flags (1 byte)
        result.push_back(static_cast<uint8_t>(flags.transform_flags));

        if (flags.transform_flags & ReplicationFlagsComponent::POSITION_CHANGED) {
            // Delta position (relative to baseline)
            glm::vec3 delta = current.position - baseline.position;
            serialize_vle(result, delta.x);
            serialize_vle(result, delta.y);
            serialize_vle(result, delta.z);
        }

        if (flags.transform_flags & ReplicationFlagsComponent::ROTATION_CHANGED) {
            // Smallest three encoding for quaternion
            serialize_smallest_three(result, current.rotation);
        }

        return result;
    }

    /// Десериализует дельту.
    [[nodiscard]] static auto deserialize_transform_delta(
        std::span<const uint8_t> data,
        NetworkTransformComponent const& baseline
    ) noexcept -> std::expected<NetworkTransformComponent, DeserializeError> {

        NetworkTransformComponent result = baseline;
        size_t offset = 0;

        // Header
        uint8_t flags = data[offset++];

        if (flags & ReplicationFlagsComponent::POSITION_CHANGED) {
            result.position.x = baseline.position.x + deserialize_vle<float>(data, offset);
            result.position.y = baseline.position.y + deserialize_vle<float>(data, offset);
            result.position.z = baseline.position.z + deserialize_vle<float>(data, offset);
        }

        if (flags & ReplicationFlagsComponent::ROTATION_CHANGED) {
            result.rotation = deserialize_smallest_three(data, offset);
        }

        return result;
    }

private:
    // Variable-length encoding for floats
    static auto serialize_vle(std::vector<uint8_t>& out, float value) noexcept -> void {
        // Quantize to 16-bit fixed point
        int16_t quantized = static_cast<int16_t>(value * 100.0f);

        // ZigZag encoding
        uint16_t zigzag = (quantized >> 15) ^ (quantized << 1);

        // Variable-length
        if (zigzag < 0x80) {
            out.push_back(static_cast<uint8_t>(zigzag));
        } else {
            out.push_back(static_cast<uint8_t>(0x80 | (zigzag & 0x7F)));
            out.push_back(static_cast<uint8_t>(zigzag >> 7));
        }
    }

    template<typename T>
    static auto deserialize_vle(std::span<const uint8_t> data, size_t& offset) noexcept -> float {
        uint16_t zigzag = data[offset++];
        if (zigzag & 0x80) {
            zigzag = (zigzag & 0x7F) | (static_cast<uint16_t>(data[offset++]) << 7);
        }

        // ZigZag decode
        int16_t quantized = static_cast<int16_t>((zigzag >> 1) ^ -(zigzag & 1));

        return static_cast<float>(quantized) / 100.0f;
    }

    // Smallest three encoding for quaternions
    static auto serialize_smallest_three(std::vector<uint8_t>& out, glm::quat const& q) noexcept -> void;
    static auto deserialize_smallest_three(std::span<const uint8_t> data, size_t& offset) noexcept -> glm::quat;
};

export enum class DeserializeError : uint8_t {
    InvalidFlags,
    BufferOverflow,
    InvalidData
};

} // namespace projectv::ecs::net
```

### 4.2 Bandwidth Budget

```cpp
export namespace projectv::ecs::net {

/// Менеджер bandwidth для приоритизации
class BandwidthManager {
public:
    static constexpr size_t TICK_BUDGET_BYTES = 16 * 1024;  // 16 KB/tick
    static constexpr size_t CLIENT_BUDGET_BYTES = 100 * 1024;  // 100 KB/s per client

    /// Вычисляет приоритет репликации для entity.
    [[nodiscard]] static auto calculate_priority(
        NetworkIdentityComponent const& identity,
        NetworkTransformComponent const& transform,
        glm::vec3 camera_position,
        float time_since_last_update
    ) noexcept -> float {

        float priority = 0.0f;

        // Базовый приоритет из компонента
        priority += static_cast<float>(identity.priority) * 10.0f;

        // Расстояние до камеры
        float distance = glm::distance(transform.position, camera_position);
        float distance_factor = 1.0f / (1.0f + distance * 0.1f);
        priority += distance_factor * 5.0f;

        // Время с последнего обновления
        priority += time_since_last_update * 2.0f;

        // Владелец получает высший приоритет
        if (identity.is_client_owned()) {
            priority *= 1.5f;
        }

        return priority;
    }

    /// Сортирует entity по приоритету и выбирает для репликации.
    [[nodiscard]] static auto select_for_replication(
        std::vector<std::pair<flecs::entity, float>> priorities,
        size_t bandwidth_budget
    ) noexcept -> std::vector<flecs::entity> {

        // Сортировка по убыванию приоритета
        std::sort(priorities.begin(), priorities.end(),
                  [](auto const& a, auto const& b) {
                      return a.second > b.second;
                  });

        std::vector<flecs::entity> selected;
        size_t used_bandwidth = 0;

        for (auto const& [entity, priority] : priorities) {
            // Оценка размера обновления
            size_t estimated_size = estimate_update_size(entity);

            if (used_bandwidth + estimated_size <= bandwidth_budget) {
                selected.push_back(entity);
                used_bandwidth += estimated_size;
            }
        }

        return selected;
    }

private:
    static auto estimate_update_size(flecs::entity entity) noexcept -> size_t {
        // Примерная оценка: NetworkID (8) + Position delta (6) + Rotation (4) + Flags (1)
        return 20;
    }
};

} // namespace projectv::ecs::net
```

---

## 5. Protocol Specification

### 5.1 Packet Types

```cpp
export namespace projectv::ecs::net {

/// Типы пакетов
export enum class PacketType : uint8_t {
    // Client → Server
    ClientInput       = 0x01,   ///< Ввод игрока
    ClientAcknowledge = 0x02,   ///< Подтверждение получения
    ClientRequest     = 0x03,   ///< Запрос (spawn, interact)

    // Server → Client
    ServerState       = 0x10,   ///< Состояние entity
    ServerDelta       = 0x11,   ///< Дельта состояния
    ServerEvent       = 0x12,   ///< Игровое событие
    ServerSpawn       = 0x13,   ///< Создание entity
    ServerDestroy     = 0x14,   ///< Удаление entity

    // Both directions
    Ping              = 0x20,
    Pong              = 0x21
};

/// Заголовок пакета
struct alignas(4) PacketHeader {
    uint16_t sequence;      ///< Номер последовательности
    uint16_t ack;           ///< Последний полученный пакет
    uint32_t ack_bitfield;  ///< Bitfield последних 32 ack
    PacketType type;
    uint8_t flags;
    uint16_t payload_size;
};

static_assert(sizeof(PacketHeader) == 12);

} // namespace projectv::ecs::net
```

---

## Статус

| Компонент                 | Статус         | Приоритет |
|---------------------------|----------------|-----------|
| NetworkID                 | Специфицирован | P0        |
| SimulationTick            | Специфицирован | P0        |
| HistoryBuffer             | Специфицирован | P0        |
| NetworkIdentityComponent  | Специфицирован | P0        |
| NetworkTransformComponent | Специфицирован | P0        |
| NetworkInputComponent     | Специфицирован | P0        |
| ClientPredictionSystem    | Специфицирован | P1        |
| ServerAuthoritySystem     | Специфицирован | P1        |
| DeltaSerializer           | Специфицирован | P1        |
| BandwidthManager          | Специфицирован | P2        |
| Protocol                  | Специфицирован | P1        |
