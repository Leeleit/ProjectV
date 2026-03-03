# Решение проблем и интеграция с IDE

**🟡 Уровень 2: Средний** — Диагностика ошибок Git, настройка CLion и VS Code.

Этот документ объединяет решение распространённых проблем Git и настройку IDE для эффективной работы с ProjectV.

---

# Часть 1: Решение проблем Git

## 1. Проблемы с подмодулями

### "submodule not found" после клонирования

```bash
# Решение: рекурсивная инициализация
git submodule update --init --recursive
```

### "detached HEAD" в подмодулях

```bash
# Вернуть подмодули к ветке main
git submodule foreach git checkout main
git submodule foreach git pull origin main
```

### Конфликты в подмодулях

```bash
cd external/SDL
git status  # Посмотреть конфликты
# Разрешить конфликты
git add .
git commit -m "Resolve merge conflict"
cd ../..
git add external/SDL
git commit -m "Update SDL submodule"
```

---

## 2. Проблемы с Git LFS

### "Could not find git-lfs"

```bash
# Установить Git LFS
# Windows: скачать с https://git-lfs.github.com/
# Linux: sudo apt install git-lfs

git lfs install
```

### Большие файлы не отслеживаются через LFS

```bash
# Добавить тип файлов
git lfs track "*.psd"
git add .gitattributes
git commit -m "chore: add .psd to LFS"
```

### "Authentication required" при работе с LFS

```bash
# Использовать SSH вместо HTTPS
git remote set-url origin git@github.com:yourusername/ProjectV.git
```

---

## 3. Проблемы с ветками и коммитами

### "Updates were rejected because the tip is behind"

```bash
# Способ 1: Rebase (предпочтительно)
git fetch origin
git rebase origin/main

# Способ 2: Merge
git merge origin/main

# Способ 3: Force push (осторожно!)
git push --force-with-lease
```

### "Your local changes would be overwritten"

```bash
# Сохранить изменения в stash
git stash
# Переключиться
git checkout other-branch
# Восстановить
git stash pop
```

### "Please tell me who you are"

```bash
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

---

## 4. Проблемы с merge/rebase

### Конфликты при слиянии

```bash
# Посмотреть конфликтующие файлы
git status

# Разрешить конфликты вручную, затем
git add <resolved-file>
git commit -m "Resolve merge conflict"

# Или использовать mergetool
git mergetool
```

### Прерванный interactive rebase

```bash
# Продолжить
git rebase --continue

# Отменить
git rebase --abort

# Пропустить текущий коммит
git rebase --skip
```

---

## 5. Проблемы с удалённым репозиторием

### "Repository not found"

```bash
# Проверить URL
git remote -v

# Изменить URL
git remote set-url origin https://github.com/user/ProjectV.git
```

### "permission denied (publickey)"

```bash
# Проверить SSH ключи
ssh -T git@github.com

# Сгенерировать новый ключ
ssh-keygen -t ed25519 -C "your.email@example.com"
ssh-add ~/.ssh/id_ed25519
```

---

## 6. Проблемы с производительностью

### Медленные операции Git

```bash
# Включить кеширование
git config --global core.fscache true

# Увеличить буфер
git config --global http.postBuffer 524288000

# Shallow clone
git clone --depth 1 https://github.com/user/ProjectV
```

### Большой размер репозитория

```bash
git gc --aggressive --prune=now
git lfs prune
```

---

## 7. Проблемы с хуками

### "pre-commit hook failed"

```bash
# Временно отключить
git commit --no-verify -m "Emergency fix"

