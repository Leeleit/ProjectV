# Карта документации ProjectV

Полный список всех файлов документации с актуальной структурой.

## Дерево документации

## Обзор структуры документации

```
docs/
├── README.md                          # Главная страница документации
├── map.md                             # Эта карта документации (вы здесь)
│
├── philosophy/                        # Философия разработки
│   ├── 00_overview.md                 # Обзор философии
│   ├── 01_manifesto.md                # Манифест ProjectV
│   ├── 02_zero-cost-abstractions.md   # Zero-Cost Abstractions
│   ├── 03_dod-philosophy.md           # DOD философия
│   ├── 04_ecs-philosophy.md           # ECS философия
│   ├── 05_optimization-philosophy.md  # Философия оптимизации
│   ├── 06_vulkan-philosophy.md        # Философия Vulkan
│   ├── 07_voxel-data-philosophy.md    # Философия воксельных данных
│   ├── 08_anti-patterns.md            # Anti-Patterns
│   ├── 09_testing-philosophy.md       # Testing Philosophy
│   ├── 10_decision-making.md          # Decision Making
│   ├── 11_evil-hacks-philosophy.md    # Evil Hacks Philosophy
│   └── 12_concurrency-philosophy.md   # Философия многопоточности
│
├── guides/                            # Руководства
│   ├── cmake/                         # CMake Guide
│   │   ├── 00_overview.md             # Обзор
│   │   ├── 01_basics-structure.md     # Основы и структура
│   │   ├── 02_dependencies.md         # Управление зависимостями
│   │   ├── 03_build-configuration.md  # Конфигурация сборки
│   │   ├── 04_advanced-optimization.md # Продвинутые техники
│   │   ├── 05_cross-platform.md       # Кросс-платформенность
│   │   └── 06_troubleshooting-ide.md  # Решение проблем и IDE
│   │
│   ├── cpp/                           # C++ Handbook
│   │   ├── 00_overview.md             # Обзор Modern C++
│   │   ├── 01_basics-revisited.md     # Фундамент Modern C++
│   │   ├── 02_memory-management.md    # Управление памятью и RAII
│   │   ├── 03_coding-standards.md     # Стандарты кодирования
│   │   ├── 04_templates-and-concepts.md # Шаблоны и концепты
│   │   ├── 05_dod-practice.md         # Практика DOD
│   │   ├── 06_functional-cpp.md       # Функциональное C++
│   │   ├── 07_error-handling.md       # Безопасность и ошибки
│   │   ├── 08_multithreading.md       # Многопоточность
│   │   ├── 09_vulkan-cpp.md           # Vulkan C++ идиомы
│   │   ├── 10_cpp23-26-features.md    # C++23/26 Features
│   │   ├── 11_banned-features.md      # Запрещённые конструкции
│   │   ├── 12_evil-cpp.md             # Опасные техники
│   │   ├── 13_modern-cpp.md           # Modules, Concepts, Coroutines
│   │   └── 14_simd-intrinsics.md      # SIMD: SSE4.2, AVX2, AVX-512
│   │
│   └── git/                           # Git Workflow
│       ├── 00_README.md               # Обзор Git для ProjectV
│       ├── 01_basics-setup.md         # Основы и настройка
│       ├── 02_branching-commits.md    # Ветвление и коммиты
│       ├── 03_collaboration.md        # Сотрудничество
│       ├── 04_submodules-lfs.md       # Submodules и LFS
│       ├── 05_advanced-workflows.md   # Продвинутые workflow
│       └── 06_troubleshooting-ide.md  # Решение проблем и IDE
│
├── architecture/                       # Архитектура движка
│   ├── README.md                      # Обзор архитектуры
│   ├── 00_roadmap_and_scope.md        # MVP vs Vision, границы проекта
│   │
│   ├── theory/                        # Теоретические основы
│   │   ├── 00_README.md               # Обзор теории
│   │   ├── 01_ecs-concepts.md         # Концепции ECS
│   │   ├── 02_memory-layout.md        # Организация памяти
│   │   ├── 03_system-communication.md # Взаимодействие систем
│   │   ├── 04_caching.md              # Стратегии кэширования
│   │   ├── 05_memory-arenas.md        # Арены памяти
│   │   └── 06_composition.md          # Композиция поведения
│   │
│   └── practice/                      # Практическая реализация
│       ├── 00_svo-architecture.md     # SVO Architecture (🔴 Level 3)
│       ├── 01_core-loop.md            # Игровой цикл
│       ├── 02_resource-management.md  # Управление ресурсами
│       ├── 03_voxel-pipeline.md       # Воксельный пайплайн
│       ├── 04_modern-vulkan-guide.md  # Современный Vulkan
│       ├── 05_flecs-vulkan-bridge.md  # Интеграция Flecs + Vulkan
│       ├── 06_jolt-vulkan-bridge.md   # Интеграция Jolt + Vulkan
│       ├── 07_coordinate-systems.md   # Системы координат
│       ├── 08_camera-controller.md    # Камера
│       ├── 09_input-system.md         # Система ввода
│       ├── 10_vox-import.md           # Импорт .VOX файлов
│       ├── 11_serialization.md        # Сериализация
│       ├── 12_custom-allocators.md    # Кастомные аллокаторы
│       ├── 13_reflection.md           # Рефлексия
│       ├── 14_team-workflow.md        # Командный workflow
│       ├── 15_gpu-debugging.md        # Отладка GPU
│       ├── 16_hot-reload.md           # Hot Reload
│       ├── 17_asset-pipeline.md       # Asset Pipeline: Offline Compiler
│       ├── 18_input-actions.md        # Input Actions: Action Mapping
│       ├── 19_game-ui.md              # Game UI Strategy
│       ├── 20_material-system.md      # Material System
│       ├── 21_networking-concept.md   # Networking (Post-MVP Vision)
│       └── 22_asset-pipeline.md       # Asset Pipeline (Full)
│
├── libraries/                          # Библиотеки ProjectV
│   │
│   ├── vulkan/                        # Vulkan API
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_advanced-features.md
│   │   ├── 06_performance.md
│   │   ├── 07_troubleshooting.md
│   │   ├── 08_use-cases.md
│   │   ├── 09_glossary.md
│   │   ├── 10_projectv-integration.md
│   │   ├── 11_projectv-voxel.md
│   │   └── 12_projectv-optimization.md
│   │
│   ├── sdl/                           # SDL3
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_vulkan.md
│   │   ├── 05_api-reference.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_performance.md
│   │   ├── 08_use-cases.md
│   │   ├── 09_decision-trees.md
│   │   ├── 10_glossary.md
│   │   ├── 11_projectv-integration.md
│   │   └── 12_projectv-patterns.md
│   │
│   ├── volk/                          # Vulkan meta-loader
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_installation.md
│   │   ├── 03_concepts.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_performance.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-overview.md
│   │   ├── 09_projectv-integration.md
│   │   └── 10_projectv-patterns.md
│   │
│   ├── vma/                           # Vulkan Memory Allocator
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_installation.md
│   │   ├── 03_concepts.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_troubleshooting.md
│   │   ├── 06_glossary.md
│   │   ├── 07_projectv-overview.md
│   │   ├── 08_projectv-integration.md
│   │   ├── 09_projectv-patterns.md
│   │   └── 10_projectv-examples.md
│   │
│   ├── imgui/                         # Immediate-mode UI
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_widgets.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-integration.md
│   │   └── 09_projectv-patterns.md
│   │
│   ├── glm/                           # Математическая библиотека
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_performance.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-integration.md
│   │   └── 09_projectv-patterns.md
│   │
│   ├── fastgltf/                      # Загрузка glTF 2.0
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_performance.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-integration.md
│   │   └── 09_projectv-patterns.md
│   │
│   ├── draco/                        # Сжатие 3D геометрии
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_integration.md
│   │   ├── 03_concepts.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_encoding-methods.md
│   │   ├── 06_prediction-schemes.md
│   │   ├── 07_gltf-transcoding.md
│   │   ├── 08_performance.md
│   │   ├── 09_advanced.md
│   │   ├── 10_troubleshooting.md
│   │   ├── 11_glossary.md
│   │   └── 12_projectv-integration.md
│   │
│   ├── flecs/                         # Entity Component System
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_tools.md
│   │   ├── 06_performance.md
│   │   ├── 07_troubleshooting.md
│   │   ├── 08_glossary.md
│   │   ├── 09_projectv-integration.md
│   │   └── 10_projectv-patterns.md
│   │
│   ├── joltphysics/                   # Физический движок
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_performance.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-integration.md
│   │   ├── 09_projectv-patterns.md
│   │   └── 10_advanced-physics.md
│   │
│   ├── miniaudio/                     # Аудио библиотека
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_performance.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-integration.md
│   │   └── 09_projectv-patterns.md
│   │
│   ├── slang/                         # Шейдерный язык
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_installation.md
│   │   ├── 03_concepts.md
│   │   ├── 04_language-reference.md
│   │   ├── 05_api-reference.md
│   │   ├── 06_vulkan-integration.md
│   │   ├── 07_performance.md
│   │   ├── 08_troubleshooting.md
│   │   ├── 09_glossary.md
│   │   ├── 10_use-cases.md
│   │   ├── 11_decision-trees.md
│   │   ├── 12_projectv-overview.md
│   │   ├── 13_projectv-integration.md
│   │   ├── 14_projectv-patterns.md
│   │   └── 15_projectv-examples.md
│   │
│   ├── tracy/                         # Профилировщик
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_installation.md
│   │   ├── 03_concepts.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_gpu-profiling.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-overview.md
│   │   ├── 09_projectv-integration.md
│   │   ├── 10_projectv-patterns.md
│   │   └── 11_projectv-examples.md
│   │
│   ├── zstd/                          # Сжатие данных
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_performance.md
│   │   ├── 06_troubleshooting.md
│   │   ├── 07_glossary.md
│   │   ├── 08_projectv-integration.md
│   │   ├── 09_custom-dictionaries.md
│   │   ├── 10_bit-packing.md
│   │   └── 11_memory-mapped-io.md
│   │
│   ├── freetype/                      # Рендеринг шрифтов
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_tools.md
│   │   ├── 06_performance.md
│   │   ├── 07_troubleshooting.md
│   │   ├── 08_glossary.md
│   │   ├── 09_projectv-integration.md
│   │   └── 10_projectv-patterns.md
│   │
│   ├── rmlui/                         # HTML/CSS UI библиотека
│   │   ├── 00_overview.md
│   │   ├── 01_quickstart.md
│   │   ├── 02_concepts.md
│   │   ├── 03_integration.md
│   │   ├── 04_api-reference.md
│   │   ├── 05_tools.md
│   │   ├── 06_performance.md
│   │   ├── 07_troubleshooting.md
│   │   ├── 08_glossary.md
│   │   ├── 09_projectv-integration.md
│   │   └── 10_projectv-patterns.md
│   │
│   └── meshoptimizer/                 # Оптимизация 3D мешей
│       ├── 00_overview.md
│       ├── 01_quickstart.md
│       ├── 02_installation.md
│       ├── 03_concepts.md
│       ├── 04_api-reference.md
│       ├── 05_troubleshooting.md
│       ├── 06_glossary.md
│       ├── 07_projectv-overview.md
│       ├── 08_projectv-integration.md
│       ├── 09_projectv-patterns.md
│       └── 10_projectv-examples.md
│
├── tutorials/                         # Туториалы
│   ├── 00_overview.md
│   └── 01_first-scene.md
│
└── future/                            # Будущие технологии
    └── 01_svo-rendering.md           # SVO рендеринг
```

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

- **С чего начать?** → [Philosophy](philosophy/00_overview.md)
- **Как писать код?** → [C++ Handbook](guides/cpp/00_overview.md)
- **Как работает движок?** → [Architecture](architecture/README.md)
- **Какие библиотеки используем?** → [Libraries](libraries)

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
