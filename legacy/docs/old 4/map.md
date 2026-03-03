# Карта документации ProjectV

**Версия:** 1.0.0
**Последнее обновление:** 22.02.2026

---

## Обзор структуры документации

```
docs/
├── map.md                    # Карта документации (этот файл)
├── README.md                 # Введение в документацию
├── architecture/             # Архитектурная документация
│   ├── README.md
│   ├── 00_roadmap_and_scope.md
│   ├── adr/                 # Architecture Decision Records
│   └── practice/            # Практические руководства
├── philosophy/              # Философия и принципы проекта
├── standards/               # Стандарты разработки
│   ├── cmake/              # Стандарты CMake
│   ├── cpp/                # Стандарты C++
│   └── git/                # Стандарты Git
├── libraries/               # Документация внешних библиотек
└── future/                  # Планы развития
```

---

## Стандарты разработки (СТВ)

### CMake (СТВ-CMAKE)

| Документ                                                                         | Идентификатор | Описание                                      |
|----------------------------------------------------------------------------------|---------------|-----------------------------------------------|
| [Спецификация системы сборки](standards/cmake/00_specification.md)               | СТВ-CMAKE-001 | Обязательные требования к конфигурации CMake  |
| [Стандарт структуры проекта](standards/cmake/01_basics-structure.md)             | СТВ-CMAKE-002 | Структура CMakeLists.txt и организация файлов |
| [Стандарт управления зависимостями](standards/cmake/02_dependencies.md)          | СТВ-CMAKE-003 | Интеграция внешних библиотек, PIMPL-паттерн   |
| [Стандарт конфигурации сборки](standards/cmake/03_build-configuration.md)        | СТВ-CMAKE-004 | Типы сборки, флаги компилятора, санитайзеры   |
| [Стандарт расширенной оптимизации](standards/cmake/04_advanced-optimization.md)  | СТВ-CMAKE-005 | LTO, PGO, unity-сборки                        |
| [Стандарт кросс-платформенной сборки](standards/cmake/05_cross-platform.md)      | СТВ-CMAKE-006 | Поддержка Windows/Linux/macOS                 |
| [Руководство по устранению неполадок](standards/cmake/06_troubleshooting-ide.md) | СТВ-CMAKE-007 | Решение типичных проблем IDE                  |

### C++ (СТВ-CPP)

| Документ                                                             | Идентификатор | Описание                                  |
|----------------------------------------------------------------------|---------------|-------------------------------------------|
| [Стандарт языка C++](standards/cpp/00_language-standard.md)          | СТВ-CPP-001   | C++26, модули, обработка ошибок           |
| [Стандарт структуры кода](standards/cpp/01_code-structure.md)        | СТВ-CPP-002   | Организация файлов, классов, функций      |
| [Стандарт управления памятью](standards/cpp/02_memory-management.md) | СТВ-CPP-003   | Аллокаторы, выравнивание, умные указатели |
| [Границы PIMPL и DOD](standards/cpp/03_pimpl_dod_boundaries.md)      | СТВ-CPP-004   | PIMPL границы, Hot Path правила           |

### Git (СТВ-GIT)

| Документ                                                                   | Идентификатор | Описание                             |
|----------------------------------------------------------------------------|---------------|--------------------------------------|
| [Стандарт контроля версий](standards/git/00_version-control-standard.md)   | СТВ-GIT-001   | Структура репозитория, коммиты, теги |
| [Стандарт стратегии ветвления](standards/git/01_branching-strategy.md)     | СТВ-GIT-002   | Модель ветвления Git Flow            |
| [Стандарт процесса Pull Request](standards/git/02_pull-request-process.md) | СТВ-GIT-003   | Ревью кода, CI/CD, слияние           |

---

## Архитектурная документация

### Architecture Decision Records (ADR)

Решения по архитектуре с обоснованием:

| ADR      | Название                      | Статус    |
|----------|-------------------------------|-----------|
| ADR-0001 | Рендерер Vulkan               | Утверждён |
| ADR-0002 | Стандарт C++26                | Утверждён |
| ADR-0003 | Архитектура ECS               | Утверждён |
| ADR-0004 | Спецификация сборки и модулей | Утверждён |

