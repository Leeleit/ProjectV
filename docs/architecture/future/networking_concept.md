# Networking Concept: Multiplayer Architecture

Высокоуровневая концепция мультиплеера для ProjectV.

---

## Важное примечание

**Данный документ описывает архитектуру Post-MVP.** Мультиплеер НЕ входит в курсовой проект и будет реализован после
успешной защиты.

См. [Roadmap & Scope](../../architecture/roadmap_and_scope.md) для границ MVP.

---

## Проблема: Синхронизация воксельного мира

Воксельный мир создаёт уникальные вызовы для мультиплеера:

| Вызов                      | Сложность                           |
|----------------------------|-------------------------------------|
| **Миллионы вокселей**      | Невозможно синхронизировать всё     |
| **Динамические изменения** | Разрушения меняют мир постоянно     |
| **Физика**                 | Синхронизация физического состояния |
| **Пропускная способность** | Ограничения сети                    |

---

## Архитектура: Server Authority + Client Prediction

### Обзор

```
┌─────────────────────────────────────────────────────────────────┐
│                    Multiplayer Architecture                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                      SERVER                              │   │
│  │                                                         │   │
│  │  ✓ Authoritative state (воксели, физика, entities)      │   │
│  │  ✓ Voxel world storage                                  │   │
│  │  ✓ Physics simulation                                   │   │
│  │  ✓ AI processing                                        │   │
│  │  ✓ Anti-cheat validation                                │   │
│  │                                                         │   │
│  │  Outgoing: Delta updates, entity state, events           │   │
│  │  Incoming: Client input, requests                        │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│                        ┌─────────┐                             │
│                        │ Network │                             │
│                        │ (UDP)   │                             │
│                        └─────────┘                             │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                      CLIENT                              │   │
│  │                                                         │   │
│  │  ✓ Prediction (movement, physics)                        │   │
│  │  ✓ Interpolation (другие игроки)                         │   │
│  │  ✓ Local voxel cache                                     │   │
│  │  ✓ Rendering                                             │   │
│  │                                                         │   │
│  │  Outgoing: Input, requests                               │   │
│  │  Incoming: State updates, voxel deltas                   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. Server Authority

### Принципы

1. **Сервер — источник истины** для всего состояния мира
2. **Клиент только предсказывает** своё состояние
3. **Все изменения вокселей** проходят через сервер
4. **Физика** выполняется на сервере

### Структура сервера

```cpp
namespace projectv::net {

class VoxelGameServer {
public:
    void tick(float deltaTime) {
        // 1. Обработка входящих пакетов
        processIncomingPackets();

        // 2. Обработка клиентского ввода
        processClientInputs();

        // 3. Симуляция физики
        physicsSystem_.update(deltaTime);

        // 4. Обновление AI
        aiSystem_.update(deltaTime);

        // 5. Проверка изменений вокселей
        processVoxelChanges();

        // 6. Отправка обновлений клиентам
        broadcastUpdates();
    }

private:
    // Состояние мира
    VoxelWorld world_;
    JoltPhysicsBridge physicsSystem_;
    AISystem aiSystem_;

    // Клиенты
    std::unordered_map<ClientID, ClientState> clients_;

    // Сеть
    UDPSocket socket_;
    PacketSerializer serializer_;

    void processIncomingPackets() {
        while (auto packet = socket_.receive()) {
            auto clientIt = clients_.find(packet->clientId);
            if (clientIt == clients_.end()) continue;

            switch (packet->type) {
                case PacketType::ClientInput:
                    handleClientInput(clientIt->second, packet->data);
                    break;

                case PacketType::VoxelModification:
                    handleVoxelModification(clientIt->second, packet->data);
                    break;

                case PacketType::ChatMessage:
                    handleChatMessage(clientIt->second, packet->data);
                    break;
            }
        }
    }

