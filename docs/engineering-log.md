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

## 2026-08-24 — Detect circle-circle collisions

### Objective

Detect when two circular bodies touch or overlap before implementing collision response.

### Method

For two circle centres, subtract their positions to obtain the displacement vector. A collision exists when the squared displacement length is no greater than the square of the combined radii:

```text
dx² + dy² <= (radiusA + radiusB)²
```

Comparing squared values avoids an unnecessary square-root calculation.

### Design decisions

- Put reusable collision logic in a small `physics-core` library rather than tying it to rendering code.
- Count exactly touching circles as colliding.
- Test geometry separately from the interactive application.
- Highlight detected circles in red, but deliberately leave their velocities unchanged until the collision-response milestone.

### Edge cases tested

- Clearly separated circles
- Exactly touching circles
- Overlapping circles
- Circles sharing the same centre
- Diagonal separation

### Next step

Calculate a collision normal, correct penetration, and apply an impulse so colliding circles bounce according to mass and restitution.

## 2026-08-27 — Resolve circle-circle collisions

### Objective

Make colliding circles separate and bounce instead of passing through each other.

### Physics model

The engine calculates a collision normal from one centre to the other. It projects relative velocity onto that normal, then applies an equal-and-opposite impulse scaled by inverse mass and restitution.

Larger circles use greater mass because mass is proportional to radius squared. The common factor π is omitted because it cancels when only relative masses matter.

### Penetration correction

Discrete simulation steps can leave circles slightly embedded. The engine moves both bodies apart along the collision normal, distributing the correction according to inverse mass.

### Important edge cases

- Separated circles receive no response.
- Touching circles moving apart receive no extra impulse.
- Equal elastic circles exchange velocities in a head-on collision.
- Coincident centres use a deterministic fallback normal instead of dividing by zero.

### Verification

- Collision detection and response tests pass through CTest.
- The project builds with all warnings enabled.
- `git diff --check` reports no whitespace errors.

### Next step

Visually verify bouncing, then separate the body model from rendering so future shapes and solvers can reuse it.

## 2026-08-28 — Separate physics from rendering

### Objective

Remove SFML drawing objects from the reusable physics model.

### Previous coupling

The original `PhysicsBody` stored motion, collision properties, and an `sf::CircleShape` together. That made the simulation dependent on a graphical window and would complicate testing or adding another renderer.

### New design

- `physics::CircleBody` owns position, velocity, acceleration, radius, inverse mass, restitution, integration, and boundary response.
- `CircleView` owns the SFML shape and synchronizes it from a `CircleBody`.
- Collision response accepts complete `CircleBody` objects through a concise overload.
- The `physics-core` library compiles independently from SFML Graphics and Window.

### Verification

- Added tests for semi-implicit integration, circle centres, wall response, floor friction, and bounce suppression.
- Both the body-dynamics and collision test suites pass.
- The full graphical application compiles with warnings enabled.

### Why this matters

The engine can now evolve independently of its visual frontend. This prepares it for rectangles, interactive tools, headless benchmarks, and a future browser renderer.

### Next step

Add mouse interaction so users can spawn, select, drag, and throw circles while preserving the separation between input, rendering, and physics.

## 2026-08-28 — Add direct interaction controls

### Objective

Turn the passive demonstration into the first version of an interactive physics laboratory.

### Controls

- Click empty space to spawn a circle.
- Hold the left mouse button on a circle to drag it.
- Release a dragged circle to throw it using the measured mouse velocity.
- Press `Space` to pause or resume.
- Press `N` while paused to advance exactly one fixed physics step.
- Press `R` to restore the original scene.

### Design decisions

- Input and selection state remain in the SFML application rather than `physics-core`.
- A dragged body temporarily has zero inverse mass, making it act like a user-controlled kinematic object during collisions.
- Throw velocity is capped to prevent noisy mouse timing from creating extreme impulses.
- The selected body is gold, detected contacts are red, and ordinary bodies are white.

### Verification

- The physics core and graphical application compile with warnings enabled.
- Both automated test suites continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Visually test all controls, then add an on-screen status and parameter panel for gravity, spawn radius, restitution, body count, and simulation performance.
