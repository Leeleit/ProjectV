# Глоссарий fastgltf

🟢 **Уровень 1: Начинающий**

Термины glTF 2.0 и fastgltf.

## Карта связей ключевых терминов

```
Файл .gltf/.glb
      ↓
    Parser
      ↓
    Asset
      ├── buffers[] (DataSource)
      ├── bufferViews[] (byteOffset/Stride)
      ├── accessors[] (Type/ComponentType)
      ├── meshes[] (Primitive[])
      ├── materials[] (PBR)
      ├── nodes[] (transform: TRS/matrix)
      ├── scenes[] (nodeIndices)
      ├── animations[]
      └── skins[]

Buffer → BufferView → Accessor → Primitive
                                     ↓
                                   Mesh → Node → Scene
```

---

## glTF 2.0

| Термин    | Определение                                                                                                          |
|-----------|----------------------------------------------------------------------------------------------------------------------|
| **glTF**  | GL Transmission Format — формат 3D-моделей от Khronos Group. Состоит из JSON-файла с метаданными и бинарных буферов. |
| **GLB**   | Бинарный контейнер glTF: один файл с JSON-чанком и бинарными чанками.                                                |
| **Asset** | Результат парсинга glTF. Содержит все данные модели: меши, материалы, узлы, анимации.                                |

## Цепочка данных

| Термин         | Определение                                                                         | Пример                             |
|----------------|-------------------------------------------------------------------------------------|------------------------------------|
| **Buffer**     | Массив байтов — сырые данные. Источник определяется через `DataSource`.             | `Buffer[0]: 16384 байт`            |
| **BufferView** | Участок буфера: offset, length, stride. Связывает Accessor с Buffer.                | `offset=0, length=8192, stride=12` |
| **Accessor**   | Типизированное описание данных: тип, componentType, count. Ссылается на BufferView. | `Vec3 Float, count=1024`           |

### Формула размера

```
size = count * getNumComponents(type) * getComponentByteSize(componentType)
```

## Геометрия

| Термин        | Определение                                                                                           |
|---------------|-------------------------------------------------------------------------------------------------------|
| **Primitive** | Часть меша: режим отрисовки, атрибуты, индексы, материал. Доступ к атрибутам через `findAttribute()`. |
| **Mesh**      | Набор примитивов. Один меш может содержать несколько примитивов с разными материалами.                |
| **Attribute** | Именованная ссылка на accessor: POSITION, NORMAL, TEXCOORD_0, TANGENT, JOINTS_0, WEIGHTS_0.           |

## Иерархия сцены

| Термин    | Определение                                                                                 |
|-----------|---------------------------------------------------------------------------------------------|
| **Node**  | Узел сцены. Содержит transform (TRS или matrix), ссылки на mesh, skin, camera, children.    |
| **Scene** | Набор корневых узлов через `nodeIndices`. У модели может быть несколько сцен.               |
| **TRS**   | Decomposed transform: Translation, Rotation, Scale. Появляется при `DecomposeNodeMatrices`. |

## Материалы

| Термин          | Определение                                                                           |
|-----------------|---------------------------------------------------------------------------------------|
| **Material**    | PBR материал: baseColor, metallic, roughness, normal, emission.                       |
| **PBR**         | Physically Based Rendering — подход к материалов, основанный на физических свойствах. |
| **TextureInfo** | Ссылка на текстуру: textureIndex, texCoordIndex, опциональный transform.              |
| **Sampler**     | Настройки фильтрации и wrapping для текстуры.                                         |

## Анимации

| Термин               | Определение                                                                            |
|----------------------|----------------------------------------------------------------------------------------|
| **Animation**        | Набор каналов и сэмплеров для анимации свойств узлов.                                  |
| **AnimationChannel** | Связь между target (node + path) и sampler.                                            |
| **AnimationSampler** | Интерполяция между ключевыми кадрами: input (время), output (значения), interpolation. |
| **Interpolation**    | LINEAR, STEP, CUBICSPLINE — метод интерполяции между кадрами.                          |

## Скиннинг

| Термин                  | Определение                                                   |
|-------------------------|---------------------------------------------------------------|
| **Skin**                | Данные для skeletal animation: joints, inverseBindMatrices.   |
| **Joint**               | Кость скелета — ссылка на Node.                               |
| **Inverse Bind Matrix** | Матрица для перевода вершины в пространство кости.            |
| **JOINTS_0**            | Атрибут примитива: индексы костей для каждой вершины (uvec4). |
| **WEIGHTS_0**           | Атрибут примитива: веса влияния костей (vec4).                |