### Академическая документация (academic/)

Документы для защиты проекта и академических целей:

| Документ                                                                         | Уровень          | Описание                    |
|----------------------------------------------------------------------------------|------------------|-----------------------------|
| [01_project_defense_model.md](architecture/academic/01_project_defense_model.md) | 🎓 Академический | Модель защиты проекта       |
| [02_mvp_defense_demo.md](architecture/academic/02_mvp_defense_demo.md)           | 🎓 Академический | Спецификация MVP демо-сцены |

### Практические руководства (practice/)

Практические руководства по реализации компонентов:

#### Архитектура движка

| Документ                                                                     | Уровень    | Описание                                 |
|------------------------------------------------------------------------------|------------|------------------------------------------|
| [00_engine-structure.md](architecture/practice/00_engine-structure.md)       | 🟢 Базовый | Структура движка и модульная архитектура |
| [01_core-loop.md](architecture/practice/01_core-loop.md)                     | 🟢 Базовый | Основной игровой цикл                    |
| [02_resource-management.md](architecture/practice/02_resource-management.md) | 🟢 Базовый | Управление ресурсами                     |
| [03_voxel-pipeline.md](architecture/practice/03_voxel-pipeline.md)           | 🟢 Базовый | Конвейер обработки вокселей              |

#### Графика и рендеринг

| Документ                                                                     | Уровень        | Описание                      |
|------------------------------------------------------------------------------|----------------|-------------------------------|
| [04_vulkan_spec.md](architecture/practice/04_vulkan_spec.md)                 | 🟡 Средний     | Спецификация Vulkan рендерера |
| [05_flecs-vulkan-bridge.md](architecture/practice/05_flecs-vulkan-bridge.md) | 🟡 Средний     | Интеграция ECS с Vulkan       |
| [06_jolt-vulkan-bridge.md](architecture/practice/06_jolt-vulkan-bridge.md)   | 🟡 Средний     | Интеграция физики с Vulkan    |
| [15_gpu-debugging.md](architecture/practice/15_gpu-debugging.md)             | 🔴 Продвинутый | Отладка GPU с RenderDoc       |
| [20_material-system.md](architecture/practice/20_material-system.md)         | 🟡 Средний     | Система материалов            |

#### Воксельные данные

| Документ                                                               | Уровень        | Описание                        |
|------------------------------------------------------------------------|----------------|---------------------------------|
| [00_svo-architecture.md](architecture/practice/00_svo-architecture.md) | 🔴 Продвинутый | Sparse Voxel Octree архитектура |
| [10_vox-import.md](architecture/practice/10_vox-import.md)             | 🟡 Средний     | Импорт .vox файлов              |
| [22_asset-pipeline.md](architecture/practice/22_asset-pipeline.md)     | 🟡 Средний     | Пайплайн обработки ресурсов     |

#### Игровые системы

| Документ                                                                   | Уровень    | Описание          |
|----------------------------------------------------------------------------|------------|-------------------|
| [07_coordinate-systems.md](architecture/practice/07_coordinate-systems.md) | 🟢 Базовый | Система координат |
| [08_camera-controller.md](architecture/practice/08_camera-controller.md)   | 🟢 Базовый | Контроллер камеры |
| [09_input-system.md](architecture/practice/09_input-system.md)             | 🟢 Базовый | Система ввода     |
| [18_input-actions.md](architecture/practice/18_input-actions.md)           | 🟡 Средний | Action mapping    |
| [19_game-ui.md](architecture/practice/19_game-ui.md)                       | 🟡 Средний | Стратегия UI      |

#### Инфраструктура

