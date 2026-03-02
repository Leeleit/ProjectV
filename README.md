# ProjectV 🧊

Высокопроизводительный воксельный движок нового поколения.
**Архитектура:** Data-Oriented Design (DOD), ECS (Flecs), Vulkan 1.4 (GPU-Driven), `stdexec` (P2300).
**Язык:** C++26 (Modules, `std::expected`, `std::mdspan`, Static Reflection).

---

## 🛑 Обязательные требования к окружению

Движок использует **кровоточащий край (bleeding edge)** технологий. Старые компиляторы это не соберут. Вам потребуются
самые свежие версии инструментов.

1. **OS:** Windows 11
2. **Компилятор:**
  * **LLVM/Clang-cl 18+** (Рекомендуется, лучшая поддержка C++26 Modules)
  * **MSVC 19.40+** (Visual Studio 2022 17.10+)
3. **CMake:** `3.38` или новее (критично для C++26 Modules).
4. **Vulkan SDK:** `1.4.304.0` или новее (Обязательно наличие Vulkan 1.4!).
   Скачать: [vulkan.lunarg.com](https://vulkan.lunarg.com/)

---

## 📥 Клонирование репозитория

Мы используем Git Submodules для всех зависимостей (Flecs, VMA, volk, SDL3, Slang). Клонировать **строго** с флагом
рекурсии:

```bash
git clone --recursive https://github.com/your-org/ProjectV.git
cd ProjectV

# Если забыли --recursive при клонировании:
# git submodule update --init --recursive
```

---

## 🛠 Настройка среды разработки (CLion)

Мы используем **CLion** + **LLVM (Clang)**.

### 1. Настройка Toolchain (Clang)

1. Установите LLVM (на Windows скачайте установщик с GitHub LLVM).
2. В CLion: `Settings` -> `Build, Execution, Deployment` -> `Toolchains`.
3. Создайте новый Toolchain, назовите его `Clang 18+`.
4. Укажите пути к C Compiler (`clang`) и C++ Compiler (`clang++`).

### 2. Настройка CMake

1. В CLion: `Settings` -> `Build, Execution, Deployment` -> `CMake`.
2. Убедитесь, что выбран Toolchain `Clang 18+`.
3. В поле `CMake options` добавьте (для быстрой компиляции MVP-версии):
   ```text
   -DPROJECTV_MVP_MODE=ON
   ```

### 3. Инструменты качества кода (Clang-Tidy & Clang-Format)

Код должен быть единообразным. Мы не спорим о табах и пробелах, за нас это делает автоматика.

1. В CLion перейдите в `Settings` -> `Editor` -> `Code Style`. Убедитесь, что включена опция **Enable ClangFormat** (
   настройки подтянутся из файла `.clang-format` в корне).
2. Перейдите в `Settings` -> `Editor` -> `Inspections` -> `C/C++` -> `Static Analysis tools` -> `Clang-Tidy`.
3. Убедитесь, что стоит галочка, файл `.clang-tidy` из корня будет использоваться автоматически.

---

## 🚀 Сборка из командной строки

Если вы предпочитаете консоль:

```bash
# 1. Генерация проекта (собираем MVP версию для быстрого старта)
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DPROJECTV_MVP_MODE=ON -DCMAKE_BUILD_TYPE=Debug

# 2. Сборка
cmake --build build --parallel
```

Для полной сборки (с физикой, Cellular Automata и т.д.) отключите MVP мод: `-DPROJECTV_MVP_MODE=OFF`.

---

## 📚 Навигация по документации (С чего начать?)

Погружение в код без понимания архитектуры ProjectV **запрещено** (вы сломаете кэш-линии и будете использовать
`std::mutex`, а мы вас за это побьем).

Вся документация лежит в папке `docs/architecture/`. Читать в следующем порядке:

1.[Roadmap & Scope](docs/architecture/roadmap_and_scope.md) — что мы делаем и границы MVP.
2.[Theory: Memory Layout](docs/architecture/theory/02_memory-layout.md) — почему мы используем SoA (Structure of Arrays)
и `alignas(64)`.

3. [Theory: ECS Concepts](docs/architecture/theory/01_ecs-concepts.md) — как работает база данных нашей игры (Flecs).
   4.[Practice: Job System](docs/architecture/practice/01_core/05_job_system.md) — как работает наша многопоточность без
   `std::thread` на базе P2300 (`stdexec`).
   5.[Practice: Vulkan Spec](docs/architecture/practice/02_render/01_vulkan_spec.md) — как мы рендерим без старых
   `VkRenderPass` через Dynamic Rendering.

---

## 📜 Главные заповеди кода ProjectV

1. **Никаких `try-catch` и `throw`.** Мы используем `std::expected<T, Error>`. Если ошибка критическая — падаем через
   `PV_ASSERT` или `std::abort()`. Fail Fast.
2. **Никаких `std::thread` и `std::mutex` в игровом цикле.** Вся асинхронность — через единый Job System (`stdexec`),
   все данные — lock-free или изолированы по потокам.
3. **Железо не врёт.** Классы с виртуальными методами (`virtual`) на тысячах объектов убивают кэш процессора (I-Cache
   misses). Используйте DOD (массивы структур) и статический полиморфизм.
4. **Zero-copy аллокации.** Не делайте `new` и `malloc` в горячих путях. Используйте `ArenaAllocator` для временных
   данных кадра и VMA для видеопамяти.
5. **Всегда профилируйте.** Оберните тяжёлый код в `PV_ZONE("Name")` для Tracy.

Добро пожаловать в хардкор.
