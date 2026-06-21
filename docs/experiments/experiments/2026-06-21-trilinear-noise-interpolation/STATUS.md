# STATUS — 2026-06-21-trilinear-noise-interpolation

**Status:** `concluded-verdict-mixed`
**Closed:** 2026-06-21
**Last action:** 2026-06-21 — concluded. verdict=mixed. See README.md §5-6.

**Phase:** 5/5 (complete)
- [x] Phase 0: Topic selection & anti-duplicate sentinel
- [x] Phase 1: Reservation (backlog + INDEX + README + STATUS)
- [x] Phase 2: Web research (12 sources verified: Minecraft trilerp, KdotJPG critique, modern GPU noise approaches)
- [x] Phase 3: Prototype (C++26 CPU harness, 390 LoC, build green)
- [x] Phase 4: Measurement (125 configs × 100 iter = 12,500 measurements)
- [x] Phase 5: Conclusion (verdict=mixed — 3×3×3 recommended, 2×2×2 rejected)

**Headline findings:**
- B_Trilerp_2 (2×2×2, 64× reduction): PSNR 4.97 dB mean — FAILS hypothesis (<1 dB)
- C_Trilerp_3 (3×3×3, 19× reduction): PSNR 30.22 dB, 12.6× speedup — RECOMMENDED
- D_Trilerp_4 (4×4×4, 8× reduction): PSNR 36.23 dB, 6.7× speedup — quality mode
- E_Spline_2 (Catmull-Rom 2×2×2): PSNR -20.76 dB — REJECTED (undersampled)

**Blocker:** нет (concluded).