    void broadcastUpdates() {
        // 1. Entity updates (позиции, состояния)
        broadcastEntityUpdates();

        // 2. Voxel deltas
        broadcastVoxelDeltas();

        // 3. Events
        broadcastEvents();
    }
};

} // namespace projectv::net
```

---

## 2. Client Prediction

### Принцип

Клиент **предсказывает** результат своих действий, не дожидаясь сервера:

```cpp
namespace projectv::net {

class ClientPrediction {
public:
    void processInput(const PlayerInput& input) {
        // 1. Сохраняем ввод для reconciliation
        inputHistory_.push_back({
            .sequence = ++sequenceNumber_,
            .input = input,
            .timestamp = getCurrentTime()
        });

        // 2. Предсказываем результат локально
        predictedState_ = simulatePhysics(predictedState_, input);

        // 3. Отправляем ввод на сервер
        sendInputToServer(sequenceNumber_, input);
    }

    void onServerUpdate(uint32_t acknowledgedSequence,
                        const PlayerState& serverState) {
        // 1. Удаляем подтверждённые вводы
        while (!inputHistory_.empty() &&
               inputHistory_.front().sequence <= acknowledgedSequence) {
            inputHistory_.pop_front();
        }

        // 2. Сравниваем с серверным состоянием
        float error = calculateError(predictedState_, serverState);

        // 3. Если ошибка велика — корректируем
        if (error > RECONCILIATION_THRESHOLD) {
            reconcile(serverState);
        }
    }

private:
    struct InputRecord {
        uint32_t sequence;
        PlayerInput input;
        float timestamp;
    };

    uint32_t sequenceNumber_ = 0;
    PlayerState predictedState_;
    std::deque<InputRecord> inputHistory_;

    static constexpr float RECONCILIATION_THRESHOLD = 0.5f;  // метры

    void reconcile(const PlayerState& serverState) {
        // 1. Откатываемся к серверному состоянию
        predictedState_ = serverState;

        // 2. Переигрываем все неподтверждённые вводы
        for (const auto& record : inputHistory_) {
            predictedState_ = simulatePhysics(predictedState_, record.input);
        }
    }

    PlayerState simulatePhysics(PlayerState state, const PlayerInput& input) {
        // Упрощённая физика для предсказания
        state.velocity += input.movement * ACCELERATION * TICK_RATE;
        state.position += state.velocity * TICK_RATE;

        // Применяем трение
        state.velocity *= FRICTION;

        return state;
    }
};

} // namespace projectv::net
```

### Entity Interpolation

Для других игроков клиент **интерполирует** между полученными состояниями:

```cpp
namespace projectv::net {

class EntityInterpolator {
public:
    void onEntityUpdate(EntityID id, const EntityState& state, float timestamp) {
        auto& buffer = entityBuffers_[id];

        // Добавляем состояние в буфер
        buffer.push_back({timestamp, state});

        // Ограничиваем размер буфера
        while (buffer.size() > BUFFER_SIZE) {
            buffer.pop_front();
        }
    }

    EntityState interpolate(EntityID id, float renderTime) {
        const auto& buffer = entityBuffers_[id];

        // Время рендеринга с задержкой
        float interpolationTime = renderTime - INTERPOLATION_DELAY;

        // Находим два состояния для интерполяции
        for (size_t i = 0; i < buffer.size() - 1; ++i) {
            if (buffer[i].timestamp <= interpolationTime &&
                buffer[i + 1].timestamp >= interpolationTime) {

                float alpha = (interpolationTime - buffer[i].timestamp) /
                             (buffer[i + 1].timestamp - buffer[i].timestamp);

                return interpolateStates(buffer[i].state,
                                        buffer[i + 1].state,
                                        alpha);
            }
        }

        // Если не нашли — возвращаем последнее
        return buffer.back().state;
    }

private:
    struct TimedState {
        float timestamp;
        EntityState state;
    };

    std::unordered_map<EntityID, std::deque<TimedState>> entityBuffers_;

    static constexpr size_t BUFFER_SIZE = 20;
    static constexpr float INTERPOLATION_DELAY = 0.1f;  // 100ms

    EntityState interpolateStates(const EntityState& a,
                                  const EntityState& b,
                                  float alpha) {
        EntityState result;
        result.position = glm::mix(a.position, b.position, alpha);
        result.rotation = glm::slerp(a.rotation, b.rotation, alpha);
        result.velocity = glm::mix(a.velocity, b.velocity, alpha);
        return result;
    }
};

} // namespace projectv::net
```

---

## 3. Voxel Synchronization

### Проблема

- Мир: 1 км³ = ~30 млрд вокселей
- Изменения: Тысячи в секунду при взрывах
- Бандwidth: Ограничен

### Решение: Delta Compression

```cpp
namespace projectv::net {

class VoxelSyncManager {
public:
    struct VoxelDelta {
        glm::ivec3 position;
        VoxelData oldValue;
        VoxelData newValue;
        float timestamp;
    };

