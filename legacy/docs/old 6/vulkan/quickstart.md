# Быстрый старт Vulkan [🟢 Уровень 1]

**🟢 Уровень 1: Начинающий** — Рисование треугольника.

## Шаг 1: Инициализация (псевдокод)

```cpp
// 1. Instance
vkCreateInstance(...);

// 2. Surface (через SDL)
SDL_Vulkan_CreateSurface(...);

// 3. Device
vkCreateDevice(...);

// 4. Swapchain
vkCreateSwapchainKHR(...);
```

## Шаг 2: Pipeline

```cpp
// Шейдеры -> Layout -> Pipeline
vkCreateGraphicsPipelines(...);
```

## Шаг 3: Рендеринг (цикл)

```cpp
// 1. Получить изображение
vkAcquireNextImageKHR(...);

// 2. Записать команды
vkBeginCommandBuffer(cmd, ...);
vkCmdBeginRenderPass(cmd, ...);
vkCmdBindPipeline(cmd, ...);
vkCmdDraw(cmd, 3, 1, 0, 0); // Треугольник
vkCmdEndRenderPass(cmd);
vkEndCommandBuffer(cmd);

// 3. Отправить и показать
vkQueueSubmit(queue, ...);
vkQueuePresentKHR(queue, ...);
```