# Или исправить проблемы, на которые указывает хук
```

### Хуки не выполняются

```bash
# Сделать исполняемыми
chmod +x .git/hooks/*
```

---

## 8. Восстановление данных

### Восстановление после ошибки

```bash
# История всех действий
git reflog

# Восстановить состояние
git reset --hard HEAD@{1}

# Восстановить удалённую ветку
git checkout -b recovered-branch SHA1
```

### Найти потерянные коммиты

```bash
git fsck --lost-found
```

---

# Часть 2: Интеграция с IDE

## 9. CLion

### Базовая настройка Git

1. **File → Settings → Version Control → Git**
2. Укажите путь: `C:\Program Files\Git\bin\git.exe`
3. Нажмите **Test**

### Горячие клавиши

| Действие     | Сочетание    |
|--------------|--------------|
| Commit       | Ctrl+K       |
| Push         | Ctrl+Shift+K |
| Pull         | Ctrl+T       |
| Show History | Alt+9        |
| Revert       | Ctrl+Alt+Z   |

### Полезные настройки

```yaml
# Автоматический fetch
Settings → Version Control → Git → "Auto-update after push"

# Inline blame
Settings → Editor → Inlay Hints → "Show inline blame"
```

### Проблема: Git не работает в CLion

1. Проверьте путь к Git в Settings
2. **File → Invalidate Caches and Restart**

---

## 10. Visual Studio Code

### Установка расширений

```bash
code --install-extension eamodio.gitlens
code --install-extension mhutchie.git-graph
```

### Настройки Git

```json
{
  "git.enabled": true,
  "git.autoRepositoryDetection": "subFolders",
  "git.confirmSync": false,
  "git.enableSmartCommit": true,
  "editor.formatOnSave": true
}
```

### Проблема: VS Code не видит подмодули

```json
{
  "git.autoRepositoryDetection": "subFolders",
  "git.ignoreSubmodules": false
}
```

---

## 11. Git GUI Tools

### GitKraken (рекомендуется)

- Визуализация сложных операций
- Поддержка Git LFS
- Интеграция с GitHub Actions

### SourceTree

- Бесплатный GUI от Atlassian
- Поддержка Git Flow

### Fork

- Современный интерфейс
- Быстрая работа с большими репозиториями

---

## 12. Автоматизация в IDE

### clang-format при сохранении

**CLion:** File → Settings → Tools → File Watchers → Добавить clang-format

**VS Code:**

```json
{
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "xaver.clang-format"
}
```

### Сниппет для Conventional Commits (VS Code)

```json
{
  "Conventional Commit": {
    "prefix": "cc",
    "body": [
      "${1|feat,fix,docs,style,refactor,perf,test,chore|}: ${2:description}"
    ]
  }
}
```

### Task для Git (VS Code)

```json
// .vscode/tasks.json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Git: Update All",
      "type": "shell",
      "command": "git pull && git submodule update --init --recursive"
    }
  ]
}
```

---

## 13. Диагностические скрипты

### Проверка состояния репозитория

```bash
#!/bin/bash
echo "=== Диагностика Git для ProjectV ==="
echo "Git version: $(git --version)"
echo "Branch: $(git branch --show-current)"
echo "Last commit: $(git log -1 --oneline)"
echo "Submodules:"
git submodule status
echo "LFS files:"
git lfs ls-files 2>/dev/null || echo "LFS не используется"
echo "Repo size:"
git count-objects -vH | grep size-pack
```

### Очистка репозитория

```bash
#!/bin/bash
echo "Очистка репозитория ProjectV..."
git clean -fd
git reset --hard HEAD
git submodule update --init --recursive
git gc --aggressive --prune=now
echo "Готово!"
```

---

## 14. Быстрый справочник

| Проблема               | Решение                                    |
|------------------------|--------------------------------------------|
| Подмодули не загружены | `git submodule update --init --recursive`  |
| LFS не работает        | `git lfs install && git lfs pull`          |
| Ветка отстаёт          | `git rebase origin/main`                   |
| Конфликт при merge     | Разрешить вручную, `git add`, `git commit` |
| Потерян коммит         | `git reflog`, `git reset --hard HEAD@{N}`  |
| Хук блокирует          | `git commit --no-verify`                   |
| Большой размер         | `git gc --aggressive --prune=now`          |

