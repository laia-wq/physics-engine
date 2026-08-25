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

## 2026-08-24 — Add a fixed physics timestep

### Objective

Make the numerical simulation independent of rendering speed.

### Problem

The original loop used the duration of each rendered frame as the physics step. That duration varies with frame rate and system load, which can change numerical integration and collision behaviour between runs.

### Decision

- Advance physics in constant `1/120` second steps.
- Accumulate real frame time and run as many fixed physics steps as needed.
- Cap an unusually long frame at `0.25` seconds to avoid an unbounded simulation backlog after the application is paused.
- Continue rendering once per outer loop, independently of the number of physics steps performed.

### Verification

- The project configures and compiles with all warning options enabled.
- `git diff --check` reports no whitespace errors.

### Next step

Visually verify motion and reset behaviour, then begin circle-circle collision detection with automated tests for its geometric edge cases.
