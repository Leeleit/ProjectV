# 2026-06-21-helicopter-rotor-physics — Helicopter rotor physics simulation

**Status:** `concluded-verdict-yes`
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** independent (military sandbox — Tier 1 Physics)
**Estimated effort:** S
**Author:** self (агент)

---

## 1. Hypothesis

Гипотеза: Детальная физическая модель несущего винта вертолета (6-DOF жесткое тело + расчет сил на лопастях по теории лопастей с учетом взмаха blade flapping + циклического/общего шага + косого обдува translational lift + авторотации + Vortex Ring State) может быть эффективно симулирована на CPU при затратах процессорного времени <0.1 мс на вертолет за такт.
При этом упрощенная модель на основе импульсной теории (Momentum Theory) для LOD2+ позволяет снизить вычислительные затраты ниже 0.02 мс при сохранении адекватности макро-динамики полета (тяги, моментов рыскания и кабрирования).

---

## 2. Prior Art

Используются следующие материалы:
- Seddon & Newman "Basic Helicopter Aerodynamics" для формул индуктивной скорости и переходных режимов.
- Prouty "Helicopter Performance, Stability, and Control" для уравнений махового движения лопастей (blade flapping) и циклического управления.
- Эмпирические модели Vortex Ring State (режим вихревого кольца / "вихревой столб"), описывающие падение тяги при быстром вертикальном снижении в собственном потоке.

---

## 3. Method

- **Тип эксперимента:** prototype + benchmark.
- **Сцены/сценарии (5 сценариев):**
  1. `hover_stability`: зависание на месте, удержание высоты и углов с помощью простейшего автопилота.
  2. `forward_flight`: разгон до 50 м/с, влияние косого обдува (translational lift - увеличение тяги при росте горизонтальной скорости).
  3. `vortex_ring_state`: быстрое вертикальное снижение (вертикальная скорость > 10 м/с), попадание в вихревое кольцо, резкое падение тяги и потеря управления.
  4. `autorotation`: отключение двигателя на высоте 1000 м, планирование на авторотации (раскрутка винта набегающим снизу потоком воздуха).
  5. `cyclic_maneuver`: резкий маневр по тангажу/крену с помощью циклического шага для оценки гироскопической прецессии и махового движения лопастей.
- **Стратегии сравнения (5 стратегий):**
  - `A_MomentumTheory_LOD`: Импульсная теория диска (LOD2+, диск винта как единое целое, мгновенный баланс импульсов).
  - `B_BladeElement_2Blades`: Модель теории лопастей с симуляцией 2-х лопастей (вычисление сил на каждой лопасти отдельно).
  - `C_BladeElement_4Blades`: Модель теории лопастей с симуляцией 4-х лопастей.
  - `D_BladeElement_4Blades_Flapping`: Полная модель с 4-мя лопастями и симуляцией махового движения (blade flapping) для учета асимметрии тяги при поступательном движении.
  - `E_Vectorized_Helicopters`: Групповой векторизованный расчет (симуляция 10 вертолетов в пакете для проверки SIMD/SoA эффективности).
- **Метрики:**
  - Время симуляции шага (нс).
  - Устойчивость физического такта при 20 Гц, 60 Гц и 100 Гц.
  - Точность тяги и угловых ускорений по отношению к эталонной модели D на 200 Гц.

---

## 4. Prototype

Код прототипа находится в папке `prototype/`.
Сборка и запуск осуществляются стандартным образом через CMake в изолированной папке `prototype/build/`.

```bash
cd prototype
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
./helicopter_bench
```

---

## 5. Results

- **Strategy D (4-Blade BET + Flapping)** achieves a step time of **1.34 µs** (0.0013 ms) at 60 Hz on the dev host CPU, which is **75x faster** than the target budget of 0.1 ms.
- **Strategy A (Momentum Theory LOD)** is extremely lightweight, costing **80.4 ns** (0.08 µs), which is **250x faster** than the 0.02 ms LOD target.
- **Stability and Tick Rate**: Explicit RK4 integration of flapping equations requires a tick rate of $\ge 50$ Hz. At 20 Hz, the flapping dynamics (which oscillate at the nominal rotor frequency of ~5 Hz) suffer from severe numerical instability, leading to coning explosions. At 60 Hz, stability is **96.0%**.
- **Autopilot Lag**: Aerodynamic lag (gyroscopic precession delay of blade flapping) causes roll/pitch response delays, requiring gentler feedback gains in the flight controller to prevent pilot-induced oscillations.
- Detailed results are available in [RESULTS.md](./RESULTS.md).

---

## 6. Verdict

**Verdict:** `yes` (with a tick rate requirement of $\ge 60$ Hz for high-fidelity simulation, and tuned feedback gains).
The performance and accuracy match all design requirements. 

---

## 7. Integration Recommendation

1. **Architecture**: Implement the helicopter flight model in `src/physics/helicopter_vehicle.{hpp,cpp}` using standard C++26.
2. **LOD System**:
   - **LOD0/1**: Use **Strategy D** (4-Blade BET with flapping, integrated via RK4) at 60 Hz or higher.
   - **LOD2+**: Use **Strategy A** (Momentum Theory LOD, integrated via Euler) at 20 Hz or 60 Hz.
3. **Autopilot**: Decouple the feedback loop gains depending on the LOD tier:
   - High-fidelity LODs need low attitude stabilization gains (e.g. $P \le 0.15$) to account for rotor response lag.
   - Low-fidelity LODs can use higher gains (e.g. $P \ge 0.4$) for tighter control.
