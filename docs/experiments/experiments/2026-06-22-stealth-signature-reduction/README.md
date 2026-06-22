# 2026-06-22-stealth-signature-reduction — Stealth Signature Reduction and Sensor Detection Range Simulation

**Status:** `concluded-verdict-yes`
**Date opened:** 2026-06-22
**Date closed:** 2026-06-22
**Stage link:** independent (military sandbox sensor logic)
**Estimated effort:** S
**Author:** Antigravity (research agent)

---

## 1. Hypothesis

1. **Aspect-Angle RCS Lookup Performance:**
   Simulating aspect-angle-dependent Radar Cross Section (RCS) via a compact polar lookup table (5° resolution azimuth and elevation grid) plus engine thermal IR and acoustic propagation math can run in `<0.5\ \mu\text{s}` per vehicle per tick on the CPU.

2. **Detection Range Signature Scaling:**
   Implementing active signature reduction strategies (Radar Absorbent Material coatings, exhaust IR suppression, engine vibration dampers) dynamically scales detection ranges according to physical equations ($R \propto \sigma^{1/4}$ for radar, $R \propto I^{1/2}$ for IR, and logarithmic spreading $TL = 20 \log_{10} R$ for acoustics).

3. **Clutter Masking Interaction:**
   Combining signature reduction with environmental parameters (rain attenuation, ground/sea clutter, electronic jamming noise) realistically hides low-signature vehicles inside background sensor noise floors.

---

## 2. Prior art

The physical modeling of vehicle stealth signature characteristics is guided by electromagnetic and acoustic sensor equations:
- **Skolnik (1980), "Introduction to Radar Systems":** Explains the physical basis of the radar range equation and aspect-angle-dependent target cross-section variations.
- **Hudson (1969), "Infrared System Engineering":** Focuses on thermal contrast irradiance, NEI sensors, and atmospheric absorption coefficients.
- **Urick (1983), "Principles of Underwater Sound":** Forms the basis for passive acoustic propagation loss modeling.
- **DCS World & War Thunder forums:** Document implementation of polar coordinate aspect RCS matrices and engine throttle thermal scaling.

---

## 3. Method

- **Type of Experiment:** Prototype + Benchmark (C++26 standalone console application).
- **Physical Model:**
  - 3D coordinate mapping translating relative sensor-to-target vectors into target-local azimuth/elevation.
  - Spherical aspect table lookup (36 elevation steps × 72 azimuth steps, 5° resolution).
  - Target engine throttle and velocity-dependent source levels.
  - Environment ambient noise, clutter levels, and atmospheric attenuation parameters.
- **Strategies (Signature Reduction):**
  - **A_BaseSignature:** Baseline unshielded target (RCS $10.0\text{ m}^2$, IR $500.0\text{ W/sr}$, Acoustic $120.0\text{ dB}$).
  - **B_RcsCoating:** RAM coating (-15 dB RCS).
  - **C_IrSuppression:** Exhaust cooling (-10 dB IR).
  - **D_AcousticQuieting:** Dampers and mufflers (-12 dB acoustic).
  - **E_FullLowObservable:** LO target (-15 dB RCS, -10 dB IR, -12 dB acoustic).
- **Environments:**
  - **1_ClearSkyDay:** Low noise ($NL = 45$ dB), minimal clutter.
  - **2_RainStorm:** High rain noise ($NL = 60$ dB), high radar/IR attenuation.
  - **3_SeaClutter:** Elevated sea surface radar clutter.
  - **4_LowAltitudeGroundClutter:** High terrain echoes and ground clutter.
  - **5_ActiveJammedEnvironment:** Barrage jamming raising the radar noise floor by 100×.
- **Metrics Collected:**
  - Mean radar, IR, and acoustic detection ranges (meters).
  - Detection success rate (target spotted before entering 1500m danger zone).
  - CPU calculation time per vehicle step (ns).

---

## 4. Prototype

The prototype consists of a standalone C++26 program located in `prototype/`.
It generates a full sweep across all combinations of strategies, environments, and random seeds (representing sensor noise).

### Build and Run Instructions

```bash
mkdir -p prototype/build
cd prototype/build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./stealth_bench
```

The benchmark outputs raw measurements to `results.csv`.

---

## 5. Results

Detailed stats are stored in [RESULTS.md](./RESULTS.md). Key findings include:
- **Radar Clutter Limits:** Low-RCS targets are successfully masked by sea and ground clutter at reference ranges. Effective tracking ranges clamp to clutter boundaries (5,000 meters).
- **Acoustic Spreading:** Muffling the acoustic source by -12 dB cuts passive detection range from 13.4 km down to **3,385 meters** (exactly fitting logarithmic spreading theory).
- **Weather Extinction:** Precipitation increases atmospheric IR attenuation, dropping baseline IRST tracking ranges from 150 km to **23.6 km**, which is further reduced to **20.1 km** using IR suppression.
- **CPU Benchmarks:** Calculating aspect-dependent coordinates and executing sensor range physics took only **320 to 500 nanoseconds** per tick. This permits dynamic updates for 1,000+ entities under **~0.35 milliseconds**, easily fitting within wargame tick budgets.

---

## 6. Verdict

`yes`

Dynamic aspect-dependent stealth signature modeling is highly performant (~350 ns per update) and matches physical detection range equations across radar, thermal, and acoustic sensors. Caching angle-dependent values in small L1-friendly aspect maps is the recommended default.

---

## 7. Integration recommendation

- **Target stage:** Stage 6+ (military sandbox / sensor mechanics).
- **Core modules affected:**
  - Create `src/sensor/SensorSignature.{hpp,cpp}` containing aspect grid caching structures.
  - Add `SensorSignatureComponent` to Flecs ECS tracking real-time signature coefficients, throttle states, and mask values.
  - Wire sensors into `SensorUpdateSystem` evaluating detection loops against the active environment parameters.
- **Acceptance criteria:**
  - Target detection ranges conform to physical logarithmic (acoustics), exponential (IR), and fourth-root (radar) equations within $\pm 1\%$ accuracy.
  - Tracy profile update cost is $<1.0\ \mu\text{s}$ per entity update.
- **Estimated effort:** S-M effort (1-2 sessions, $\approx 450$ lines of C++ code).

---

## 8. Sources

See [sources.md](./sources.md).

---

## 9. Mapping to ProjectV hot-path

- **Voxel Clutter:** Real-time clutter values can be derived directly from voxel material buffers (e.g. sea water vs. ground rocks).
- **Weather Attenuation:** Coupled with dynamic weather cellular-automata loops.
- **Hardware baseline:** Zen 3 5800X (refer to `hardware-profile.md` §1-2).
