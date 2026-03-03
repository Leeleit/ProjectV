# Draco

🟡 **Уровень 2: Средний**

**Draco** — библиотека сжатия и распаковки 3D геометрических данных (mesh и point cloud) от Google. Спроектирована для
эффективного хранения и передачи 3D-графики с минимальной потерей визуального качества.

Версия: **1.5.7**
Исходники: [google/draco](https://github.com/google/draco)

---

## Возможности

- **Сжатие mesh** — треугольные меши с connectivity data
- **Сжатие point cloud** — облака точек с произвольными атрибутами
- **Квантование** — контролируемая потеря точности для лучшего сжатия
- **Prediction schemes** — предсказание значений атрибутов по соседним элементам
- **glTF интеграция** — EXT_mesh_draco расширение
- **Metadata** — пользовательские данные в сжатом файле

## Архитектура сжатия

```
Исходные данные (Mesh / PointCloud)
         ↓
    Quantization (опционально)
         ↓
    Prediction Scheme
         ↓
    Entropy Encoding (rANS)
         ↓
    Draco bitstream (.drc)
```

## Compression ratio

| Тип данных            | Без сжатия | Draco (default) | Сжатие |
|-----------------------|------------|-----------------|--------|
| Mesh 100K triangles   | 12.5 MB    | 0.8 MB          | 15x    |
| Point cloud 1M points | 28 MB      | 2.1 MB          | 13x    |
| Skinned mesh          | 18 MB      | 1.4 MB          | 12x    |

> **Примечание:** Результаты зависят от настроек квантования и типа геометрии.

## Сравнение с альтернативами

| Функция                 | Draco    | Open3D  | MeshOptimizer |
|-------------------------|----------|---------|---------------|
| Mesh compression        | Да       | Да      | Да            |
| Point cloud compression | Да       | Да      | Нет           |
| Lossless                | Частично | Да      | Нет           |
| glTF extension          | Да       | Нет     | Да            |
| GPU decoding            | Нет      | Нет     | Да            |
| C++ API                 | Да       | Да      | Да            |
| Decode speed            | Средняя  | Быстрая | Быстрая       |

**Когда выбрать Draco:**

- Нужна интеграция с glTF через EXT_mesh_draco
- Требуется сжатие point cloud
- Важен максимальный compression ratio
- Данные передаются по сети

**Когда выбрать альтернативы:**

- **MeshOptimizer** — если нужен GPU decoding или lossless
- **Open3D** — если нужен полный pipeline обработки 3D данных

## Компоненты библиотеки

| Компонент              | Назначение                              |
|------------------------|-----------------------------------------|
| `draco::Decoder`       | Декодирование .drc в Mesh/PointCloud    |
| `draco::Encoder`       | Кодирование с базовыми настройками      |
| `draco::ExpertEncoder` | Детальный контроль над каждым атрибутом |
| `draco::Mesh`          | Треугольный меш с атрибутами            |
| `draco::PointCloud`    | Облако точек с атрибутами               |
| `draco_transcoder`     | CLI инструмент для glTF                 |

## Методы кодирования

### Mesh

| Метод       | Описание           | Compression | Decode speed |
|-------------|--------------------|-------------|--------------|
| Edgebreaker | Connectivity-first | Лучший      | Средняя      |
| Sequential  | Simple traversal   | Хороший     | Быстрая      |

### Point Cloud

| Метод      | Описание             | Compression | Decode speed |
|------------|----------------------|-------------|--------------|
| KD-Tree    | Spatial partitioning | Лучший      | Медленная    |
| Sequential | Linear traversal     | Хороший     | Быстрая      |

## Prediction schemes

| Scheme              | Применение     | Описание                                   |
|---------------------|----------------|--------------------------------------------|
| Parallelogram       | Mesh positions | Предсказание по смежным треугольникам      |
| Multi-parallelogram | Mesh positions | Улучшенный parallelogram                   |
| Geometric normal    | Normals        | Предсказание нормалей по геометрии         |
| Delta               | Generic        | Простая дельта-кодировка                   |
| Tex coords portable | UV             | Специализированно для текстурных координат |

## Квантование

Draco использует квантование для уменьшения точности данных:

| Атрибут   | Default (бит) | Рекомендуемый диапазон |
|-----------|---------------|------------------------|
| Position  | 11            | 10-16                  |
| Normal    | 8             | 6-10                   |
| Tex coord | 10            | 8-12                   |
| Color     | 8             | 6-10                   |

> **Примечание:** Большее число бит = выше точность, но хуже сжатие.

## Документация

| Файл                                                     | Содержание                                     |
|----------------------------------------------------------|------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)                     | Минимальные примеры кодирования/декодирования  |
| [02_integration.md](02_integration.md)                   | CMake интеграция, флаги сборки                 |
| [03_concepts.md](03_concepts.md)                         | Основные понятия: Mesh, PointCloud, Attributes |
| [04_api-reference.md](04_api-reference.md)               | Справочник API: Decoder, Encoder               |
| [05_encoding-methods.md](05_encoding-methods.md)         | Методы: Edgebreaker, KD-Tree, Sequential       |
| [06_prediction-schemes.md](06_prediction-schemes.md)     | Prediction schemes                             |
| [07_gltf-transcoding.md](07_gltf-transcoding.md)         | Интеграция с glTF                              |
| [08_performance.md](08_performance.md)                   | Оптимизация compression/decompression          |
| [09_advanced.md](09_advanced.md)                         | Metadata, animations, transcoder features      |
| [10_troubleshooting.md](10_troubleshooting.md)           | Решение проблем                                |
| [11_glossary.md](11_glossary.md)                         | Глоссарий терминов                             |
| [12_projectv-integration.md](12_projectv-integration.md) | Интеграция в ProjectV                          |

## Требования

- **C++11** или новее (C++17 для DRACO_TRANSCODER_SUPPORTED)
- Платформы: Windows, Linux, macOS, Android, iOS

## Оригинальная документация

- [Draco README](https://github.com/google/draco/blob/main/README.md)
- [BUILDING.md](https://github.com/google/draco/blob/main/BUILDING.md)
- [glTF EXT_mesh_draco](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression)
