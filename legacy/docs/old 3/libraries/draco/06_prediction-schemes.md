# Prediction Schemes

🟡 **Уровень 2: Средний**

Методы предсказания значений атрибутов для улучшения сжатия.

## Принцип работы

Prediction schemes предсказывают значение атрибута на основе соседних элементов. Вместо хранения абсолютных значений
кодируется разница (residual) между предсказанием и реальным значением.

```
Original:    [1.0, 1.1, 1.2, 1.3, 1.4]
Predicted:   [---, 1.0, 1.1, 1.2, 1.3]
Residual:    [1.0, 0.1, 0.1, 0.1, 0.1]  ← Меньше энтропия
```

## Доступные schemes

```cpp
enum PredictionSchemeMethod {
    PREDICTION_NONE = -2,
    PREDICTION_UNDEFINED = -1,
    PREDICTION_DIFFERENCE = 0,           // Delta coding
    MESH_PREDICTION_PARALLELOGRAM = 1,   // Параллелограмм
    MESH_PREDICTION_MULTI_PARALLELOGRAM = 2,
    MESH_PREDICTION_TEX_COORDS_PORTABLE = 5,
    MESH_PREDICTION_GEOMETRIC_NORMAL = 6,
};
```

## Delta Coding (PREDICTION_DIFFERENCE)

Базовый метод — предсказание через предыдущее значение.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::GENERIC,
    draco::PREDICTION_DIFFERENCE
);
```

**Использование:**

- Generic атрибуты
- Point cloud данные
- Когда нет topology information

**Пример:**

```
Values:     [100, 102, 105, 103, 108]
Predicted:  [---, 100, 102, 105, 103]
Delta:      [100,   2,   3,  -2,   5]
```

## Parallelogram Prediction

Предсказание позиции вершины через смежные треугольники.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_PARALLELOGRAM
);
```

**Принцип:**

```
    C
   / \
  /   \
 A-----B-----> P (predicted)

P = B + C - A
```

Для нового угла при вершине B предсказание вычисляется из смежного треугольника ABC.

**Использование:**

- Mesh positions
- Только для triangle meshes
- Требует connectivity data

**Точность:**

- Работает хорошо для гладких поверхностей
- Ошибки на sharp edges, creases

## Multi-Parallelogram Prediction

Улучшенный parallelogram с несколькими соседними треугольниками.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_MULTI_PARALLELOGRAM
);
```

**Принцип:**

```
       C       D
      / \     / \
     /   \   /   \
    A-----B-----E-----F
            \
             P (predicted)

P = weighted_average(parallelogram_predictions)
```

**Использование:**

- Mesh positions
- Лучше для валиков (valence > 4)
- Выше точность на гладких поверхностях

**Trade-offs:**

- Лучшее предсказание
- Медленнее encoding/decoding
- Больше overhead

## Constrained Multi-Parallelogram

Вариант с ограничением на количество используемых соседей.

```cpp
// Выбирается автоматически при средних speed values
encoder.SetSpeedOptions(3, 3);
```

Оптимизация между точностью и скоростью.

## Geometric Normal Prediction

Специализированный метод для нормалей.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::NORMAL,
    draco::MESH_PREDICTION_GEOMETRIC_NORMAL
);
```

**Принцип:**

1. Вычисление геометрической нормали из позиций вершин
2. Предсказание нормали атрибута на основе геометрической
3. Кодирование разницы через octahedron transform

**Octahedron Transform:**

```
3D normal (x, y, z) → 2D octahedron coordinates (u, v)

     /\
    /  \     Нормали отображаются
   /    \    на поверхность октаэдра
  /______\
```

> **Примечание:** Здесь используется SIMD для оптимизации. См. гайд docs/guides/cpp/14_simd-intrinsics.md

## Tex Coords Portable Prediction

Специализированный метод для UV координат.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::TEX_COORD,
    draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
);
```

**Принцип:**

1. Анализ смежных треугольников
2. Предсказание UV на основе текстурной непрерывности
3. Обработка seams (швов) в UV mapping

**Особенности:**

- Работает с UV seams
- Портативный между платформами
- Учитывает texture wrapping

## Transform Types

Prediction schemes используют transforms для residual coding:

```cpp
enum PredictionSchemeTransformType {
    PREDICTION_TRANSFORM_NONE = -1,
    PREDICTION_TRANSFORM_DELTA = 0,         // Базовый delta
    PREDICTION_TRANSFORM_WRAP = 1,          // Wrap-around для integer
    PREDICTION_TRANSFORM_NORMAL_OCTAHEDRON = 2,        // Для normal
    PREDICTION_TRANSFORM_NORMAL_OCTAHEDRON_CANONICALIZED = 3,
};
```

### Wrap Transform

Для integer атрибутов с ограниченным диапазоном:

```cpp
// Вместо:
// residual = actual - predicted