| Документ                                                                 | Уровень        | Описание                 |
|--------------------------------------------------------------------------|----------------|--------------------------|
| [11_serialization.md](architecture/practice/11_serialization.md)         | 🟡 Средний     | Сериализация данных      |
| [12_custom-allocators.md](architecture/practice/12_custom-allocators.md) | 🔴 Продвинутый | Кастомные аллокаторы     |
| [13_reflection.md](architecture/practice/13_reflection.md)               | 🟡 Средний     | Reflection с Glaze       |
| [14_team-workflow.md](architecture/practice/14_team-workflow.md)         | 🟡 Средний     | Командный workflow       |
| [16_hot-reload.md](architecture/practice/16_hot-reload.md)               | 🟡 Средний     | Hot-reload шейдеров      |
| [17_asset-management.md](architecture/practice/17_asset-management.md)   | 🟡 Средний     | Runtime загрузка ассетов |

#### Продвинутые системы (Post-MVP)

| Документ                                                                               | Уровень        | Описание                       |
|----------------------------------------------------------------------------------------|----------------|--------------------------------|
| [21_networking-concept.md](architecture/practice/21_networking-concept.md)             | 🔴 Продвинутый | Концепция мультиплеера         |
| [23_gpu-cellular-automata.md](architecture/practice/23_gpu-cellular-automata.md)       | 🔴 Продвинутый | GPU клеточные автоматы         |
| [24_network-ready-ecs.md](architecture/practice/24_network-ready-ecs.md)               | 🔴 Продвинутый | Network-Ready ECS              |
| [25_cpu_gpu_physics_sync.md](architecture/practice/25_cpu_gpu_physics_sync.md)         | 🔴 Продвинутый | CPU-GPU синхронизация физики   |
| [26_modding_api_spec.md](architecture/practice/26_modding_api_spec.md)                 | 🟡 Средний     | Modding API                    |
| [27_destruction_physics_spec.md](architecture/practice/27_destruction_physics_spec.md) | 🔴 Продвинутый | Физика разрушений              |
| [30_ca_physics_bridge.md](architecture/practice/30_ca_physics_bridge.md)               | 🔴 Продвинутый | Мост CA-Physics                |
| [31_job_system_p2300_spec.md](architecture/practice/31_job_system_p2300_spec.md)       | 🔴 Продвинутый | Job System на базе P2300       |
| [32_voxel_sync_pipeline.md](architecture/practice/32_voxel_sync_pipeline.md)           | 🔴 Продвинутый | Амортизированные voxel updates |

---

## Философия проекта

Принципы и подходы к разработке:

| Документ                                                          | Описание                          |
|-------------------------------------------------------------------|-----------------------------------|
| [Обзор](philosophy/00_overview.md)                                | Введение в философию проекта      |
| [Манифест](philosophy/01_manifesto.md)                            | Основные принципы разработки      |
| [Zero-cost abstractions](philosophy/02_zero-cost-abstractions.md) | Абстракции без накладных расходов |
| [DOD-философия](philosophy/03_dod-philosophy.md)                  | Data-Oriented Design              |
| [ECS-философия](philosophy/04_ecs-philosophy.md)                  | Entity Component System           |
| [Оптимизация](philosophy/05_optimization-philosophy.md)           | Принципы оптимизации              |
| [Vulkan-философия](philosophy/06_vulkan-philosophy.md)            | Подход к графическому API         |
| [Воксельные данные](philosophy/07_voxel-data-philosophy.md)       | Организация данных вокселей       |
| [Антипаттерны](philosophy/08_anti-patterns.md)                    | Запрещённые практики              |
| [Тестирование](philosophy/09_testing-philosophy.md)               | Подход к тестированию             |
| [Принятие решений](philosophy/10_decision-making.md)              | Процесс принятия решений          |
| [Evil Hacks](philosophy/11_evil-hacks-philosophy.md)              | Обоснованные нарушения            |
| [Конкурентность](philosophy/12_concurrency-philosophy.md)         | Многопоточность                   |

---

## Документация библиотек

Техническая документация по используемым библиотекам:

