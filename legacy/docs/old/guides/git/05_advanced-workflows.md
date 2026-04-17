# Продвинутые техники Git

**🔴 Уровень 3: Продвинутый** — Interactive rebase, bisect, cherry-pick и Git hooks.

В сложных ситуациях базовых команд недостаточно. Этот документ описывает продвинутые сценарии для поддержания чистоты
репозитория ProjectV.

---

## 1. Interactive Rebase

**Цель:** Очистить историю коммитов перед слиянием ветки.

Часто в процессе разработки появляются коммиты вроде `fix typo`, `wip`, `try again`. Их не нужно тащить в `main`.

**Команда:**

```bash
# Переписать последние N коммитов
git rebase -i HEAD~3
```

**Редактор откроется со списком:**

```
pick 7f6d234 feat: add physics component
pick 2d8b123 fix: typo in physics
pick 9a1c456 fix: crash on start
```

| Команда        | Действие                        |
|----------------|---------------------------------|
| `pick`         | Оставить как есть               |
| `squash` / `s` | Объединить с предыдущим         |
| `reword` / `r` | Изменить сообщение              |
| `drop` / `d`   | Удалить коммит                  |
| `edit` / `e`   | Остановиться для редактирования |

**Пример (объединение 3 коммитов в 1):**

```
pick 7f6d234 feat: add physics component
squash 2d8b123 fix: typo in physics
squash 9a1c456 fix: crash on start
```

> ⚠️ **Никогда не делайте rebase публичных веток (`main`)** — это перепишет историю и сломает работу коллег!

---

## 2. Stash (временное хранилище)

**Ситуация:** Вы работаете над фичей, но нужно срочно переключиться на `hotfix` в другой ветке. Коммитить незавершенную
работу нельзя.

```bash
# Сохранить изменения в стек
git stash save "wip: physics implementation"

# Рабочая директория теперь чистая
git checkout hotfix/critical-crash

# ... работа над hotfix ...
git commit -m "fix: critical crash"

# Вернуться и восстановить изменения
git checkout feat/physics
git stash pop
```

**Команды stash:**

| Команда           | Действие                          |
|-------------------|-----------------------------------|
| `git stash list`  | Показать список сохранений        |
| `git stash apply` | Применить, но не удалять из стека |
| `git stash pop`   | Применить и удалить из стека      |
| `git stash drop`  | Удалить последнее сохранение      |
| `git stash clear` | Очистить весь стек                |

---

## 3. Bisect (поиск бага)

**Ситуация:** Вчера все работало, сегодня крашится. Нужно найти коммит, который сломал билд.

```bash
git bisect start
git bisect bad                 # Текущий коммит сломан
git bisect good v1.0.0         # Эта версия работала
# Git автоматически переключится на середину диапазона

# Проверяете билд, затем:
git bisect good   # или git bisect bad

# После нахождения виновного коммита:
git bisect reset
```

> **Для небольших команд:** Bisect используется редко. Обычно проще посмотреть последние коммиты через
`git log --oneline -10`.

---

## 4. Cherry-pick

**Ситуация:** Нужно взять один конкретный коммит из другой ветки, не мержа всю ветку.

```bash
# Скопировать коммит в текущую ветку
git cherry-pick <commit-hash>

# Если нужны несколько коммитов
git cherry-pick <hash1> <hash2> <hash3>

# Диапазон коммитов
git cherry-pick <start-hash>..<end-hash>
```

Git скопирует изменения и создаст новый коммит. Могут возникнуть конфликты — решаются как обычно.

---

## 5. Git Hooks

Git hooks — скрипты, которые запускаются автоматически при определенных событиях.

### Типы hooks

| Хук             | Когда запускается        | Использование                 |
|-----------------|--------------------------|-------------------------------|
| `pre-commit`    | Перед созданием коммита  | Форматирование, линтинг       |
| `commit-msg`    | После ввода сообщения    | Проверка Conventional Commits |
| `pre-push`      | Перед отправкой          | Тесты, сборка                 |
| `post-checkout` | После переключения ветки | Обновление подмодулей         |

### pre-commit: Автоматическое форматирование

**`.git/hooks/pre-commit`:**

```bash
#!/bin/bash

if ! command -v clang-format &> /dev/null; then
    echo "clang-format не установлен"
    exit 1
fi

echo "Форматирование измененных файлов..."
git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|h|hpp)$' | while read file; do
    if [ -f "$file" ]; then
        clang-format -i --style=file "$file"
        git add "$file"
    fi
done
```

### commit-msg: Проверка Conventional Commits

**`.git/hooks/commit-msg`:**

```bash
#!/bin/bash

COMMIT_MSG=$(cat "$1")
PATTERN="^(feat|fix|docs|style|refactor|perf|test|chore)(\([a-z-]+\))?: .{1,72}$"

if ! echo "$COMMIT_MSG" | grep -qE "$PATTERN"; then
    echo "Ошибка: Сообщение не соответствует Conventional Commits"
    echo "Формат: <type>[scope]: <description>"
    echo "Пример: feat: add voxel chunk meshing"
    exit 1
fi
```

### Установка hooks

```bash
# Сделать исполняемыми
chmod +x .git/hooks/pre-commit
chmod +x .git/hooks/commit-msg

# Отключить для одного коммита
git commit --no-verify -m "Emergency fix"
```

---

## 6. Reflog (история действий)

**Ситуация:** Случайно удалили ветку или сделали `reset --hard` не на тот коммит.

```bash
# Показать историю всех действий
git reflog

# Пример вывода:
# a1b2c3d HEAD@{0}: reset: moving to HEAD~1
# d4e5f6g HEAD@{1}: commit: feat: add feature
# g7h8i9j HEAD@{2}: checkout: moving from feat to main

# Восстановить состояние
git reset --hard HEAD@{1}

# Восстановить удаленную ветку
git checkout -b recovered-branch HEAD@{2}
```

---

## 7. Очистка репозитория

```bash
# Сжать репозиторий (выполнять периодически)
git gc --aggressive --prune=now
```

---

## 8. Быстрый справочник

| Сценарий                     | Команда                  |
|------------------------------|--------------------------|
| Объединить коммиты           | `git rebase -i HEAD~N`   |
| Сохранить изменения временно | `git stash`              |
| Найти баг в истории          | `git bisect start`       |
| Скопировать коммит           | `git cherry-pick <hash>` |
| Восстановить после ошибки    | `git reflog`             |
| Очистить репозиторий         | `git gc --aggressive`    |
| Отключить hooks              | `git commit --no-verify` |