## Morph Targets

| Термин           | Определение                                                                           |
|------------------|---------------------------------------------------------------------------------------|
| **Morph Target** | Blend shape — дельты позиций/нормалей для деформации меша.                            |
| **Weight**       | Вес morph target (0-1) для интерполяции между базовой и целевой формой.               |
| **targets**      | Массив morph targets в Primitive. Каждый target — набор атрибутов (POSITION, NORMAL). |

## Sparse Accessors

| Термин              | Определение                                                                 |
|---------------------|-----------------------------------------------------------------------------|
| **Sparse Accessor** | Accessor с частичным обновлением данных. Хранит только изменённые значения. |
| **Sparse Indices**  | Индексы элементов, которые заменяются.                                      |
| **Sparse Values**   | Новые значения для указанных индексов.                                      |

## Система загрузки fastgltf

| Термин            | Определение                                                                        |
|-------------------|------------------------------------------------------------------------------------|
| **Parser**        | Класс для парсинга glTF/GLB. Не потокобезопасен, переиспользуйте между загрузками. |
| **Expected\<T\>** | Тип возврата функций загрузки: содержит либо T, либо Error.                        |
| **DataSource**    | variant с источником данных буфера/изображения.                                    |

## DataSource варианты

| Вариант          | Когда появляется              | Нужен кастомный адаптер? |
|------------------|-------------------------------|--------------------------|
| **ByteView**     | GLB, base64 без копирования   | Нет                      |
| **Array**        | GLB, встроенные данные        | Нет                      |
| **Vector**       | `LoadExternalBuffers`         | Нет                      |
| **URI**          | Внешний файл без загрузки     | Да                       |
| **CustomBuffer** | `setBufferAllocationCallback` | Да                       |
| **BufferView**   | Изображение в bufferView      | —                        |
| **Fallback**     | EXT_meshopt_compression       | Да                       |

## Options

| Опция                           | Эффект                                                |
|---------------------------------|-------------------------------------------------------|
| **LoadExternalBuffers**         | Загрузка внешних .bin файлов в Vector                 |
| **LoadExternalImages**          | Загрузка внешних изображений                          |
| **DecomposeNodeMatrices**       | Разложение матриц узлов на TRS                        |
| **GenerateMeshIndices**         | Генерация индексов для примитивов без indicesAccessor |
| **AllowDouble**                 | Разрешить GL_DOUBLE как componentType                 |
| **DontRequireValidAssetMember** | Пропустить проверку поля asset                        |

## Category

| Значение           | Что парсится                                       |
|--------------------|----------------------------------------------------|
| **All**            | Всё                                                |
| **OnlyRenderable** | Всё, кроме Animations, Skins                       |
| **OnlyAnimations** | Только Animations, Accessors, BufferViews, Buffers |

## Accessor Types

| Тип        | Компоненты | Пример               |
|------------|------------|----------------------|
| **Scalar** | 1          | float, uint32_t      |
| **Vec2**   | 2          | glm::vec2            |
| **Vec3**   | 3          | glm::vec3            |
| **Vec4**   | 4          | glm::vec4, glm::quat |
| **Mat2**   | 4          | glm::mat2            |
| **Mat3**   | 9          | glm::mat3            |
| **Mat4**   | 16         | glm::mat4            |

## Component Types

| Тип               | Размер           | OpenGL константа  |
|-------------------|------------------|-------------------|
| **Byte**          | 1 byte (signed)  | GL_BYTE           |
| **UnsignedByte**  | 1 byte           | GL_UNSIGNED_BYTE  |
| **Short**         | 2 bytes (signed) | GL_SHORT          |
| **UnsignedShort** | 2 bytes          | GL_UNSIGNED_SHORT |
| **UnsignedInt**   | 4 bytes          | GL_UNSIGNED_INT   |
| **Float**         | 4 bytes          | GL_FLOAT          |

## Термины которые часто путают

| Пара                      | Различие                                                                  |
|---------------------------|---------------------------------------------------------------------------|
| **Buffer vs BufferView**  | Buffer — сырые байты; BufferView — "окно" в Buffer с offset/length/stride |
| **Accessor vs Primitive** | Accessor — описание данных; Primitive — использование этих данных         |
| **Node vs Scene**         | Node — элемент иерархии; Scene — набор корневых Node                      |
| **DataSource vs Buffer**  | DataSource — откуда брать данные; Buffer — сами данные                    |
| **TRS vs Matrix**         | TRS — decomposed transform; Matrix — combined transform                   |

---

Используйте этот глоссарий как справочник при чтении других разделов.
