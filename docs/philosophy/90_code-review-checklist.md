# Чек-лист для code review

Документ — обязательный чек-лист для каждого PR. Проверяется до merge.

---

## Базовые проверки

- [ ] Код компилируется без warnings (`-Werror`).
- [ ] Все тесты проходят (`ctest`).
- [ ] Нет новых warnings от clang-tidy.
- [ ] Форматирование соответствует `.clang-format`.
- [ ] Нет отладочного кода (`printf`, `std::cerr`, `assert(false)`).
- [ ] Нет закомментированного кода без обоснования.
- [ ] Commit message соответствует `AGENTS.md §5.1` (или аналогичному
  протоколу).

---

## C++26 специфика

- [ ] Нет `try`/`throw`/`catch` в Runtime коде.
- [ ] Нет `dynamic_cast`, `typeid`.
- [ ] Нет `std::shared_ptr` без обоснования.
- [ ] Нет `std::map`/`std::unordered_map` в hot path.
- [ ] Нет `std::string` в hot path (используется `StringID` или
  `std::string_view`).
- [ ] `[[nodiscard]]` на функциях, возвращающих `Result<T, E>`.
- [ ] `noexcept` на деструкторах, swap, move-операциях.
- [ ] `alignas(std::hardware_destructive_interference_size)` для данных
  разных потоков.
- [ ] `static_assert` на размер и alignment критичных структур.

---

## DOD / Cache

- [ ] Hot path использует SoA layout (или archetype-based ECS).
- [ ] Поля упорядочены по убыванию размера в структурах.
- [ ] Нет pointer chasing в hot path (vec<Entity*> → vec<Entity>).
- [ ] Нет false sharing (atomic fields разнесены по cache lines).
- [ ] Hot и cold данные разделены.
- [ ] `std::span` для non-owning array views.
- [ ] Batch processing вместо per-element loops.

---

## Apple Silicon совместимость

- [ ] Нет hard-coded `alignas(64)` (использовать
  `std::hardware_destructive_interference_size`).
- [ ] Нет x86-specific intrinsics без `#ifdef __APPLE__` fallback.
- [ ] Нет assumptions о 64-байтной cache line.
- [ ] NEON-совместимый SIMD (или `std::simd` для портабельности).

---

## Vulkan / GPU

- [ ] Нет утечек Vulkan handles (VkBuffer, VkImage, VkDeviceMemory).
- [ ] Command buffers не используются после `vkResetCommandPool`.
- [ ] Descriptor sets обновляются через `vkUpdateDescriptorSets`, не
  пересоздаются.
- [ ] Pipeline cache используется для тяжёлых pipelines.
- [ ] Vulkan validation layers clean в Debug.
- [ ] Нет synchronous `vkQueueWaitIdle` в hot path.
- [ ] Resource barriers корректны (через Synchronization2).
- [ ] Timeline semaphores для кросс-очередной синхронизации.

---

## ECS / flecs

- [ ] Компоненты — POD без логики.
- [ ] Системы объявляют read/write зависимости через query API.
- [ ] Нет прямого доступа к `world` из компонентов (только через
  системы).
- [ ] Фаза pipeline указана (`OnLoad`, `OnUpdate`, `OnStore`).
- [ ] Entity lifeliness операции не в multi-threaded system.
- [ ] `ecs_defer_*` используется в readonly mode корректно.

---

## Конкурентность

- [ ] Нет `std::thread` в hot path (только Job System).
- [ ] Нет мьютексов в hot path (lock-free или архитектурное
  избегание).
- [ ] Data race: проверка через ThreadSanitizer в Debug.
- [ ] Job dependencies объявлены в DAG явно.
- [ ] Lock-free структуры через `std::atomic` + CAS.

---

## Память и аллокации

- [ ] Нет `new`/`malloc` в hot path.
- [ ] Кастомные аллокаторы (linear/frame/pool) для hot path данных.
- [ ] `std::pmr::polymorphic_allocator` для контейнеров с custom arena.
- [ ] Heap allocations только на этапе инициализации.
- [ ] Нет утечек памяти (проверка через ASan в Debug).

---

## Обработка ошибок

- [ ] Все ожидаемые ошибки через `std::expected<T, E>`.
- [ ] Инварианты проверяются через `assert` (или `[[pre:...]]` в C++26).
- [ ] Нет silently swallowed exceptions.
- [ ] Ошибки логируются с достаточным контекстом.

---

## Производительность

- [ ] Профилировщик показывает отсутствие regression.
- [ ] Нет новых heap allocations в hot path.
- [ ] Нет redundant work в loop body.
- [ ] Cache locality сохранён или улучшен.
- [ ] Branch prediction не ухудшен.

---

## Тестирование

- [ ] Новые алгоритмы имеют unit-тесты.
- [ ] Баги исправлены regression-тестом.
- [ ] Property-based тесты для нетривиальных алгоритмов.
- [ ] Coverage для изменённого кода не упал.

---

## Документация

- [ ] Engineering contracts обновлены, если введены новые.
- [ ] Roadmap обновлён, если задача завершена.
- [ ] Public API задокументирован в комментариях.

---

## Стиль кода

- [ ] Комментарии — одна строка, после кода.
- [ ] EVIL помечает только злые хаки.
- [ ] Нет magic numbers (именованные константы).
- [ ] `const` для всех неизменяемых переменных.
- [ ] Файлы ≤ 600 строк (где возможно).

---

## Финальная проверка

- [ ] PR description объясняет **что** и **почему**, не **как**.
- [ ] Все комментарии актуальны.
- [ ] Нет temporary debug prints.
- [ ] CI pipeline (build + tests + lint) зелёный.

---

## Пример: применение в движке

В ProjectV этот чек-лист автоматизирован через `scripts/code-review.sh`,
который прогоняет clang-tidy, ASan/UBSan check, тесты на изменённые
файлы. Ручная проверка — по разделам выше.

---

## Источники и дальнейшее чтение

- [10_manifesto.md](10_manifesto.md) — фундаментальные принципы.
- [11_anti-patterns.md](11_anti-patterns.md) — что нельзя.
- [12_decision-making.md](12_decision-making.md) — как принимаются решения.
- [13_evil-hacks.md](13_evil-hacks.md) — когда хаки оправданы.
- [30_optimization.md](30_optimization.md) — иерархия оптимизации.
- [94_build-and-ci.md](94_build-and-ci.md) — CI gates.