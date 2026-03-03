# Производительность Slang

**🟡 Уровень 2: Средний** — Оптимизации производительности компиляции и выполнения шейдеров Slang.

---

## Оптимизация времени компиляции

### Модульная компиляция

Модульная система Slang позволяет компилировать только изменённые модули:

```
Изменённый модуль → Перекомпиляция только этого модуля
                         ↓
Кэшированные модули → Линковка → SPIR-V
```

При изменении одного модуля в большой кодобазе (100+ файлов):

- **GLSL**: полная перекомпиляция (10-30 секунд)
- **Slang**: инкрементальная перекомпиляция (1-5 секунд)

### Параметры компиляции

```bash
# Для разработки: быстрая компиляция
slangc shader.slang -o shader.spv -target spirv -O1

# Для production: максимальная оптимизация
slangc shader.slang -o shader.spv -target spirv -O3

# Параллельная компиляция (Slang API)
# session->setEnableParallelCompilation(true);
```

### Кэширование модулей

```cpp
// Включение кэша на диске через Slang API
slang::SessionDesc sessionDesc{};
sessionDesc.enableEffectCache = true;
sessionDesc.effectCachePath = "build/.slang_cache";
```

### Структура модулей для кэширования

```
shaders/
├── core/           # Редко меняется → кэшируется надолго
│   ├── types.slang
│   └── math.slang
├── materials/      # Иногда меняется
│   └── pbr.slang
└── render/         # Часто меняется → перекомпилируется
    └── main.slang
```

---

## Оптимизация времени выполнения

### Специализация generics

```slang
// Generic шейдер
generic<T>
struct Processor
{
    T process(T input) { return input * 2.0; }
};

// Явная специализация для конкретного типа
specialized<float>
struct Processor<float>
{
    float process(float input)
    {
        // Компилятор может применить специфичные оптимизации
        return fma(input, 2.0, 0.0);  // FMA инструкция
    }
};
```

### Инлайнинг функций

```slang
// Принудительный инлайн для критичных функций
[ForceInline]
float criticalCalculation(float x)
{
    return x * 2.0 + 1.0;
}

// Запрет инлайна для больших функций
[NoInline]
float complexNoise(float3 p)
{
    // Сложные вычисления, инлайн ухудшит производительность
    return fractalNoise(p, 8);
}
```

### Группировка данных

```slang
// SOA (Structure of Arrays) для лучшей cache locality
struct VoxelDataSOA
{
    float* densities;   // Все densities подряд
    float3* colors;     // Все colors подряд
    uint* materials;    // Все materials подряд
};

// Вместо AOS (Array of Structures)
struct VoxelDataAOS
{
    float density;
    float3 color;
    uint material;
};
// Плохо для cache locality при обработке одного поля
```

### Shared memory в compute shaders

```slang
[numthreads(32, 32, 1)]
void csMain(uint3 id : SV_DispatchThreadID, uint3 localId : SV_GroupThreadID)
{
    // Shared memory для данных, используемых в группе
    groupshared float sharedData[32][32];

    // Загрузка в shared memory
    sharedData[localId.x][localId.y] = input[id.xy];

    // Синхронизация перед использованием
    GroupMemoryBarrierWithGroupSync();

    // Обработка с быстрым доступом к shared memory
    float result = process(sharedData, localId);
    output[id.xy] = result;
}
```

---

## Сравнение с GLSL/HLSL

### Время компиляции

| Сценарий                       | GLSL               | Slang (первая компиляция) | Slang (с кэшем) |
|--------------------------------|--------------------|---------------------------|-----------------|
| Большая кодобаза (100+ файлов) | 10-30 сек          | 15-45 сек                 | 1-5 сек         |
| Изменение одного файла         | 10-30 сек (полная) | 1-3 сек (инкремент)       | 0.5-2 сек       |
| Маленький проект (1-5 файлов)  | 0.1-0.5 сек        | 0.2-0.8 сек               | 0.1-0.3 сек     |

### Размер SPIR-V

| Метрика               | GLSL    | Slang  |
|-----------------------|---------|--------|
| Размер бинарника      | Базовый | +0-10% |
| Количество инструкций | Базовое | +0-5%  |
| Время выполнения      | Базовое | +0-5%  |

Разница в размере и производительности выполнения минимальна, так как оба компилируются в один и тот же SPIR-V.

---

## Профилирование

### Измерение времени компиляции

```bash
# Встроенное измерение времени фаз
slangc shader.slang -target spirv -o shader.spv -time-phases
```

### Slang API: измерение

```cpp
auto start = std::chrono::high_resolution_clock::now();

Slang::ComPtr<slang::IBlob> spirvCode;
linkedProgram->getEntryPointCode(0, 0, spirvCode.writeRef(), nullptr);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Compilation time: " << duration.count() << "ms\n";
std::cout << "SPIR-V size: " << spirvCode->getBufferSize() << " bytes\n";
```

### Анализ SPIR-V

```bash
# Генерация SPIR-V ассемблера для анализа
slangc shader.slang -target spirv-asm -o shader.spvasm

# Подсчёт инструкций
grep -c "Op" shader.spvasm
```

---

## Рекомендации

### Организация кода

1. **Выносить стабильный код в модули**: типы данных, математика, константы
2. **Избегать циклических зависимостей**: модули должны иметь чёткую иерархию
3. **Минимизировать зависимости между модулями**: меньше зависимостей — быстрее компиляция

### Выбор стратегии компиляции

| Сценарий       | Стратегия                              |
|----------------|----------------------------------------|
| Финальный билд | Offline компиляция через CMake, -O3    |
| Разработка     | Runtime компиляция с кэшированием, -O1 |
| CI/CD          | Offline компиляция, проверка ошибок    |

### Память компилятора

```cpp
// Освобождение ресурсов после компиляции
session = nullptr;
globalSession = nullptr;
// Сборка мусора Slang
```

---

## Типичные проблемы

### Медленная компиляция

**Причины:**

- Большое количество модулей с перекрёстными зависимостями
- Глубокая вложенность `#include` или `import`
- Сложные generic-конструкции без специализации

**Решения:**

- Реорганизовать структуру модулей
- Использовать кэширование
- Явно специализировать generics для часто используемых типов

### Большой размер SPIR-V

**Причины:**

- Отладочная информация (`-g`)
- Неоптимизированный код
- Дублирование кода через copy-paste

**Решения:**

- Использовать `-O` или `-O2`
- Убрать отладочную информацию для production
- Вынести общий код в функции или модули
