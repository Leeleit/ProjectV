# Работа в команде

**🟡 Уровень 2: Средний** — Pull Requests, код-ревью и CI/CD.

Эффективная работа в команде требует четких процессов. Этот документ описывает workflow для совместной разработки
ProjectV.

---

## 1. Pull Request Process

Pull Request (PR) — основной механизм внесения изменений в ProjectV.

### Когда создавать PR?

- Новая функциональность готова к интеграции
- Исправление бага завершено
- Рефакторинг, затрагивающий несколько файлов
- Существенные изменения в документации

### Структура хорошего PR

**Заголовок:** Следует Conventional Commits

```
feat: add voxel chunk meshing
fix: resolve crash in physics system
docs: update git workflow documentation
```

**Шаблон описания PR:**

```markdown
## Что сделано
- Добавлен алгоритм greedy meshing для воксельных чанков
- Оптимизировано использование памяти на 40%
- Добавлены unit tests

## Почему это нужно
Текущая реализация создает слишком много вершин для плоских поверхностей.

## Как тестировалось
- [x] Сборка проходит без ошибок
- [x] Все существующие тесты проходят
- [x] Добавлены новые тесты

## Скриншоты (если применимо)
![Screenshot](link)

## Связанные Issues
Closes #123
```

---

## 2. Код-ревью

Код-ревью — критически важный процесс для поддержания качества кода.

### Принципы ревью

**Для ревьювера:**

- Фокусируйтесь на качестве кода, а не на стиле (стиль проверяет clang-format)
- Задавайте вопросы, а не давайте указания
- Объясняйте, *почему* что-то нужно изменить
- Отвечайте в течение 24 часов

**Для автора:**

- Готовьте PR к ревью (тесты проходят, код отформатирован)
- Отвечайте на комментарии своевременно
- Не принимайте комментарии на личный счет

### Чек-лист для ревьювера

- [ ] Код следует стандартам ProjectV (DOD/ECS где применимо)
- [ ] Нет очевидных багов или уязвимостей
- [ ] Тесты покрывают новый функционал
- [ ] Документация обновлена
- [ ] Производительность не ухудшена
- [ ] Нет дублирования кода
- [ ] Имена переменных/функций понятны
- [ ] Обработка ошибок присутствует

---

## 3. CI/CD Pipeline

ProjectV использует GitHub Actions для автоматической проверки кода.

### Что проверяет CI:

1. **Сборка** — компиляция на разных платформах
2. **Тесты** — запуск unit tests
3. **Линтинг** — проверка clang-format и clang-tidy
4. **Статический анализ** — проверка качества кода

### Конфигурация `.github/workflows/build.yml`:

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive
        lfs: true

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
```

### Защита ветки `main`

Настройте в GitHub Settings → Branches:

- Require pull request before merging
- Require status checks to pass
- Require conversation resolution

---

## 4. Разрешение конфликтов в команде

### Конфликты кода (Merge conflicts)

- Используйте `git merge` или `git rebase`
- При сложных конфликтах — pair programming
- Обсуждайте спорные решения в PR comments

### Конфликты архитектурных решений

- Создайте ADR (Architecture Decision Record)
- Проведите дизайн-ревью с командой
- Используйте голосование при отсутствии консенсуса

### Конфликты приоритетов

- Используйте GitHub Issues с labels
- Определяйте приоритеты на weekly meeting
- Следуйте roadmap проекта

---

## 5. Code Ownership

### `.github/CODEOWNERS`:

```
# Ядро движка
/src/engine/              @team-engine
/src/vulkan/              @team-graphics
/src/physics/             @team-physics

# Инструменты
/tools/                   @team-tools

# Ресурсы
/assets/                  @team-art

# Документация
/docs/                    @team-docs

# Конфигурации
CMakeLists.txt            @team-engine
```

Code owner автоматически запрашивается на ревью при изменении соответствующих файлов.

---

## 6. Hotfix Workflow

Для срочных исправлений используется упрощенный процесс:

```bash
# 1. Создать ветку от main
git checkout main
git pull origin main
git checkout -b hotfix/critical-crash

# 2. Исправить проблему
git add .
git commit -m "fix: resolve critical crash on startup"

# 3. Быстрый PR
git push -u origin hotfix/critical-crash

# 4. После минимального ревью (1 ревьювер) — мерж
# 5. Создать follow-up issue для полноценного исправления
```

---

## 7. Инструменты для командной работы

### Обязательные:

| Инструмент    | Назначение                    |
|---------------|-------------------------------|
| GitHub/GitLab | Хостинг кода, PR, issues      |
| Git           | Система контроля версий       |
| CMake         | Система сборки                |
| clang-format  | Автоматическое форматирование |

### Рекомендуемые:

| Инструмент    | Назначение            |
|---------------|-----------------------|
| Discord/Slack | Коммуникация          |
| Trello/Notion | Управление задачами   |
| Figma/Miro    | Дизайн и планирование |

---

## 8. Стандартный workflow

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Планирование │───►│  Разработка  │───►│  Тестирование │
│  (issue)      │    │  (branch)    │    │  (локально)   │
└─────────────┘    └─────────────┘    └─────────────┘
                                            │
                                            ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│    Мерж      │◄───│    Ревью     │◄───│  Создание PR  │
│   (squash)   │    │   (review)   │    │              │
└─────────────┘    └─────────────┘    └─────────────┘
```

### Шаги:

1. **Планирование** — создание issue, обсуждение
2. **Разработка** — создание branch, написание кода
3. **Тестирование** — локальные тесты, написание unit tests
4. **PR** — описание изменений, запрос ревью
5. **Ревью** — обсуждение, правки
6. **Мерж** — squash and merge в main

