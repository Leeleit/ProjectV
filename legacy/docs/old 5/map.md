# Карта документации ProjectV

Полный список всех файлов документации с актуальной структурой.

## Дерево документации

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

## Статистика

| Категория               | Количество файлов |
|-------------------------|-------------------|
| Philosophy              | 13                |
| Guides (CMake)          | 7                 |
| Guides (C++)            | 15                |
| Guides (Git)            | 7                 |
| Architecture (theory)   | 7                 |
| Architecture (practice) | 24                |
| Libraries               | ~130              |
| Tutorials               | 2                 |
| Future                  | 1                 |
| **Итого**               | **~205 файлов**   |

## Быстрая навигация

- **С чего начать?** → [Philosophy](philosophy/00_overview.md)
- **Как писать код?** → [C++ Handbook](guides/cpp/00_overview.md)
- **Как работает движок?** → [Architecture](architecture/README.md)
- **Какие библиотеки используем?** → [Libraries](libraries)

---

*Карта обновлена: 22.02.2026*