    // На сервере: сбор изменений
    void collectChanges(const VoxelWorld& world, float deltaTime) {
        for (const auto& change : world.getRecentChanges()) {
            deltas_.push_back({
                .position = change.position,
                .oldValue = change.oldValue,
                .newValue = change.newValue,
                .timestamp = getCurrentTime()
            });
        }
    }

    // Сжатие дельт для отправки
    std::vector<uint8_t> compressDeltas(const std::vector<VoxelDelta>& deltas) {
        std::vector<uint8_t> result;

        // 1. Группировка по чанкам
        auto chunkGroups = groupByChunk(deltas);

        // 2. RLE для позиций
        for (const auto& [chunkPos, chunkDeltas] : chunkGroups) {
            // Заголовок чанка
            serialize(result, chunkPos);
            serialize(result, static_cast<uint16_t>(chunkDeltas.size()));

            // Дельты в относительных координатах
            glm::ivec3 lastPos{0, 0, 0};
            for (const auto& delta : chunkDeltas) {
                glm::ivec3 localPos = delta.position - chunkPos * CHUNK_SIZE;
                glm::ivec3 deltaPos = localPos - lastPos;

                // Delta encoding для позиций
                serializeVLE(result, deltaPos.x);
                serializeVLE(result, deltaPos.y);
                serializeVLE(result, deltaPos.z);

                // Новое значение
                serialize(result, delta.newValue);

                lastPos = localPos;
            }
        }

        // 3. Zstd сжатие
        result = zstdCompress(result, 3);

        return result;
    }

    // На клиенте: применение дельт
    void applyDeltas(VoxelWorld& world, const std::vector<uint8_t>& data) {
        auto decompressed = zstdDecompress(data);

        // Десериализация и применение
        size_t offset = 0;
        while (offset < decompressed.size()) {
            glm::ivec3 chunkPos = deserialize<glm::ivec3>(decompressed, offset);
            uint16_t count = deserialize<uint16_t>(decompressed, offset);

            glm::ivec3 lastPos{0, 0, 0};
            for (uint16_t i = 0; i < count; ++i) {
                glm::ivec3 deltaPos;
                deltaPos.x = deserializeVLE<int32_t>(decompressed, offset);
                deltaPos.y = deserializeVLE<int32_t>(decompressed, offset);
                deltaPos.z = deserializeVLE<int32_t>(decompressed, offset);

                glm::ivec3 localPos = lastPos + deltaPos;
                glm::ivec3 worldPos = chunkPos * CHUNK_SIZE + localPos;

                VoxelData newValue = deserialize<VoxelData>(decompressed, offset);

                world.setVoxel(worldPos, newValue);
                lastPos = localPos;
            }
        }
    }

private:
    std::vector<VoxelDelta> deltas_;

    std::unordered_map<glm::ivec3, std::vector<VoxelDelta>>
    groupByChunk(const std::vector<VoxelDelta>& deltas) {
        std::unordered_map<glm::ivec3, std::vector<VoxelDelta>> groups;
        for (const auto& delta : deltas) {
            glm::ivec3 chunkPos = delta.position / CHUNK_SIZE;
            groups[chunkPos].push_back(delta);
        }
        return groups;
    }
};

} // namespace projectv::net
```

### Приоритезация обновлений

```cpp
namespace projectv::net {

class VoxelUpdatePrioritizer {
public:
    struct PrioritizedUpdate {
        glm::ivec3 position;
        float priority;
        float lastSent;
    };

