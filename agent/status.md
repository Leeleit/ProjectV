# Status

Текущее состояние проекта и ближайший рабочий фокус.

Дата последнего обновления: `2026-04-07`

---

## 1. Текущий статус

Проект находится в стадии `pre-MVP alpha / ранний vertical slice`.

Сильная сторона:

- базовый voxel rendering pipeline уже существует и работает.
- интерактивный editing loop уже поднят до уровня CPU raycast + runtime remove/place block.
- selection visual feedback уже поднят до отдельного overlay path с block highlight и crosshair.
- debug stats уже перенесены в in-app HUD с hotkey toggle, camera/selection telemetry, корректной top-left screen-space привязкой и базовым static-analysis cleanup в input/test glue, включая test fixture setup.

Главный разрыв:

- до честного sandbox MVP всё ещё не хватает прогнать resize / restore / shutdown после интерактивных edits и собрать понятный smoke checklist.

---

## 2. Ближайший рабочий milestone

Ближайшая цель:

- интерактивный voxel MVP с уже работающими block picking, remove/place loop, block highlight, crosshair и базовым HUD, плюс проверенный runtime stability path и smoke checklist.

Это главный критерий, который отделяет текущий рендер-прототип от настоящего раннего MVP.

---

## 3. Что уже сделано по организации проекта

Уже добавлено:

- корневой `TODO.md` как живой roadmap;
- корневой `AGENTS.md` как обязательный протокол работы;
- папка `agent/` для памяти, статуса и решений.

---

## 4. Что сейчас рекомендуется делать дальше

Приоритетный порядок:

1. Проверить resize / restore / shutdown после интерактивных world edits, HUD и overlay path.
2. Подготовить smoke checklist для интерактивного voxel MVP.
3. Досинхронизировать корневую документацию и authored docs с новым interaction/render loop.
4. После этого заходить в ECS/physics/save-load.

---

## 5. Что не должно сбить фокус

Пока не делать главным направлением:

- `SVO`
- mesh shaders
- большой renderer rewrite
- тяжёлый job system заранее
- complex editor
- мультиплеер
- модульную/плагинную платформу

Это допустимо только как отдельный R&D-уголок без блокировки mainline.

---

## 6. Рабочие риски

- Корневой roadmap может снова устареть, если его не обновлять после каждой заметной задачи.
- Legacy-документы могут уводить в сторону foundation/R&D, если забыть, что код уже ушёл дальше.
- Преждевременная оптимизация и архитектурный перфекционизм могут затормозить появление живого MVP.
- Неаккуратная работа с рабочим деревом может затронуть чужие изменения в `README.md` и `external/tracy`.

---

## 7. Обязательное обновление после следующей заметной задачи

После следующей содержательной задачи нужно:

- обновить соответствующие пункты в `TODO.md`;
- обновить этот файл;
- при необходимости дополнить `memory.md`;
- при наличии долгоживущего решения обновить `decisions.md`.