| Библиотека                               | Назначение             | Тип             |
|------------------------------------------|------------------------|-----------------|
| [SDL](libraries/sdl)                     | Окна, ввод             | C-библиотека    |
| [Vulkan](libraries/vulkan)               | Графика                | C-библиотека    |
| [VMA](libraries/vma)                     | Управление памятью GPU | C-библиотека    |
| [volk](libraries/volk)                   | Загрузчик Vulkan       | C-библиотека    |
| [glm](libraries/glm)                     | Математика             | Header-only C++ |
| [glaze](libraries/glaze)                 | JSON-сериализация      | Header-only C++ |
| [flecs](libraries/flecs)                 | ECS                    | C++ (PIMPL)     |
| [JoltPhysics](libraries/joltphysics)     | Физика                 | C++ (PIMPL)     |
| [ImGui](libraries/imgui)                 | UI                     | C++ (PIMPL)     |
| [Tracy](libraries/tracy)                 | Профилирование         | C++ (PIMPL)     |
| [fastgltf](libraries/fastgltf)           | Загрузка glTF          | C++ (PIMPL)     |
| [Draco](libraries/draco)                 | Сжатие мешей           | C++ (PIMPL)     |
| [meshoptimizer](libraries/meshoptimizer) | Оптимизация геометрии  | C++             |
| [miniaudio](libraries/miniaudio)         | Аудио                  | C-библиотека    |
| [RmlUi](libraries/rmlui)                 | UI (альтернатива)      | C++ (PIMPL)     |
| [slang](libraries/slang)                 | Шейдеры                | C++             |
| [zstd](libraries/zstd)                   | Сжатие                 | C-библиотека    |
| [FreeType](libraries/freetype)           | Шрифты                 | C-библиотека    |

---

## Быстрая навигация

### Для новых разработчиков

1. Прочитать [README.md](README.md) — общее введение
2. Изучить [Философию проекта](philosophy/00_overview.md)
3. Ознакомиться со стандартами:

- [СТВ-CPP-001: Стандарт языка C++](standards/cpp/00_language-standard.md)
- [СТВ-GIT-001: Стандарт контроля версий](standards/git/00_version-control-standard.md)

### Для настройки окружения

1. [СТВ-CMAKE-001: Спецификация системы сборки](standards/cmake/00_specification.md)
2. [СТВ-CMAKE-006: Кросс-платформенная сборка](standards/cmake/05_cross-platform.md)
3. [СТВ-CMAKE-007: Устранение неполадок](standards/cmake/06_troubleshooting-ide.md)

### Для разработки

1. [СТВ-CPP-002: Стандарт структуры кода](standards/cpp/01_code-structure.md)
2. [СТВ-CPP-003: Стандарт управления памятью](standards/cpp/02_memory-management.md)
3. [СТВ-CMAKE-003: Управление зависимостями](standards/cmake/02_dependencies.md)

### Для ревью кода

1. [СТВ-GIT-003: Процесс Pull Request](standards/git/02_pull-request-process.md)
2. [Философия: Антипаттерны](philosophy/08_anti-patterns.md)

---

## Обозначения и терминология

### Ключевые слова требований

| Ключевое слово                  | Значение                                   |
|---------------------------------|--------------------------------------------|
| **ДОЛЖЕН** / **ОБЯЗАТЕЛЬНО**    | Требование обязательно к выполнению        |
| **НЕ ДОЛЖЕН** / **ЗАПРЕЩЕНО**   | Действие запрещено                         |
| **СЛЕДУЕТ** / **РЕКОМЕНДУЕТСЯ** | Рекомендация, которую желательно выполнить |
| **МОЖЕТ** / **ОПЦИОНАЛЬНО**     | Действие опционально                       |

### Типы документов

| Префикс    | Тип документа                      |
|------------|------------------------------------|
| СТВ        | Стандарт (обязателен к исполнению) |
| ADR        | Architecture Decision Record       |
| PRACTICE   | Практическое руководство           |
| PHILOSOPHY | Философский документ               |

---

## История редакций

| Версия | Дата       | Изменения                              |
|--------|------------|----------------------------------------|
| 1.0.0  | 22.02.2026 | Первоначальная версия на русском языке |