// Используется wrap:
// residual = wrap(actual - predicted, min_value, max_value)
```

Уменьшает magnitude residuals.

### Normal Octahedron Transform

Для нормалей:

```cpp
// 3D normal → 2D octahedron coordinates
// residuals кодируются в 2D пространстве
```

## Выбор prediction scheme

### По типу атрибута

| Атрибут   | Рекомендуемый scheme                  |
|-----------|---------------------------------------|
| POSITION  | PARALLELOGRAM или MULTI_PARALLELOGRAM |
| NORMAL    | GEOMETRIC_NORMAL                      |
| TEX_COORD | TEX_COORDS_PORTABLE                   |
| COLOR     | DIFFERENCE                            |
| GENERIC   | DIFFERENCE                            |

### По типу геометрии

| Геометрия         | Рекомендации        |
|-------------------|---------------------|
| Smooth mesh       | MULTI_PARALLELOGRAM |
| Hard surface mesh | PARALLELOGRAM       |
| Point cloud       | DIFFERENCE          |
| Organic model     | MULTI_PARALLELOGRAM |

### Автоматический выбор

```cpp
// По умолчанию encoder выбирает оптимальный scheme
draco::Encoder encoder;
encoder.SetSpeedOptions(0, 0);  // Лучший compression
// Prediction schemes выбираются автоматически
```

## Примеры

### Явное указание schemes

```cpp
draco::Encoder encoder;

// Positions: multi-parallelogram
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_MULTI_PARALLELOGRAM
);

// Normals: geometric prediction
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::NORMAL,
    draco::MESH_PREDICTION_GEOMETRIC_NORMAL
);

// UV: portable tex coords
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::TEX_COORD,
    draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
);

// Colors: simple delta
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::COLOR,
    draco::PREDICTION_DIFFERENCE
);
```

### Через ExpertEncoder

```cpp
draco::ExpertEncoder encoder(mesh);

// Атрибут 0 (position): parallelogram
encoder.SetPredictionSchemeForAttribute(0, draco::MESH_PREDICTION_PARALLELOGRAM);

// Атрибут 1 (normal): geometric normal
encoder.SetPredictionSchemeForAttribute(1, draco::MESH_PREDICTION_GEOMETRIC_NORMAL);

// Атрибут 2 (uv): tex coords portable
encoder.SetPredictionSchemeForAttribute(2, draco::MESH_PREDICTION_TEX_COORDS_PORTABLE);

// Атрибут 3 (generic): no prediction
encoder.SetPredictionSchemeForAttribute(3, draco::PREDICTION_NONE);
```

### Отключение prediction

```cpp
// Иногда prediction ухудшает сжатие
// Например, для случайных данных

encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::GENERIC,
    draco::PREDICTION_NONE
);
```

## Влияние на compression

Ориентировочное улучшение compression ratio:

| Scheme              | Improvement vs NONE |
|---------------------|---------------------|
| DIFFERENCE          | 10-20%              |
| PARALLELOGRAM       | 20-35%              |
| MULTI_PARALLELOGRAM | 25-40%              |
| GEOMETRIC_NORMAL    | 30-50%              |
| TEX_COORDS_PORTABLE | 15-25%              |

> **Примечание:** Результаты зависят от характеристик модели.

## Диагностика

### Проверка эффективности prediction

```cpp
// После кодирования проверьте размер атрибутов
for (int i = 0; i < mesh.num_attributes(); ++i) {
    const auto* attr = mesh.attribute(i);
    std::cout << "Attribute " << i << ": "
              << attr->size() << " values\n";
}
```

### Когда prediction не помогает

- Случайные данные (noise)
- Высокочастотные изменения
- Несвязанные атрибуты
- Очень маленькие меши (< 100 vertices)

## Внутренняя реализация

### Encoder side

```cpp
// Упрощённая логика
for (each vertex in traversal_order) {
    predicted_value = compute_prediction(vertex, neighbors);
    residual = actual_value - predicted_value;
    encode_residual(residual);
}
```

### Decoder side

```cpp
// Упрощённая логика
for (each vertex in traversal_order) {
    predicted_value = compute_prediction(vertex, neighbors);
    residual = decode_residual();
    actual_value = predicted_value + residual;
}
```

Traversal order должен быть идентичным для encoder и decoder.
