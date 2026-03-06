# Решение проблем Dear ImGui

**🟡 Уровень 2: Средний**

## Деревья решений

### Диагностика инициализации

```mermaid
flowchart TD
    Start["Ошибка инициализации"] --> Context{Context создан?}
    Context -- Нет --> Create["Вызвать CreateContext()"]
    Context -- Да --> Backend{Backends инициализированы?}
    Backend -- Нет --> InitBackend["Вызвать InitForVulkan/Init"]
    Backend -- Да --> Pool{Descriptor Pool валиден?}
    Pool -- Нет --> FixPool["Добавить флаг FREE_DESCRIPTOR_SET_BIT"]
    Pool -- Да --> CheckVolk{Используется volk?}
    CheckVolk -- Да --> LoadDev["Проверить volkLoadDevice"]
```

### Диагностика отрисовки

```mermaid
flowchart TD
    Start["Нет UI"] --> Frame{NewFrame вызван?}
    Frame -- Нет --> CallNewFrame["Вызвать NewFrame для всех backend'ов"]
    Frame -- Да --> Render{Render вызван?}
    Render -- Нет --> CallRender["Вызвать ImGui::Render()"]
    Render -- Да --> DrawData{RenderDrawData вызван?}
    DrawData -- Нет --> CallDraw["Вызвать RenderDrawData внутри RenderPass"]
    DrawData -- Да --> Font{Шрифт загружен?}
    Font -- Нет --> BuildFont["Проверить BuildFontAtlas"]
```

## Частые проблемы

### Белые прямоугольники вместо текста

**Причина:** Не загружена текстура шрифта.
**Решение:**

1. Проверьте Descriptor Pool.
2. Убедитесь, что `ImGui_ImplVulkan_CreateFontsTexture` выполнился (обычно внутри Init).

### Виджеты не реагируют

**Причина:** Конфликт ID или перехват ввода.
**Решение:**

1. Используйте `PushID`/`PopID` в циклах.
2. Проверьте `io.WantCaptureMouse`.

### Ошибка линковки (Unresolved external)

**Причина:** Не скомпилированы файлы backend'ов.
**Решение:** Добавьте `imgui_impl_sdl3.cpp` и `imgui_impl_vulkan.cpp` в `add_library` в CMake.

---

## См. также

- [Интеграция](integration.md) — настройка.
- [Справочник API](api-reference.md) — функции.
