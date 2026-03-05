# Производительность fastgltf

🟢 **Уровень 1: Начинающий**

Benchmarks и обоснование выбора fastgltf.

## Сравнение с альтернативами

### Тест 1: Embedded buffers (base64)

Модель: **2CylinderEngine** (1.7MB embedded buffer, закодирован в base64)

fastgltf включает оптимизированный base64-декодер с поддержкой AVX2, SSE4 и ARM Neon.

| Библиотека   | Время | Относительно fastgltf |
|--------------|-------|-----------------------|
| **fastgltf** | ~4ms  | 1x (базовая линия)    |
| tinygltf     | ~98ms | 24.5x медленнее       |
| cgltf        | ~30ms | 7.4x медленнее        |

### Тест 2: Большие JSON-файлы

Модель: **Amazon Bistro** (148k строк JSON, конвертирован в glTF 2.0)

Показывает чистую скорость десериализации.

| Библиотека   | Время | Относительно fastgltf |
|--------------|-------|-----------------------|
| **fastgltf** | ~10ms | 1x (базовая линия)    |
| tinygltf     | ~14ms | 1.4x медленнее        |
| cgltf        | ~50ms | 5x медленнее          |

### Выводы

- **Base64-декодирование**: fastgltf лидирует благодаря SIMD
- **JSON-парсинг**: fastgltf быстрее за счёт simdjson
- **Большие модели**: разница критична при загрузке сложных сцен

> **Примечание:** fastgltf следует принципу zero-cost abstractions — по умолчанию выполняется только парсинг JSON.
> См. [02_zero-cost-abstractions.md](../../philosophy/02_zero-cost-abstractions.md).

## Оптимизация загрузки

### Category

Выбор правильного Category ускоряет загрузку:

| Category         | Что загружается                             | Когда использовать              |
|------------------|---------------------------------------------|---------------------------------|
| `OnlyRenderable` | Всё, кроме Animations/Skins                 | Статические модели              |
| `OnlyAnimations` | Animations, Accessors, BufferViews, Buffers | Извлечение анимаций             |
| `All`            | Всё                                         | Анимированные модели, редакторы |

```cpp
// Для статических моделей — быстрее на 30-40%
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);
```

### Источники данных

| Класс            | Когда использовать     | Особенности                   |
|------------------|------------------------|-------------------------------|
| `GltfDataBuffer` | Стандартная загрузка   | Универсальный                 |
| `MappedGltfFile` | Большие файлы (>100MB) | Memory mapping, меньше памяти |
| `GltfFileStream` | Streaming              | Потоковое чтение              |

### Переиспользование Parser

```cpp
// Хорошо: переиспользуем parser
fastgltf::Parser parser;  // Создаём один раз

for (const auto& file : files) {
    auto data = fastgltf::GltfDataBuffer::FromPath(file);
    auto asset = parser.loadGltf(data.get(), ...);
    // ...
}

// Плохо: создаём parser для каждого файла
for (const auto& file : files) {
    fastgltf::Parser parser;  // Неэффективно!
    // ...
}
```

### Асинхронная загрузка

Parser не потокобезопасен, но можно использовать отдельные экземпляры:

```cpp
// Каждый поток — свой parser
thread_local fastgltf::Parser threadParser;

// Или пул парсеров
std::vector<std::unique_ptr<fastgltf::Parser>> parserPool;
```

## Профилирование

### Типичные временные затраты

| Операция                      | Время (примерно) | Оптимизации                   |
|-------------------------------|------------------|-------------------------------|
| Парсинг JSON (10MB)           | 5-15ms           | `Category::OnlyRenderable`    |
| Загрузка буферов (50MB)       | 20-50ms          | `LoadExternalBuffers` + async |
| Чтение accessor (100K вершин) | 1-3ms            | Правильный тип в шаблоне      |
| Обход сцены (1000 узлов)      | 0.5-1ms          | `iterateSceneNodes`           |

### Bottlenecks

1. **Base64-декодирование** — используйте GLB вместо embedded buffers
2. **Загрузка внешних файлов** — используйте async загрузку
3. **Конвертация типов** — используйте `copyFromAccessor` с правильным типом

## Рекомендации

### Для статических моделей

```cpp
fastgltf::Parser parser;
auto data = fastgltf::GltfDataBuffer::FromPath(path);
auto asset = parser.loadGltf(
    data.get(),
    path.parent_path(),
    fastgltf::Options::LoadExternalBuffers,
    fastgltf::Category::OnlyRenderable
);
```

### Для больших файлов

```cpp
// Memory mapping для больших GLB
auto data = fastgltf::MappedGltfFile::FromPath("large_model.glb");
```

### Для множества файлов

```cpp
// Переиспользуйте parser и используйте async
fastgltf::Parser parser;  // Один на поток

// Параллельная загрузка
std::vector<std::future<Asset>> futures;
for (const auto& file : files) {
    futures.push_back(std::async([&parser, file] {
        auto data = fastgltf::GltfDataBuffer::FromPath(file);
        return parser.loadGltf(data.get(), ...).get();
    }));
}
