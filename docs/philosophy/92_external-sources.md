# Канонические внешние источники

Единый список ссылок на первоисточники, на которых построена философия
каталога.

> Дата отсечки: 2026-06-27. Работы старше 2014 упоминаются только если
> канонические и не были вытеснены более свежими.

---

## Data-Oriented Design

| Источник | Год | Зачем |
|:---------|:----|:------|
| Mike Acton, *Data-Oriented Design and C++*, CppCon 2014 keynote | 2014 | Канонические 12 принципов DOD. |
| Lucian Radu Teodorescu, *Revisiting Data-Oriented Design*, ACCU Overload 30/167 | 2022 | Структурная переформулировка 12 принципов. |
| Vittorio Romeo, *More Speed & Simplicity: Practical DOD in C++*, CppCon 2025 keynote | 2025 | Свежее переосмысление. |

URL:

- Acton 2014: <https://www.youtube.com/watch?v=rX0ItVEVjHc>
- Romeo 2025: <https://www.youtube.com/watch?v=SzjJfKHygaQ>
- Teodorescu 2022: <https://accu.org/journals/overload/30/167/teodorescu/>

---

## ECS (Entity Component System)

| Источник | Год | Зачем |
|:---------|:----|:------|
| Sander Mertens, *Building an ECS: Data Oriented Hierarchies* | 2026 | Иерархии через flecs. |
| *The Essence of Entity Component System* (arXiv:2606.14919) | 2026 | Формализация archetype ECS. |
| flecs DeepWiki — *Multithreading and Staging* | 2026 | Подробно о staging system flecs v4. |

URL:

- Mertens: <https://www.flecs.dev/flecs/>
- DeepWiki: <https://deepwiki.com/SanderMertens/flecs/5.3-multithreading-and-staging>

---

## C++26 и эволюция языка

| Источник | Год | Зачем |
|:---------|:----|:------|
| Herb Sutter, *C++26 is done! Trip report March 2026 ISO C++* | 2026-03 | Финальный отчёт о завершении C++26. |
| cppreference.com, *C++26* | 2026 | Таблица всех фич, paper numbers, статус поддержки. |
| P2996R13, *Reflection for C++26* | 2025 | Основной proposal по reflection. |
| P2900R14, *Contracts for C++* | 2025-02 | Основной proposal по contracts. |
| P4043R0, *Are C++ Contracts Ready to Ship in C++26?* | 2026 | Дискуссионный paper. |
| P2300R10, `std::execution` (Sender/Receiver) | 2025 | Стандартизированная асинхронность. |
| NVIDIA `stdexec` | 2026 | Reference implementation P2300. |

URL:

- Sutter March 2026:
  <https://herbsutter.com/2026/03/29/c26-is-done-trip-report-march-2026-iso-c-standards-meeting-london-croydon-uk/>
- cppreference C++26: <https://cppreference.com/cpp/26>
- P2996R13:
  <https://www.open-std.org/jtc1/SC22/wg21/docs/papers/2025/p2996r13.html>
- P2900R14 issue: <https://github.com/cplusplus/papers/issues/1648>
- P4043R0:
  <https://www.open-std.org/jtc1/SC22/wg21/docs/papers/2026/p4043r0.html>
- NVIDIA stdexec: <https://github.com/NVIDIA/stdexec>

---

## Vulkan и GPU

| Источник | Год | Зачем |
|:---------|:----|:------|
| Vulkan 1.4 Specification (1.4.308) | 2026-06 | Каноническая спека. |
| Sascha Willems, *How to Vulkan in 2026* | 2026 | Лучшие практики и примеры. |
| Vulkanised 2026 papers | 2026 | Kerem Tuncer (FrameGraph), Preetish Kakkar (mobile Vulkan). |
| NVIDIA, *Improve Shader Performance with SER* | 2022 | Shader Execution Reordering. |
| NVIDIA, *Best Practices for Using NVIDIA RTX Ray Tracing* | 2022 | RT best practices. |
| AMD, *uProf documentation* | 2026 | Аппаратные счётчики. |
| Intel, *VTune Profiler 2026.1* | 2026 | Release notes. |
| D3D12 Developer Blog, *Shader Execution Reordering* | 2026-02 | SER в DXR. |

URL:

- Vulkan 1.4 спека:
  <https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html>
- Vulkan 1.4 version appendix:
  <https://docs.vulkan.org/spec/latest/appendices/versions.html>
- VK_EXT_descriptor_indexing:
  <https://vulkan.lunarg.com/doc/view/1.4.341.1/windows/antora/refpages/latest/refpages/source/VK_EXT_descriptor_indexing.html>
- VK_EXT_mesh_shader:
  <https://vulkan.lunarg.com/doc/view/1.4.341.1/windows/antora/refpages/latest/refpages/source/VK_EXT_mesh_shader.html>
- NVIDIA SER:
  <https://developer.nvidia.com/blog/improve-shader-performance-and-in-game-frame-rates-with-shader-execution-reordering/>
- NVIDIA RT best practices:
  <https://developer.nvidia.com/blog/best-practices-for-using-nvidia-rtx-ray-tracing-updated/>
- AMD uProf: <https://www.amd.com/en/developer/uprof.html>
- Intel VTune:
  <https://www.intel.com/content/www/us/en/developer/articles/release-notes/vtune-profiler/2026.html>
