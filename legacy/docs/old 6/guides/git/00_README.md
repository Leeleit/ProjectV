# Git Workflow для ProjectV

Полное руководство по работе с Git в проекте ProjectV — от базовой настройки до продвинутых техник для воксельного
движка.

---

## Содержание

| # | Документ                                                 | Уровень        | Описание                              |
|---|----------------------------------------------------------|----------------|---------------------------------------|
| 1 | [Настройка Git](01_basics-setup.md)                      | 🟢 Начинающий  | Базовая конфигурация, алиасы, Git LFS |
| 2 | [Стратегия ветвления и коммиты](02_branching-commits.md) | 🟢 Начинающий  | GitHub Flow, Conventional Commits     |
| 3 | [Работа в команде](03_collaboration.md)                  | 🟡 Средний     | Pull Requests, код-ревью, CI/CD       |
| 4 | [Подмодули и Git LFS](04_submodules-lfs.md)              | 🔴 Продвинутый | Управление зависимостями и ресурсами  |
| 5 | [Продвинутые техники](05_advanced-workflows.md)          | 🔴 Продвинутый | Rebase, bisect, cherry-pick, hooks    |
| 6 | [Решение проблем и IDE](06_troubleshooting-ide.md)       | 🟡 Средний     | Диагностика, CLion, VS Code           |

---

## Learning Path

### Для начинающих

```
01_basics-setup.md → 02_branching-commits.md → 03_collaboration.md
```

### Для опытных разработчиков

```
04_submodules-lfs.md → 05_advanced-workflows.md → 06_troubleshooting-ide.md
```

---

## Быстрый старт

### Первая настройка

```bash
# 1. Клонировать проект с подмодулями
git clone --recursive https://github.com/yourname/ProjectV.git

# 2. Настроить Git
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# 3. Установить Git LFS
git lfs install
```

### Ежедневный workflow

```bash
# Создать ветку для задачи
git checkout -b feat/my-feature

# Работать...
git add .
git commit -m "feat: add my feature"

# Отправить и создать PR
git push -u origin feat/my-feature
```

---

## Основные правила

### Именование веток

| Префикс     | Назначение             | Пример                  |
|-------------|------------------------|-------------------------|
| `feat/`     | Новая функциональность | `feat/voxel-rendering`  |
| `fix/`      | Исправление бага       | `fix/crash-on-load`     |
| `docs/`     | Документация           | `docs/git-workflow`     |
| `refactor/` | Рефакторинг            | `refactor/cleanup-main` |

### Conventional Commits

```
<type>[scope]: <description>

Примеры:
feat: add voxel chunk meshing
fix: resolve crash in physics system
docs: update setup guide
```

**Типы:** `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`

---

## Частые команды

| Задача                    | Команда                                   |
|---------------------------|-------------------------------------------|
| Обновить подмодули        | `git submodule update --init --recursive` |
| Скачать LFS файлы         | `git lfs pull`                            |
| Объединить коммиты        | `git rebase -i HEAD~N`                    |
| Найти баг в истории       | `git bisect start`                        |
| Восстановить после ошибки | `git reflog`                              |
| Отключить hooks           | `git commit --no-verify`                  |

---

## Структура ProjectV

```
ProjectV/
├── src/           # Исходный код движка
├── external/      # Подмодули Git (SDL, Vulkan, ECS...)
├── assets/        # Ресурсы (Git LFS)
├── docs/          # Документация
└── tests/         # Тесты
```

---

## Дополнительные ресурсы

- [Conventional Commits](https://www.conventionalcommits.org/)
- [Git Documentation](https://git-scm.com/doc)
- [GitHub Flow](https://docs.github.com/en/get-started/quickstart/github-flow)
- [Git LFS](https://git-lfs.github.com/)

---

← [К документации ProjectV](../README.md)
