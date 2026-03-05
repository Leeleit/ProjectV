# Стратегия ветвления и культура коммитов

**🟢 Уровень 1: Начинающий** — GitHub Flow, feature branches и Conventional Commits.

Чистая история изменений и правильная работа с ветками — залог понимания проекта новыми разработчиками и эффективной
работы в команде.

---

## 1. Стратегия ветвления (GitHub Flow)

Для ProjectV используется адаптированный **GitHub Flow / Feature Branch Workflow**.

### Основные ветки

#### `main`

- **Стабильность:** Всегда в состоянии "собирается без ошибок" (build passing)
- **Защита:** Защищена от прямых коммитов (`push force` запрещен)
- **Назначение:** Версия кода, готовая к релизу

#### `develop` (опционально)

Используется, если команда решит разделять релизы от разработки. Для небольших команд достаточно одной `main`.

### Feature Branches

Вся разработка ведется в отдельных ветках, ответвляемых от `main`.

**Именование:** `тип/краткое-описание-на-английском`

| Префикс     | Назначение             | Пример                  |
|-------------|------------------------|-------------------------|
| `feat/`     | Новая функциональность | `feat/voxel-rendering`  |
| `fix/`      | Исправление бага       | `fix/crash-on-load`     |
| `docs/`     | Документация           | `docs/git-workflow`     |
| `refactor/` | Рефакторинг            | `refactor/cleanup-main` |
| `perf/`     | Оптимизация            | `perf/chunk-meshing`    |

**Создание ветки:**

```bash
# Обновиться до актуального состояния main
git checkout main
git pull origin main

# Создать новую ветку
git checkout -b feat/new-physics-system
```

---

## 2. Процесс разработки (Workflow)

```
main ─────●────────────●────────────●────────►
          │            │            │
          │ feat/voxel │            │
          │    │       │            │
          │    ├──●──●─┤            │
          │    │  │  │ │            │
          │    │  │  │ │ fix/crash  │
          │    │  │  │ │    │       │
          │    │  │  │ ├──●──●──────┤
          │    │  │  │ │            │
          └────┴──┴──┴─┴────────────┘
               PR  PR
```

### Шаги:

1. **Start:** Создайте ветку (`feat/name`) от актуального `main`
2. **Work:** Пишите код, делайте атомарные коммиты
3. **Sync:** Регулярно подтягивайте изменения из `main`:
   ```bash
   git fetch origin
   git rebase origin/main
   ```
4. **Push:** Отправьте ветку на сервер:
   ```bash
   git push -u origin feat/name
   ```
5. **Pull Request:** Создайте PR в интерфейсе GitHub/GitLab
6. **Merge:** После аппрува и успешных тестов
7. **Cleanup:** Удалите ветку

---

## 3. Атомарные коммиты

Каждый коммит должен быть **атомарным** — решать **одну** задачу.

**❌ Плохо:**

```
Коммит: "Fix bugs and cleanup code"
Внутри: исправлен рендеринг, переименована переменная, добавлен .gitignore
```

Почему плохо: Невозможно откатить только одно изменение.

**✅ Хорошо:**

```
Коммит 1: fix: resolve crash in physics system
Коммит 2: refactor: rename velocity to linear_velocity
Коммит 3: chore: update .gitignore
```

Почему хорошо: Можно легко откатить или cherry-pick каждое изменение.

---

## 4. Conventional Commits

Мы следуем упрощенной версии [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### Типы (Types)

| Тип        | Назначение                           | Пример                                 |
|------------|--------------------------------------|----------------------------------------|
| `feat`     | Новая функциональность               | `feat: add voxel chunk meshing`        |
| `fix`      | Исправление ошибки                   | `fix: correct jump height calculation` |
| `docs`     | Изменения в документации             | `docs: update setup guide`             |
| `style`    | Форматирование (не влияет на логику) | `style: format main.cpp`               |
| `refactor` | Рефакторинг без изменения поведения  | `refactor: simplify collision loop`    |
| `perf`     | Улучшение производительности         | `perf: optimize raycasting`            |
| `test`     | Добавление/исправление тестов        | `test: add inventory tests`            |
| `chore`    | Обслуживание (сборка, зависимости)   | `chore: bump version to 1.2.0`         |

### Правила описания

1. **Повелительное наклонение:** "Add feature", а не "Added feature"
2. **Английский язык:** Для совместимости с инструментами
3. **Без точки в конце:** Ограничение 50-72 символа

### Тело (опционально)

Для сложных изменений добавьте детали после пустой строки:

```
feat: implement greedy meshing for chunks

This reduces vertex count by 40% in flat terrain scenarios.
Previously, we were drawing every single face.

Ref: #123
```

---

## 5. Разрешение конфликтов

Конфликты случаются, когда два человека правили одни и те же строки.

**Пример конфликта:**

```cpp
<<<<<<< HEAD
int speed = 10; // Ваш код
=======
int speed = 20; // Код из main
>>>>>>> main
```

**Решение:**

1. Отредактируйте файл, оставив правильный вариант
2. Удалите маркеры `<<<<`, `====`, `>>>>`
3. Добавьте файл: `git add file.cpp`
4. Завершите: `git rebase --continue` или `git commit`

---

## 6. Чего избегать

- `WIP` коммитов в `main` (в фича-ветках допустимо, но перед мержем — squash)
- Пустых сообщений: `fix`, `upd`, `...`
- Закомментированного кода (он есть в истории Git)
- Коммитов "на потом" с кучей изменений

---

## 7. Быстрый справочник

### Частые команды

```bash
# Создать ветку
git checkout -b feat/my-feature

# Посмотреть статус
git status

# Добавить файлы
git add .

# Закоммитить
git commit -m "feat: add my feature"

# Отправить на сервер
git push -u origin feat/my-feature

# Обновиться из main
git fetch origin
git rebase origin/main

# Разрешить конфликт и продолжить
git add .
git rebase --continue
```

### Пример хорошего workflow

```bash
# 1. Начало работы
git checkout main
git pull origin main
git checkout -b feat/chunk-loading

# 2. Работа
git add src/chunk.cpp
git commit -m "feat: add chunk loading from disk"

git add tests/chunk_test.cpp
git commit -m "test: add chunk loading tests"

# 3. Синхронизация перед PR
git fetch origin
git rebase origin/main

# 4. Отправка
git push -u origin feat/chunk-loading

# 5. Создать PR через GitHub/GitLab
```

