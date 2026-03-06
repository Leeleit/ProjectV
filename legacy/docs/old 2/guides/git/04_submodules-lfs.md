# Подмодули и Git LFS для ProjectV

**🔴 Уровень 3: Продвинутый** — Управление зависимостями и ресурсами в воксельном движке.

ProjectV имеет уникальные требования к управлению версиями из-за архитектуры (DOD/ECS), зависимостей (подмодули) и типов
контента (воксельные данные, шейдеры, ресурсы).

---

## 1. Работа с подмодулями

### Структура подмодулей ProjectV

```
external/
├── SDL/           # Оконная система и ввод
├── volk/          # Загрузчик Vulkan функций
├── VMA/           # Vulkan Memory Allocator
├── glm/           # Математическая библиотека
├── fastgltf/      # Парсер glTF
├── flecs/         # ECS система
├── JoltPhysics/   # Физический движок
├── imgui/         # Immediate-mode UI
├── miniaudio/     # Аудио библиотека
└── tracy/         # Профилировщик
```

### Клонирование с подмодулями

```bash
# Правильное клонирование (рекомендуется)
git clone --recursive https://github.com/yourname/ProjectV.git

# Если забыли --recursive
cd ProjectV
git submodule update --init --recursive
```

### Обновление подмодулей

```bash
# Обновить все подмодули до последних коммитов их веток
git submodule update --remote --recursive

# Обновить конкретный подмодуль
cd external/SDL
git checkout main
git pull origin main
cd ../..
git add external/SDL
git commit -m "chore: update SDL to latest version"
```

### Фиксация версий подмодулей

```bash
# После тестирования с новой версией
git add external/
git commit -m "chore: pin submodule versions"

# Просмотр зафиксированных версий
git submodule status
```

### Автоматизация

**Скрипт `scripts/update-submodules.sh`:**

```bash
#!/bin/bash
echo "Обновление подмодулей ProjectV..."

for dir in external/*/; do
    if [ -d "$dir/.git" ]; then
        echo "Обновление: $(basename "$dir")"
        cd "$dir"
        git fetch origin
        git checkout main
        git pull origin main
        cd ../..
    fi
done

echo "Готово! Не забудьте протестировать и закоммитить."
```

---

## 2. Git LFS для игровых ресурсов

> **Связь с философией:** Воксельные данные (SVO, чанки) — это тяжёлые бинарники с уникальными требованиями к хранению и
> передаче. См. [07_voxel-data-philosophy.md](../../philosophy/07_voxel-data-philosophy.md) для понимания структуры
> данных.

Воксельный движок работает с большими объемами данных. Git LFS обязателен для бинарных файлов.

### Типы файлов для LFS

| Категория         | Форматы                                   |
|-------------------|-------------------------------------------|
| Текстуры          | `.png`, `.jpg`, `.tga`, `.dds`, `.ktx`    |
| 3D модели         | `.gltf`, `.glb`, `.fbx`, `.obj`, `.blend` |
| Аудио             | `.wav`, `.ogg`, `.mp3`, `.flac`           |
| Видео             | `.mp4`, `.webm`, `.mov`                   |
| Воксельные данные | `.vox`, `.qb`, `.svo`                     |
| Скомпилированное  | `.spv`, `.pdb`, `.dll`                    |

### Полный `.gitattributes` для ProjectV

```gitattributes
# === Текстуры ===
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.jpeg filter=lfs diff=lfs merge=lfs -text
*.tga filter=lfs diff=lfs merge=lfs -text
*.dds filter=lfs diff=lfs merge=lfs -text
*.ktx filter=lfs diff=lfs merge=lfs -text

# === 3D модели ===
*.gltf filter=lfs diff=lfs merge=lfs -text
*.glb filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.obj filter=lfs diff=lfs merge=lfs -text
*.blend filter=lfs diff=lfs merge=lfs -text

# === Аудио ===
*.wav filter=lfs diff=lfs merge=lfs -text
*.ogg filter=lfs diff=lfs merge=lfs -text
*.mp3 filter=lfs diff=lfs merge=lfs -text
*.flac filter=lfs diff=lfs merge=lfs -text

# === Воксельные данные ===
*.vox filter=lfs diff=lfs merge=lfs -text
*.svo filter=lfs diff=lfs merge=lfs -text
*.qb filter=lfs diff=lfs merge=lfs -text

# === Скомпилированные файлы ===
*.spv filter=lfs diff=lfs merge=lfs -text

# === Исключения (обычный Git) ===
*.json -text
*.cpp -text
*.h -text
*.glsl -text
*.md -text
```

### Миграция существующих файлов в LFS

```bash
# Проверить, какие файлы нужно мигрировать
git lfs migrate info --everything

# Мигрировать по типам файлов
git lfs migrate import --include="*.png,*.jpg,*.gltf"

# Мигрировать по размеру (все файлы больше 10MB)
git lfs migrate import --above=10MB

# Принудительный пуш (перезаписывает историю!)
git push --force-with-lease
```

---

## 3. Workflow для воксельных данных

### Структура директории ресурсов

