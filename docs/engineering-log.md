# Engineering Log

This log records important decisions, evidence, problems, and lessons from development.

## 2026-08-24 — Establish a professional project foundation

### Objective

Document the existing simulation and prepare the repository for incremental, testable development.

### Baseline

The application renders three circular bodies with different radii, initial velocities, and restitution values. Gravity accelerates them downward, the window boundaries constrain them, and pressing `R` resets the scene.

### Decisions

- Continue using C++17 and SFML 3 rather than a prebuilt game-physics system.
- Use SFML for windowing and rendering while implementing the physics ourselves.
- Keep generated `build/` output outside version control.
- Enable common compiler warnings to catch questionable code early.
- Remove duplicated includes and name important simulation constants before adding new physics behaviour.

### Verification

- The initial repository state compiled successfully with CMake.
- The initial scene was visually confirmed to fall, bounce, and reset.

### Next step

Implement a fixed-timestep simulation so physics behaviour does not depend on rendering frame rate.