    void calculatePriorities(const std::vector<ClientState>& clients) {
        for (auto& update : pendingUpdates_) {
            update.priority = 0.0f;

            for (const auto& client : clients) {
                // Расстояние до игрока
                float distance = glm::distance(
                    glm::vec3(update.position),
                    client.position
                );

                // Ближе = выше приоритет
                float distancePriority = 1.0f / (1.0f + distance * 0.01f);

                // Время с последней отправки
                float timeSinceSent = getCurrentTime() - update.lastSent;
                float stalenessPriority = timeSinceSent * 0.5f;

                // Видимость для игрока
                float visibilityPriority = isVisible(update.position, client) ? 2.0f : 1.0f;

                update.priority += distancePriority * stalenessPriority * visibilityPriority;
            }
        }

        // Сортировка по приоритету
        std::sort(pendingUpdates_.begin(), pendingUpdates_.end(),
                  [](const auto& a, const auto& b) {
                      return a.priority > b.priority;
                  });
    }

    std::vector<VoxelDelta> getTopUpdates(size_t maxCount) {
        std::vector<VoxelDelta> result;
        for (size_t i = 0; i < std::min(maxCount, pendingUpdates_.size()); ++i) {
            result.push_back(pendingUpdates_[i]);
            pendingUpdates_[i].lastSent = getCurrentTime();
        }
        return result;
    }

private:
    std::vector<PrioritizedUpdate> pendingUpdates_;
};

} // namespace projectv::net
```

---

## 4. Bandwidth Optimization

### Статистика

| Метрика        | Значение          | Формула                 |
|----------------|-------------------|-------------------------|
| Entity updates | ~500 bytes/player | 20 players = 10 KB/tick |
| Voxel deltas   | ~50 bytes/change  | 100 changes = 5 KB/tick |
| Input packets  | ~50 bytes/player  | 20 players = 1 KB/tick  |
| **Total**      | ~16 KB/tick       | 20 tick/s = 320 KB/s    |

### Оптимизации

```cpp
namespace projectv::net {

class BandwidthOptimizer {
public:
    // 1. Сжатие пакетов
    std::vector<uint8_t> compressPacket(const std::vector<uint8_t>& data) {
        // Zstd с динамическим уровнем
        int level = (data.size() > 1000) ? 3 : 1;
        return zstdCompress(data, level);
    }

    // 2. Rate limiting
    bool shouldSendUpdate(ClientID client, UpdateType type) {
        auto& limits = rateLimits_[client];

        switch (type) {
            case UpdateType::Position:
                return limits.positionCounter++ % 2 == 0;  // 50%
            case UpdateType::VoxelDelta:
                return limits.voxelBudget > 0;
            case UpdateType::FullState:
                return limits.fullStateTimer.elapsed() > 5.0f;  // 5 секунд
        }

        return true;
    }

    // 3. Бандwidth budgeting
    void allocateBudget(const std::vector<ClientState>& clients) {
        // Общий бюджет: 100 KB/s на клиент
        const size_t TOTAL_BUDGET = 100 * 1024;

        for (auto& [client, limits] : rateLimits_) {
            // Распределение по типам
            limits.voxelBudget = TOTAL_BUDGET * 0.5;   // 50 KB/s
            limits.entityBudget = TOTAL_BUDGET * 0.3;  // 30 KB/s
            limits.eventBudget = TOTAL_BUDGET * 0.2;   // 20 KB/s
        }
    }

private:
    struct RateLimits {
        size_t voxelBudget;
        size_t entityBudget;
        size_t eventBudget;
        int positionCounter = 0;
        Timer fullStateTimer;
    };

    std::unordered_map<ClientID, RateLimits> rateLimits_;
};

} // namespace projectv::net
```

---

## 5. Spatial Partitioning

### Зоны для масштабирования

```cpp
namespace projectv::net {

class SpatialPartitioning {
public:
    // Разделение мира на зоны для распределения нагрузки
    static constexpr int ZONE_SIZE = 256;  // 256³ вокселей

    struct Zone {
        glm::ivec3 position;
        std::vector<ClientID> clients;
        std::vector<EntityID> entities;
        std::vector<glm::ivec3> dirtyChunks;
    };

    void updateZones() {
        // 1. Распределяем клиентов по зонам
        for (auto& [id, client] : clients_) {
            glm::ivec3 zonePos = client.position / (float)ZONE_SIZE;
            zones_[zonePos].clients.push_back(id);
        }

        // 2. Определяем interest sets
        for (auto& [id, client] : clients_) {
            client.interestZones = getNearbyZones(client.position, 2);  // 2 зоны вокруг
        }
    }

