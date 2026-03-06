# ProjectV – Воксельный движок

## Гайд по установке и настройке окружения

1. Установить [Clion](https://www.jetbrains.com/clion/download/download-thanks.html?platform=windows) (нужен VPN) (это
   платная программа. Чтобы активировать, следуйте инструкции на [сайте](https://306.antroot.ru/jetbrains-activation)) (
   вы можете использовать любой другой редактор/IDE, но Clion, я считаю, – лучший вариант).
2. Установить [LLVM](https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.0/LLVM-22.1.0-win64.exe)  (
   можно сначала 2, а потом 1).
3. Установить [Vulkan SDK](https://sdk.lunarg.com/sdk/download/1.4.341.1/windows/vulkansdk-windows-X64-1.4.341.1.exe)
4. Установить [Microsoft Visual Studio Build Tools 2026](https://aka.ms/vs/stable/vs_BuildTools.exe).
5. Установить Ninja (через PowerShell):

```powershell
winget install Ninja-build.Ninja
```

6. Установить Ccache (через PowerShell):

```powershell
winget install ccache
```

7. Склонировать данный репозиторий с сабмодулями:

```powershell
git clone --recurse-submodules https://github.com/Leeleit/ProjectV
```

8. Запустить Clion, настроить под себя. В настройках Toolchains (Build, Execution, Deployment -> Toolchains) удалить все
   тулчейны, также в Cmake (Build, Execution, Deployment -> Cmake) удалить базовый Cmake профиль. Включить профиль
   windows-clang-debug.
9. Сконфигурировать Cmake, скомпилировать код, запустить. Должно вывести "Hello, World!" (на текущий момент).
10. ???
11. PROFIT!

#### Опционально:

1. Настроить clang-format и clang-tidy (снизу справа в нижней панели).
2. Настроить быстрый reformat (Настройки -> Keymap -> ищем Reformat Code, назначаем комбинацию для быстрого реформата).
3. Настроить красивые шрифт и тему (Я
   использую [Monocraft](https://github.com/IdreesInc/Monocraft/releases/download/v4.2.1/Monocraft-ttf.zip) и
   тему [Islands] Gerry Violet (в плагинах Clion устанавливается, называется Gerry Themes)).
4. Настроить мягкие переносы (Settings -> General -> Editor -> Soft Wraps и вписать туда *.cpp; *.c; *.hpp; *.h).

### **Данный гайд был успешно проверен на компе Пети, поэтому у остальных всё тоже должно работать.**

---

### Вопросы? Пишите мне, так называемому тимлиду...
