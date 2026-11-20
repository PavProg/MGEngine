# MGEngine

Игровой движок на C++17 и OpenGL 3.3: ECS, загрузка моделей (.obj), текстур
(PNG/JPG) и шейдеров, менеджер ресурсов с кэшированием, асинхронной загрузкой
и горячей заменой, профайлер Tracy, job-система на enkiTS.

Платформа: Windows (MSVC + vcpkg).

---

## Что нужно установить

1. **Git** — https://git-scm.com
2. **Visual Studio 2022 или новее** (проверено на Visual Studio 2026)
   с компонентом **«Разработка классических приложений на C++»**.
   В нём уже есть компилятор MSVC, CMake и Ninja — отдельно ставить не нужно.
3. **vcpkg** — менеджер C++-зависимостей (установка ниже).
4. Для просмотра профиля — **Tracy 0.14.1** (только при необходимости, см. ниже).

> Команды из консоли выполняются в **«Developer PowerShell for VS»**
> (Пуск → начните печатать «Developer PowerShell»). В обычном PowerShell
> команды `cmake`, `ninja`, `cl` могут быть не найдены.

---

## Зависимости

Ставятся **автоматически** при первой настройке проекта — вручную ничего качать не нужно.

| Библиотека | Зачем | Откуда |
|---|---|---|
| glfw3 | окно и ввод | vcpkg (`vcpkg.json`) |
| glm | математика | vcpkg |
| enkiTS | job-система (пул потоков и задачи) | vcpkg |
| tracy | профайлер | своя копия порта в папке `vcpkg-overlay/tracy` |
| glad, nlohmann/json, stb_image, tinyobjloader | OpenGL, JSON, картинки, модели | лежат в репозитории, папка `dependencies/include` |

Версии всех библиотек из vcpkg зафиксированы в `vcpkg-configuration.json` (поле `baseline`),
поэтому на любом компьютере собирается одно и то же.

---

## Установка vcpkg (один раз)

Путь `C:\vcpkg` важен — он прописан в `CMakePresets.json`.

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

Если vcpkg уже стоит в другом месте — см. раздел «Если vcpkg не в C:\vcpkg».

---

## Сборка и запуск

### Через Visual Studio
1. «Файл → Открыть → Папка» и выбрать папку проекта.
2. Выбрать конфигурацию `windows-x64-debug` и дождаться окончания настройки CMake.
3. «Сборка → Собрать всё».

### Из консоли

```powershell
git clone <URL-репозитория>
cd MGEngine

cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

cd build\windows-x64-debug
.\MyGameEngine.exe
```

Пресеты:

| Пресет | Для чего |
|---|---|
| `windows-x64-debug` | повседневная разработка, работают проверки `assert` |
| `windows-x64-release` | быстрая сборка без отладочной информации |
| `windows-x64-profile` | для замеров в Tracy (оптимизация + отладочная информация) |

Первая настройка идёт дольше: vcpkg скачивает и собирает все библиотеки.
После сборки рядом с `.exe` автоматически копируются папка `assets` и нужные DLL
(`glfw3.dll`, `enkiTS.dll` и др.). Лог пишется в `game.log` в той же папке.

---

## Профайлер Tracy

1. Скачать Tracy **0.14.1** (версия должна совпадать с версией в проекте):
   https://github.com/wolfpld/tracy/releases
2. Запустить игру и `tracy-profiler.exe`, нажать «Connect».

В Debug-сборке Tracy записывает данные только пока подключён профайлер (режим on-demand).

---

## Режим замера

Для замеров производительности движок умеет сам грузить тестовую пачку файлов и закрываться.

1. Один раз сгенерировать тяжёлые файлы (~480 МБ, в git не попадают):
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\gen_bench_assets.ps1
   ```
2. Собрать пресет `windows-x64-profile` («Перестроить всё», чтобы файлы скопировались в папку сборки).
3. Запустить из папки сборки:
   ```powershell
   .\MyGameEngine.exe --bench=load-async --novsync
   ```

| Флаг | Что делает |
|---|---|
| `--bench=steady` | 15 с работы без загрузок |
| `--bench=load-sync` | загрузка пачки в главном потоке |
| `--bench=load-async` | асинхронная загрузка пачки |
| `--bench=load-exit` | выход через 0,1 с после начала загрузки |
| `--novsync` | не ждать монитор (иначе кадр всегда ~16,7 мс) |

Если `--novsync` не помогает, проверьте в драйвере видеокарты, что вертикальная
синхронизация не включена принудительно (AMD: «Ждать вертикального обновления» →
«Выкл., если не указано приложением»).

---

## Возможные проблемы

| Проблема | Решение |
|---|---|
| `cmake` / `ninja` / `cl` не распознаётся | Запускайте из «Developer PowerShell for VS». |
| `Could not read CMAKE_TOOLCHAIN_FILE … C:/vcpkg/...` | vcpkg не в `C:\vcpkg`. См. раздел ниже. |
| `vcpkg install failed`, а в логе сборки пакета `error D8050` или `D8018` | В имени пользователя Windows есть русские буквы, и компилятор не может создать временные файлы. Создайте папку `C:\Temp` и в «Изменение переменных среды текущего пользователя» задайте `TEMP` и `TMP` = `C:\Temp`. Перезапустите Visual Studio. |
| `Port is missing portfile: …vcpkg-overlay/tracy/portfile.cmake` | Файл не попал в git. В `.gitignore` должна быть строка `!vcpkg-overlay/**/*.cmake`, а сам файл — закоммичен. |
| `find_package(...) not found` | Проверьте, что в корне есть `vcpkg.json` и сборка идёт через пресет (vcpkg-toolchain). Имя в CMake чувствительно к регистру: `enkiTS`, а не `enkits`. |
| При запуске: `glfw3.dll` или `enkiTS.dll` not found | Скопируйте DLL из `build\windows-x64-debug\vcpkg_installed\x64-windows\bin` рядом с `.exe`. |
| При запуске: `LoadLibrary failed with error 87` | Ошибка видеодрайвера при создании OpenGL-окна. Обновите драйвер видеокарты (у AMD — с «чистой установкой») и перезагрузитесь. |
| Не находятся `assets` / текстуры | Запускайте `.exe` из его папки `build\windows-x64-debug\`. |

---

## Если vcpkg не в C:\vcpkg

Не редактируйте `CMakePresets.json` (он в репозитории). Создайте рядом
локальный `CMakeUserPresets.json` (он в `.gitignore`) и укажите свой путь:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "my-debug",
      "inherits": "windows-x64-debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "D:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ]
}
```

И собирайте своим пресетом: `cmake --preset my-debug`.

---

## Рекомендации

- **Чистая пересборка.** Если после изменения структур или заголовков появляются
  странные ошибки — в Visual Studio «Проект → Удалить кэш и выполнить повторную
  настройку», или удалите папку `build\` и настройте заново.
- **Новая библиотека.** Допишите её в `dependencies` в `vcpkg.json`, затем
  `find_package` и `target_link_libraries` в `CMakeLists.txt`. Точные имена для CMake
  vcpkg показывает в конце установки (раздел «provides CMake targets»).
- **tools/gen_bench_assets.** Основная задача файлв - нагрузить движок. Скрипт создает пачку больших
  тестовых ассетов, на загрузке которых можно что-то измерить.