    // Отправка обновлений только заинтересованным клиентам
    void broadcastToZone(const Zone& zone, const Packet& packet) {
        for (ClientID client : zone.clients) {
            sendPacket(client, packet);
        }

        // Также отправляем клиентам в соседних зонах
        for (const auto& neighborPos : getNeighborZones(zone.position)) {
            for (ClientID client : zones_[neighborPos].clients) {
                // Только если клиент заинтересован
                if (isInterested(client, zone.position)) {
                    sendPacket(client, packet);
                }
            }
        }
    }

private:
    std::unordered_map<glm::ivec3, Zone> zones_;
    std::unordered_map<ClientID, ClientState> clients_;

    std::vector<glm::ivec3> getNearbyZones(const glm::vec3& pos, int radius) {
        std::vector<glm::ivec3> result;
        glm::ivec3 center = pos / (float)ZONE_SIZE;

        for (int x = -radius; x <= radius; ++x) {
            for (int y = -radius; y <= radius; ++y) {
                for (int z = -radius; z <= radius; ++z) {
                    result.push_back(center + glm::ivec3(x, y, z));
                }
            }
        }

        return result;
    }
};

} // namespace projectv::net
```

---

## 6. Anti-Cheat

### Валидация на сервере

```cpp
namespace projectv::net {

class AntiCheat {
public:
    bool validateInput(ClientID client, const PlayerInput& input) {
        const auto& state = getClientState(client);

        // 1. Проверка скорости
        float maxSpeed = state.isSprinting ? SPRINT_SPEED : WALK_SPEED;
        float inputSpeed = glm::length(input.movement);
        if (inputSpeed > maxSpeed * 1.1f) {  // 10% tolerance
            logSuspiciousActivity(client, "Speed hack detected");
            return false;
        }

        // 2. Проверка телепортации
        float distance = glm::distance(input.position, state.position);
        float maxDistance = maxSpeed * TICK_DELTA * 2.0f;  // 2 ticks tolerance
        if (distance > maxDistance) {
            logSuspiciousActivity(client, "Teleport detected");
            return false;
        }

        // 3. Проверка rate of fire
        if (input.flags & InputFlags::Fire) {
            float timeSinceLastShot = getCurrentTime() - state.lastShotTime;
            if (timeSinceLastShot < MIN_FIRE_INTERVAL) {
                logSuspiciousActivity(client, "Rapid fire detected");
                return false;
            }
        }

        return true;
    }

    bool validateVoxelModification(ClientID client,
                                   const glm::ivec3& position,
                                   VoxelOperation operation) {
        const auto& state = getClientState(client);

        // 1. Проверка расстояния
        float distance = glm::distance(glm::vec3(position), state.position);
        if (distance > MAX_INTERACTION_DISTANCE) {
            logSuspiciousActivity(client, "Distance hack detected");
            return false;
        }

        // 2. Проверка линии видимости
        if (!hasLineOfSight(state.position, glm::vec3(position))) {
            logSuspiciousActivity(client, "Wallhack suspected");
            return false;
        }

        // 3. Проверка rate limiting
        auto& modCount = modificationCounts_[client];
        if (modCount > MAX_MODIFICATIONS_PER_SECOND) {
            logSuspiciousActivity(client, "Modification rate exceeded");
            return false;
        }
        ++modCount;

        return true;
    }

private:
    std::unordered_map<ClientID, uint32_t> modificationCounts_;

    void logSuspiciousActivity(ClientID client, const std::string& reason) {
        // Логирование и возможный бан
        // ...
    }
};

} // namespace projectv::net
```

---

## Резюме

### Ключевые решения

| Решение                  | Обоснование                   |
|--------------------------|-------------------------------|
| **Server Authority**     | Безопасность, консистентность |
| **Client Prediction**    | Плавный геймплей без лагов    |
| **Delta Compression**    | Экономия bandwidth            |
| **Spatial Partitioning** | Масштабируемость              |
| **Anti-Cheat**           | Защита от читеров             |

### Фазы реализации (Post-MVP)

1. **Phase 4a**: Базовый сервер + клиент
2. **Phase 4b**: Voxel sync + prediction
3. **Phase 4c**: Optimizations + anti-cheat
4. **Phase 4d**: Dedicated server + matchmaking
