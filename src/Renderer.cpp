#include "Renderer.hpp"
#include "VulkanInit.hpp"

SDL_AppResult DrawFrame(AppState *state) // Функция итераций, самая главная функция в движке: она вызывается в
										 // цикле и держит программу открытой и работающей
{
	if (!state)
		return SDL_APP_CONTINUE;

	// 1. Проверка на свернутое окно. Если ширина или высота равны 0 (окно минимизировано), мы не можем ничего рисовать. Просто ждем, пока окно снова не развернут
	if (state->extent.width == 0 || state->extent.height == 0) {
		if (!RecreateSwapchain(state)) {
			SDL_Log("RecreateSwapchain failed");
			return SDL_APP_FAILURE;
		}
		return SDL_APP_CONTINUE;
	}

	uint32_t cf = state->currentFrame; // cf = current frame
	VkCommandBuffer cmd = state->commandBuffers[cf];
	VkFence inFlightFence = state->inFlightFences[cf];
	VkSemaphore imageAvailableSemaphore = state->imageAvailableSemaphores[cf];
	VkSemaphore renderFinishedSemaphore = state->renderFinishedSemaphores[cf];

	// 2. Ожидание видеокарты (CPU-GPU синхронизация). Мы ждем, пока видеокарта закончит рисовать предыдущий кадр (на который завязан inFlightFence). Если этого не делать, CPU будет записывать команды быстрее, чем GPU их выполняет, и мы перезапишем Command Buffer прямо во время его использования
	vkWaitForFences(state->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

	// 3. Запрос следующей картинки у Swapchain
	uint32_t imageIndex = 0;
	VkResult acquireRes = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX,
												imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex); // Функция просит swapchain дать нам индекс картинки, в которую сейчас можно рисовать. Когда картинка реально будет готова (не используется монитором), Vulkan автоматически "зажжет" imageAvailableSemaphore

	// Проверяем, не устарел ли Swapchain (например, пользователь изменил размер окна). Если OUT_OF_DATE — текущий swapchain больше не подходит под окно, его нужно пересоздать
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
		if (!RecreateSwapchain(state))
			return SDL_APP_FAILURE;
		return SDL_APP_CONTINUE; // Начинаем кадр заново уже с новым swapchain
	} else if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		return SDL_APP_CONTINUE; // Произошла другая ошибка, пробуем в следующем кадре
	}

	// 4. Подготовка к записи новых команд. Только теперь мы точно знаем, что видеокарта закончила с этим кадром и мы получили новую картинку. Сбрасываем Fence (закрываем забор), чтобы следующий кадр снова ждал
	vkResetFences(state->device, 1, &inFlightFence);
	vkResetCommandBuffer(cmd, 0); // Очищаем Command Buffer от старых команд прошлого кадра (спасибо флагу RESET из шага 12 InitVulkan)

	// 5. Начало записи команд
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		SDL_Log("vkBeginCommandBuffer failed");
		return SDL_APP_FAILURE;
	}

	// 6 RenderPass
	// 6.1. БАРЬЕР: Переводим картинку из состояния UNDEFINED (какой она пришла из Swapchain) в состояние COLOR_ATTACHMENT_OPTIMAL (в которое можно безопасно рисовать).
	VkImageMemoryBarrier2 imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // Ждем готовности вывода
	imageBarrier.srcAccessMask = 0;												 // До этого нам не важен доступ
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // Блокируем вывод
	imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;		 // Мы будем ПИСАТЬ пиксели
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;							 // Не заботимся о старых пикселях
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;			 // Новый макет для отрисовки
	imageBarrier.image = state->swapchainImages[imageIndex];					 // Текущая картинка
	imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};	 // Применяем ко всему цвету

	VkDependencyInfo depInfo = {};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo); // Применяем барьер ДО начала рендеринга

	// 6.2. DYNAMIC RENDERING: Описываем, куда рисовать "на лету" без Framebuffer
	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = state->swapchainImageViews[imageIndex];		// Прямая ссылка на вид картинки
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Текущий макет (только что установили)
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					// Очищаем перед рисованием
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;					// Сохраняем после рисования
	colorAttachment.clearValue.color = {{0.0f, 1.0f, 0.0f, 1.0f}};			// Цвет очистки (черный)

	VkRenderingInfo renderInfo = {};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.extent = state->extent; // Область рисования равна размеру окна
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(cmd, &renderInfo); // Начинаем отрисовку! (Замена vkCmdBeginRenderPass)

	// ... Тут будут находиться ваши команды отрисовки (vkCmdDraw и т.д.) ...

	vkCmdEndRendering(cmd); // Заканчиваем отрисовку

	// 6.3. БАРЬЕР: Переводим картинку из состояния отрисовки в состояние для вывода на экран (PRESENT)
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; // Мы только что писали в нее
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;	 // Вывод на экран - это конец пайплайна
	imageBarrier.dstAccessMask = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Текущий статус после отрисовки
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		   // Готовим для монитора

	vkCmdPipelineBarrier2(cmd, &depInfo); // Применяем барьер ПОСЛЕ рендеринга

	if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
		SDL_Log("vkEndCommandBuffer failed");
		return SDL_APP_FAILURE;
	}

	// 7. Отправка команд на видеокарту (Submit). Мы говорим видеокарте: "Выполни эти команды, но..." В Synchronization2 семафоры и буферы описываются отдельными компактными структурами
	VkSemaphoreSubmitInfo waitSemaphoreInfo = {};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // Ждем перед выводом цвета

	VkSemaphoreSubmitInfo signalSemaphoreInfo = {};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = renderFinishedSemaphore;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // Сигналим после вывода цвета

	VkCommandBufferSubmitInfo cmdBufferInfo = {};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferInfo.commandBuffer = cmd;

	VkSubmitInfo2 submitInfo2 = {};
	submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo2.waitSemaphoreInfoCount = 1;
	submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo2.commandBufferInfoCount = 1;
	submitInfo2.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo2.signalSemaphoreInfoCount = 1;
	submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	const VkResult submitRes = vkQueueSubmit2(state->queue, 1, &submitInfo2, inFlightFence); // Отправляем команды в очередь с использованием нового API Synchronization2
	if (submitRes != VK_SUCCESS) {
		SDL_Log("vkQueueSubmit2 failed");
		return SDL_APP_FAILURE;
	}

	// 8. Вывод готового кадра на экран (Present)
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1; // Ждем, пока видеокарта реально закончит рисовать (зажжется renderFinishedSemaphore)
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &state->swapchain;
	presentInfo.pImageIndices = &imageIndex; // Какую именно картинку показывать

	const VkResult presentRes = vkQueuePresentKHR(state->queue, &presentInfo); // Просим презентационный движок показать кадр на мониторе

	// 9. Финальная проверка Swapchain после презентации. Если после показа мы узнали, что окно изменилось (OUT_OF_DATE), или стало неоптимальным (SUBOPTIMAL), или мы сами поймали ивент от SDL (windowResized) — пересоздаем Swapchain
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || state->windowResized) {
		state->windowResized = false;
		if (!RecreateSwapchain(state)) {
			SDL_Log("RecreateSwapchain failed");
			return SDL_APP_FAILURE;
		}
	} else if (presentRes != VK_SUCCESS) {
		SDL_Log("vkQueuePresentKHR failed");
		return SDL_APP_FAILURE;
	}

	// В САМОМ КОНЦЕ ИТЕРАЦИИ сдвигаем индекс кадра:
	state->currentFrame = (state->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	return SDL_APP_CONTINUE;
}
