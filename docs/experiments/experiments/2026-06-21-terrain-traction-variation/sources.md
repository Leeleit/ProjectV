# Sources — Terrain Traction Variation

This document list the academic papers, developer diaries, tutorials, and references used to construct the terrain traction and tire slip model.

## 1. Primary Academic and Engineering Sources

- **Pacejka, H. B.** (2005). *Tire and Vehicle Dynamics*. Elsevier.
  - *Annotation:* The canonical reference for the Pacejka Magic Formula, which calculates longitudinal force ($F_x$), lateral force ($F_y$), and self-aligning torque ($M_z$) as non-linear functions of slip ratio ($\kappa$) and slip angle ($\alpha$). The benchmark simplifies this to the longitudinal force equation:
    $$F_x = D \sin(C \arctan(B \kappa - E(B \kappa - \arctan(B \kappa))))$$
    where $B$, $C$, $D$, and $E$ are stiffness, shape, peak, and curvature coefficients.

- **Milliken, W. F., & Milliken, D. L.** (1995). *Race Car Vehicle Dynamics*. SAE International.
  - *Annotation:* Chapter 14 covers tire behavior and slip ratio calculation. Longitudinal slip ratio ($\kappa$) is defined as:
    $$\kappa = \frac{\omega r - v_x}{v_x}$$
    during acceleration, and:
    $$\kappa = \frac{\omega r - v_x}{\omega r}$$
    during braking (where $\omega$ is wheel angular velocity, $r$ is wheel radius, and $v_x$ is vehicle longitudinal velocity).

- **Beckman, Brian** (1991–2007). *The Physics of Racing*. Series of Papers.
  - *Annotation:* Part 21 ("A Tire Model") provides a highly readable guide for game developers on adapting the Pacejka Magic Formula for real-time physics engines, including traction limits and stability constraints at low speeds.

## 2. Game Industry Implementations and Tutorials

- **Monster, Marco** (2003). *Car Physics for Games*.
  - *Annotation:* The seminal tutorial for implementing tire slip and Pacejka calculations in game loops. It details the relationship between engine torque, wheel inertia, slip ratio, and friction forces.

- **Zagrebelnyy, Pavel** (2015). *Spintires GDC Talk: Dynamic Mud and Terrain Deformation*.
  - *Annotation:* Explains how Spintires calculates traction coefficients based on terrain heightmap deformation and wheel sinkage. Surface traction varies from 1.0 (dry asphalt) to 0.15 (deep mud/swamp), with torque reduction proportional to wheel spin/slip ratio.

- **Gaijin Entertainment / War Thunder Dev Diaries** (2018–2025). *Ground Vehicle Traction and Soil Pressure Updates*.
  - *Annotation:* Explains how traction coefficients depend on vehicle weight distribution, tread/tire width (ground pressure), and surface type. Highlights the difference in steering responses for tracked vs. wheeled vehicles on mud/snow/sand.
