# ProjectV – Воксельный движок

## Гайд по установке и настройке окружения

1. Установить [Clion](https://www.jetbrains.com/clion/download/download-thanks.html?platform=windows) (нужен VPN) (это
   платная программа. Чтобы активировать, следуйте инструкции на [сайте](https://306.antroot.ru/jetbrains-activation)) (
   вы можете использовать любой другой редактор/IDE, но Clion, я считаю, – лучший вариант)
2. Установить [LLVM](https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.0/LLVM-22.1.0-win64.exe)
3. Установить [Microsoft Visual Studio Build Tools 2026](https://aka.ms/vs/stable/vs_BuildTools.exe)
4. Установить Ninja (через PowerShell):

```powershell
winget install Ninja-build.Ninja
```

5. Установить Ccache (через PowerShell):

```powershell
winget install ccache
```

6. Склонировать данный репозиторий с сабмодулями:

```powershell
git clone --recurse-submodules https://github.com/Leeleit/ProjectV
```

7. Запустить Clion, настроить под себя. В настройках Toolchains (Build, Execution, Deployment -> Toolchains) удалить все
   тулчейны, также в Cmake (Build, Execution, Deployment -> Cmake) удалить базовый Cmake профиль. Включить профиль
   windows-clang-debug.
8. Сконфигурировать Cmake, скомпилировать код, запустить. Должно вывести "Hello, World!" (на текущий момент).
9. ???
10. PROFIT!

### **Данный гайд был успешно проверен на компе Пети, поэтому у остальных всё тоже должно работать**

---

### Вопросы? Пишите мне, так называемому тимлиду.
