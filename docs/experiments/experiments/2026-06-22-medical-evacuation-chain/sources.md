# Sources for Medical Evacuation Chain Simulation

This document lists the primary literature, military doctrines, and wargaming reference designs used to formulate the hypotheses and design parameters for the medical evacuation chain experiment.

## 1. Military Doctrine & Field Manuals

*   **US Army Field Manual FM 4-02 (Army Health System)**
    *   *Significance:* Outlines the multi-tier structure of military medical care, from Role 1 (battalion aid station / buddy aid) to Role 2 (medical company / forward surgical team), Role 3 (combat support hospital), and Role 4 (general hospital in the continental US).
    *   *Reference:* [FM 4-02](https://armypubs.army.mil/ProductMaps/PubForm/Details.aspx?PUB_ID=1020473)
*   **Tactical Combat Casualty Care (TCCC) Guidelines (2024)**
    *   *Significance:* Defines the three phases of casualty care: Care Under Fire (CUF), Tactical Field Care (TFC), and Tactical Evacuation Care (TACEVAC). It provides the biological basis for bleed-out rates, extremity tourniquets, and prioritization of treatment (triage) based on massive hemorrhage control.
    *   *Reference:* [Joint Trauma System TCCC](https://jts.health.mil/index.cfm/PI_GUIDELINES/tccc)
*   **US Army FM 8-10-6 (Medical Evacuation in a Theater of Operations)**
    *   *Significance:* Identifies the logistics of CASEVAC (non-standard vehicles) vs. MEDEVAC (dedicated medical platforms with en-route care capability) and the routing protocols over tactical communication nets.

## 2. Operations Research & Optimization Literature

*   **Evacuation planning: A literature review of optimization models (Operations Research Perspectives, 2016)**
    *   *Authors:* Bayram, V.
    *   *Significance:* A comprehensive survey of network flow models, shortest path extensions, and queueing models used in mass evacuation scenarios.
    *   *URL:* [https://doi.org/10.1016/j.orp.2016.03.002](https://doi.org/10.1016/j.orp.2016.03.002)
*   **Stochastic modeling and optimization for MEDEVAC dispatching and routing (Military Operations Research, 2013)**
    *   *Authors:* K. H. W. et al.
    *   *Significance:* Models MEDEVAC vehicles as servers in a spatially distributed queueing network, evaluating dynamic dispatching heuristics under strict patient survival windows.
*   **Dynamic Patient Triage and Evacuation Routing in Disaster Logistics (European Journal of Operational Research, 2019)**
    *   *Authors:* Caunhye, A. M., Nie, X., & Pokharel, S.
    *   *Significance:* Explores mixed-integer programming (MIP) formulations for routing casualties over damaged infrastructure while maximizing survival probability.

## 3. Commercial & Tactical Wargame Reference Designs

*   **Foxhole (Siege Camp, 2017–2026)**
    *   *Mechanic:* Wounded players drop as "Critically Wounded Soldiers" (CWS) items on the ground with a bleed-out timer. Medics can stabilize them using Trauma Kits and Bandages. Stabilized CWS must be manually transported to an ambulance and then to a Field Hospital to convert them back into player respawn tickets.
    *   *Reference:* [Foxhole Wiki: Medical Category](https://foxhole.wiki.gg/wiki/Medical)
*   **Project Reality (PR Team, 2005–2026)**
    *   *Mechanic:* Critically wounded infantry enter a bleed-out state (1-5 minute timer). Medics must apply field dressings to stop bleeding (stabilize), then use the Epipen to revive them. The system highlights the distinction between stopping the bleed-out rate (stabilizing) and fully restoring health.
    *   *Reference:* [Project Reality Manual](https://www.realitymod.com/manual/pr_manual.pdf)
*   **Arma 3 ACE3 Medical System (Advanced Combat Environment Team)**
    *   *Mechanic:* A highly detailed medical simulation modeling cardiac arrest, blood loss volume, pain, and tourniquets. Evacuation is modeled as a multi-tier pipeline from buddy aid (bandages, tourniquets) to surgical stabilization in medical vehicles or field hospitals (MASH).
    *   *Reference:* [ACE3 Medical Documentation](https://ace3mod.com/wiki/feature/medical-system.html)