```
assets/
├── textures/
│   └── brick_wall/
│       ├── source/               # Исходники (PSD, KRA) - Git LFS
│       │   └── brick_wall.psd
│       ├── high/                 # 4K текстуры - Git LFS
│       │   ├── brick_wall_4k.png
│       │   └── brick_wall_4k_n.png
│       ├── medium/               # 2K текстуры
│       └── low/                  # 1K текстуры
├── models/
│   └── character/
│       ├── character.blend       # Исходник - Git LFS
│       └── character.glb         # Экспорт - Git LFS
├── worlds/
│   └── world1/
│       ├── chunks/               # Воксельные чанки - Git LFS
│       ├── metadata.json         # Метаданные - обычный Git
│       └── palette.bin           # Палитра - Git LFS
└── shaders/
    ├── src/                      # Исходные шейдеры - обычный Git
    │   ├── vertex.glsl
    │   └── fragment.glsl
    └── compiled/                 # SPIR-V - Git LFS
        ├── vertex.spv
        └── fragment.spv
```

### Git workflow для ресурсов

```bash
# 1. Художник создает текстуру
git add assets/textures/brick_wall/source/brick_wall.psd

# 2. Экспорт в разные LOD (автоматически или вручную)
scripts/export_texture.py assets/textures/brick_wall/source/brick_wall.psd

# 3. Коммит всех версий
git add assets/textures/brick_wall/
git commit -m "feat: add brick wall texture with LODs"

# 4. Обновление только одной версии
git add assets/textures/brick_wall/medium/brick_wall_2k.png
git commit -m "fix: update brick wall 2k normal map"
```

### Workflow для шейдеров

```bash
# 1. Изменить исходный шейдер
edit shaders/src/vertex.glsl

# 2. Скомпилировать
glslc shaders/src/vertex.glsl -o shaders/compiled/vertex.spv

# 3. Закоммитить и исходник, и бинарник
git add shaders/src/vertex.glsl shaders/compiled/vertex.spv
git commit -m "feat: update vertex shader for new lighting model"
```

---

## 4. Интеграция с DOD/ECS архитектурой

### Git паттерны для DOD

**Разделение данных и систем:**

```
src/
├── data/               # Чистые данные
│   ├── voxel_data.cpp
│   └── transform_data.cpp
├── systems/            # Системы обработки
│   ├── rendering_system.cpp
│   └── physics_system.cpp
└── components/         # ECS компоненты
    ├── voxel_component.hpp
    └── transform_component.hpp
```

**Атомарные коммиты для DOD:**

```bash
# Изменение только данных
git add src/data/voxel_data.cpp
git commit -m "perf: optimize voxel data layout for cache locality"

# Изменение только системы
git add src/systems/rendering_system.cpp
git commit -m "feat: add frustum culling to rendering system"
```

---

## 5. CI/CD с LFS и подмодулями

### GitHub Actions конфигурация

```yaml
name: ProjectV CI

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive  # Важно для подмодулей!
        lfs: true              # Важно для Git LFS!

    - name: Install Vulkan SDK
      run: choco install vulkan-sdk -y

    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --config Release

    - name: Run Tests
      run: cd build && ctest -C Release --output-on-failure

    - name: Validate Shaders
      run: python shaders/validate_shaders.py

    - name: Check Asset Sizes
      run: python scripts/check_asset_sizes.py --max-size=100MB
```

---

## 6. Best Practices

### Для разработчиков

- Всегда используйте `--recursive` при клонировании
- Коммитьте исходные шейдеры вместе со скомпилированными версиями
- Разделяйте данные и код в разных коммитах
- Используйте Conventional Commits с scope для модулей

### Для художников

- Работайте только в `assets/` директории
- Коммитьте исходники (.blend, .psd) и экспортированные версии
- Следите за размерами файлов (используйте LOD)
- Используйте стандартные форматы

### Для релиз-менеджеров

- Фиксируйте версии подмодулей перед релизом
- Проверяйте, что все LFS файлы загружены
- Тестируйте на чистой копии репозитория
- Создавайте теги с семантическим версионированием

---

## 7. Скрипты автоматизации

### Скрипт настройки проекта

**`scripts/setup-projectv.sh`:**

```bash
#!/bin/bash
echo "Настройка ProjectV development environment..."

# Клонирование с подмодулями
git clone --recursive https://github.com/yourname/ProjectV.git
cd ProjectV

# Установка Git hooks
chmod +x scripts/setup-git-hooks.sh
./scripts/setup-git-hooks.sh

# Настройка Git LFS
git lfs install

# Скачивание LFS файлов
git lfs pull

# Настройка конфигов
cp config/default/* config/user/

echo "ProjectV настроен!"
echo "Следующие шаги:"
echo "1. Установите Vulkan SDK"
echo "2. Настройте IDE (CLion/VS Code)"
echo "3. Запустите сборку: cmake -B build && cmake --build build"
```

### Скрипт проверки состояния

**`scripts/check-repo-status.sh`:**

```bash
#!/bin/bash
echo "=== Статус репозитория ProjectV ==="

echo "Подмодули:"
git submodule status

echo -e "\nLFS файлы:"
git lfs ls-files

echo -e "\nРазмер репозитория:"
git count-objects -vH

echo -e "\nПоследние коммиты:"
git log --oneline -5
```