- Vulkanised 2026: <https://www.khronos.org/events/vulkanised-2026>

---

## Архитектура движков

| Источник | Год | Зачем |
|:---------|:----|:------|
| Jason Gregory, *Game Engine Architecture*, 4ed (двухтомник) | 2026-04 | Канонический справочник. ~1100 стр. |
| Game++ blog, PVS-Studio | 2026 | Серия «C++, game engines, and architectures». |
| AMD Game Engineering team, *How do I become a graphics programmer?* | 2023 | Путь в graphics programming. |
| Data-Oriented Design book (Richard Fabian) | 2025 reprint | Книга по DOD с нуля. |

URL:

- Gregory 4ed: <https://www.gameenginebook.com/>
- Gregory 4ed Routledge:
  <https://www.routledge.com/Game-Engine-Architecture-Two-Volume-Set/Gregory/p/book/9781041162599>
- Game++ blog: <https://pvs-studio.com/en/blog/posts/1361/>
- AMD guide: <https://gpuopen.com/learn/how_do_you_become_a_graphics_programmer/>

---

## Время, физика, детерминизм

| Источник | Год | Зачем |
|:---------|:----|:------|
| Glenn Fiedler, *Fix Your Timestep!* | 2004 | Каноническая статья о fixed timestep + accumulator + интерполяция. |

URL:

- Fiedler: <https://gafferongames.com/post/fix_your_timestep/>

---

## Аппаратное обеспечение (perf и архитектура)

| Источник | Год | Зачем |
|:---------|:----|:------|
| TheLinuxCode, *CPU vs GPU in 2026: A Practical, Numbers-First Guide* | 2026-01 | Конкретные числа для hardware-tour. |
| Acomquest, *CPU vs GPU: Understanding the Architecture Difference*, Medium | 2026-02 | Визуализация warp/wavefront. |
| Travis Downs, *Performance resources* blog | ongoing | Канонический блог по cache, prefetching, branch prediction на x86. |
| Daniel Lemire, blog | ongoing | SIMD, fast integer parsing. |

URL:

- TheLinuxCode 2026:
  <https://thelinuxcode.com/cpu-vs-gpu-in-2026-a-practical-numbers-first-guide-for-developers/>
- Medium Feb 2026:
  <https://medium.com/@indiai/cpu-vs-gpu-understanding-the-architecture-difference-26d865fc0665>
- Travis Downs: <https://travisdowns.github.io/>
- Lemire: <https://lemire.me/blog/>

---

## Профилирование и отладка

| Источник | Год | Зачем |
|:---------|:----|:------|
| Tracy Profiler manual | 2025-12 | Tracy 0.13.1 manual. |
| Randomascii, *Intel Architecture Code Analyzer* | ongoing | Блог Bruce Dawson (ex-Google, ex-Valve). |
| Chandler Carruth, *Garbage In, Garbage Out* (CppCon 2017) | 2017 | C++ perf optimisation talk. |

URL:

- Tracy releases: <https://github.com/wolfpld/tracy/releases>
- Randomascii: <https://randomascii.wordpress.com/>
- Carruth CppCon 2017: <https://www.youtube.com/watch?v=nXaxk27zwlk>

---

## Воксельные данные и sparse storage

| Источник | Год | Зачем |
|:---------|:----|:------|
| Samuli Laine, Tero Karras, *Efficient Sparse Voxel Octrees*, NVIDIA Research | 2010 | Канонический paper по SVO. |
| Ken Museth, *NanoVDB* | 2021 | Sparse VDB структура для GPU. |
| *Meshlet ray tracing pipelines* (DiVA Portal) | 2026 | Интеграция meshlet + RT в DirectX 12. |

URL:

- Laine & Karras 2010:
  <https://research.nvidia.com/publication/efficient-sparse-voxel-octrees>
- NanoVDB:
  <https://github.com/AcademySoftwareFoundation/openvdb/tree/master/nanovdb>
- Meshlet RT DiVA: <https://www.diva-portal.org/smash/get/diva2:2041049/FULLTEXT01.pdf>

---

## Memory allocators (2025-2026)

| Источник | Год | Зачем |
|:---------|:----|:------|
| Microsoft Research, *mimalloc: A new, high-performance, scalable memory allocator* | 2026-05 | Архитектура mimalloc. |
| braindetox, *mimalloc Deep Dive 2026* | 2026-05 | Сравнение mimalloc, jemalloc, tcmalloc, rpmalloc. |
| Unreal Engine Forums, *Difference in memory allocators* | 2026-02 | UE 5 интеграция mimalloc. |

URL:

- Microsoft Research mimalloc:
  <https://www.microsoft.com/en-us/research/blog/mimalloc-a-high-performance-scalable-memory-allocator-for-the-modern-era/>
- braindetox:
  <https://braindetox.kr/en/posts/mimalloc_performance_allocator_2026.html>

---

## Что не цитируется

- **Game Engine Black Book** (Wolfgang Engel, серия DOOM/Wolf3D). Исторический
  интерес, не современные практики.
- **Real-Time Rendering 4ed** (Akenine-Möller et al.). Хорошая книга, но не
  «наша философия» — это справочник по алгоритмам.
- **GPU Gems / GPU Pro.** Старые (2004-2014) сборники. Многое устарело.
- **Статьи до 2014** по C++ performance — `std::string`, `std::shared_ptr`,
  `std::map` стали существенно лучше в C++17/20/23.