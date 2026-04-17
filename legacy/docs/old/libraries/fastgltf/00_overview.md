# fastgltf

🟡 **Уровень 2: Средний**

**fastgltf** — высокопроизводительная библиотека загрузки glTF 2.0 на C++17 с минимальными зависимостями. Использует
SIMD (simdjson) для ускорения парсинга JSON и base64-декодирования. Поддерживает полную спецификацию glTF 2.0 и
множество расширений.

Версия: **0.9.0**
Исходники: [spnda/fastgltf](https://github.com/spnda/fastgltf)

---

## Возможности

- **glTF 2.0** — полная поддержка спецификации (чтение и запись)
- **SIMD-оптимизации** — ускорение парсинга в 2-20 раз по сравнению с аналогами
- **Минимальные зависимости** — только simdjson
- **Accessor tools** — утилиты для чтения данных, включая sparse accessors
- **GPU-ready** — возможность прямой записи в mapped GPU buffers
- **C++20 modules** — опциональная поддержка модулей
- **Android** — нативная загрузка из APK assets

## Сравнение с альтернативами

| Функция                   | cgltf    | tinygltf | fastgltf |
|---------------------------|----------|----------|----------|
| Чтение glTF 2.0           | Да       | Да       | Да       |
| Запись glTF 2.0           | Да       | Да       | Да       |
| Поддержка расширений      | Да       | Частично | Да       |
| Декодирование изображений | Да       | Да       | Нет      |
| Built-in Draco            | Нет      | Да       | Нет      |
| Memory callbacks          | Да       | Нет      | Частично |
| Android assets            | Нет      | Да       | Да       |
| Accessor utilities        | Да       | Нет      | Да       |
| Sparse accessor utilities | Частично | Нет      | Да       |
| Matrix accessor utilities | Частично | Нет      | Да       |
| Node transform utilities  | Да       | Нет      | Да       |

**Когда выбрать fastgltf:**

- Нужна максимальная скорость загрузки
- Требуется современный C++ API (std::variant, std::optional)
- Важна типобезопасность
- Нужны accessor tools для работы с данными

**Когда выбрать альтернативы:**

- **tinygltf** — если нужна встроенная загрузка изображений или Draco-декомпрессия
- **cgltf** — если нужен C API или header-only решение

## Философия

fastgltf следует принципу C++: "you don't pay for what you don't use".

**По умолчанию:**

- Только парсинг JSON
- GLB-буферы загружаются в память (ByteView/Array)
- Внешние буферы — только URI (без загрузки)

**Опционально (через Options):**

- `LoadExternalBuffers` — загрузка внешних .bin файлов
- `LoadExternalImages` — загрузка внешних изображений
- `DecomposeNodeMatrices` — разложение матриц на TRS

## Типобезопасность

Сравнение подходов:

```cpp
// tinygltf: -1 означает "отсутствует"
if (node.mesh != -1) {
    auto& mesh = model.meshes[node.mesh];
}

// fastgltf: Optional с проверкой типа
if (node.meshIndex.has_value()) {
    auto& mesh = asset.meshes[*node.meshIndex];
}
```

## Проекты, использующие fastgltf

- [Fwog](https://github.com/JuanDiegoMontoya/Fwog) — современная абстракция OpenGL 4.6
- [Castor3D](https://github.com/DragonJoker/Castor3D) — мультиплатформенный 3D движок
- [Raz](https://github.com/Razakhel/RaZ) — современный игровой движок на C++17
- [vkguide](https://vkguide.dev) — современный туториал по Vulkan
- [lvgl](https://github.com/lvgl/lvgl) — графическая библиотека для встраиваемых систем
- [OptiX_Apps](https://github.com/NVIDIA/OptiX_Apps) — официальные примеры NVIDIA OptiX
- [vk-gltf-viewer](https://github.com/stripe2933/vk-gltf-viewer) — высокопроизводительный glTF рендерер на Vulkan

## Документация

| Файл                                           | Содержание                                     |
|------------------------------------------------|------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | Быстрый старт: минимальный пример загрузки     |
| [02_integration.md](02_integration.md)         | Интеграция: CMake, Options, Extensions         |
| [03_concepts.md](03_concepts.md)               | Основные понятия: Asset, Accessor, DataSource  |
| [04_api-reference.md](04_api-reference.md)     | Справочник API: Parser, Asset, structures      |
| [05_tools.md](05_tools.md)                     | Accessor tools: чтение данных                  |
| [06_performance.md](06_performance.md)         | Производительность и benchmarks                |
| [07_advanced.md](07_advanced.md)               | Продвинутые темы: sparse, animations, skinning |
| [08_troubleshooting.md](08_troubleshooting.md) | Решение проблем: ошибки и диагностика          |
| [09_glossary.md](09_glossary.md)               | Глоссарий терминов                             |

## Требования

- **C++17** или новее (опционально C++20 для модулей)
- **simdjson** (подгружается автоматически через CMake)
- Платформы: Windows, Linux, macOS, Android

## Оригинальная документация

- [fastgltf docs](https://github.com/spnda/fastgltf/tree/main/docs) — официальная документация
- [glTF 2.0 Specification](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html) — спецификация формата
- [glTF Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf) — краткий справочник от Khronos
