# Sources — 2026-06-21-fixed-wing-flight-model-simulation

## Primary sources

1. **Stevens, B. L., Lewis, F. L., & Johnson, E. N. (2015). Aircraft Control and Simulation: Dynamics, Controls Design, and Autonomous Systems (3rd Edition).** Wiley.
   - *Description:* The definitive textbook on flight dynamics, coordinate systems, rigid-body equations of motion, and aerodynamic coefficient representations.
   - *Relevance:* Used as the baseline for 6-DOF equations of motion (Euler/RK4) and quaternions to prevent gimbal lock.

2. **McCormick, B. W. (1995). Aerodynamics, Aeronautics, and Flight Mechanics (2nd Edition).** Wiley.
   - *Description:* Clear explanation of lifting surface aerodynamics, parasitic drag, induced drag, and simple wing theories.
   - *Relevance:* Provides formulas for lift-curve slope $C_L(\alpha)$ and induced drag coefficient.

3. **Blade Element Theory (BET) for Flight Simulation**
   - *Description:* X-Plane flight simulator (Austin Meyer) uses Blade Element Theory to dynamically evaluate lift and drag across wings, rather than precomputed stability derivatives.
   - *Relevance:* Baseline model for dividing the wings and tail into strips (4 segments) and evaluating local angle of attack ($\alpha_i$).

4. **Compressibility Correction (Prandtl-Glauert & Wave Drag Rise)**
   - *Description:* Aerodynamic textbooks detailing wave drag rise at transonic/supersonic regimes ($M > 0.7$).
   - *Relevance:* Simple wave drag expansion term $C_{D,wave} = C_{D,wave,max} \cdot \max(0, M - M_{crit})^2$ to model drag penalties near Mach 1.0.

## Secondary / Online references

5. **Gaffer on Games (Glenn Fiedler, 2004) - "Integration Basics"**
   - *Description:* Comprehensive review of numerical integration methods for games (Euler, Verlet, RK4).
   - *Relevance:* Stability comparison of Euler vs RK4 at lower tick rates (20 Hz and 60 Hz).
