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

## 2026-08-28 — Add a live laboratory control panel

### Objective

Make physics parameters observable and adjustable while the simulation runs.

### Controls and instrumentation

- Display running or paused state, body count, render rate, and the fixed physics rate.
- Adjust gravity, spawn radius, spawn restitution, and floor friction with sliders.
- Pause, single-step, reset, and clear the scene with visible buttons.
- Keep the existing keyboard controls for efficient use.

### Design decisions

- Use Dear ImGui through the official ImGui-SFML bridge instead of implementing generic UI widgets inside the physics project.
- Pin both dependency versions in CMake so builds remain reproducible.
- Respect ImGui's mouse and keyboard capture flags so panel input cannot accidentally manipulate the simulation beneath it.
- Keep panel and rendering code in the application layer; `physics-core` remains independent and testable.

### Verification

- The complete application compiles with warnings enabled.
- Both automated physics test suites continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Visually verify the panel and begin debug drawing for velocity vectors, collision normals, and contact points.

## 2026-08-29 — Visualize hidden physics data

### Objective

Expose important internal simulation values so collision behavior can be inspected rather than inferred only from moving shapes.

### Visual language

- Green lines begin at body centers and represent velocity direction and magnitude.
- Magenta markers identify calculated circle-circle contact points.
- Cyan lines begin at contacts and show the collision normal from the first body toward the second.

### Design decisions

- Make each overlay independently selectable from the laboratory panel.
- Scale and cap velocity lines so high speeds remain readable on screen.
- Derive contact visualization from body state without changing collision response.
- Keep all debug rendering outside `physics-core`; the simulation library still has no graphics dependency.

### Verification

- The application compiles with warnings enabled.
- Both automated physics test suites continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Visually verify the overlays, then measure the current all-pairs collision algorithm before introducing spatial partitioning.

### Visual verification correction

The first implementation reconstructed contacts after collision resolution. Because penetration correction had already separated the bodies, brief impacts could disappear before rendering. Contact points and normals are now captured inside the simulation step immediately before response, then retained until the frame is drawn.

## 2026-08-29 — Establish an all-pairs performance baseline

### Objective

Measure the current collision search before replacing it with a more advanced algorithm.

### Instrumentation

- Count every candidate body pair tested during one physics step.
- Count how many of those candidates are actual contacts.
- Measure the duration of a complete physics step in milliseconds.
- Provide repeatable 100-body and 300-body scenes from the control panel.

### What the baseline demonstrates

The current nested-loop search compares every body with every body after it. This avoids duplicate checks, but the amount of work still grows rapidly: 100 bodies require 4,950 pair checks per step, while 300 require 44,850. Most pairs are far apart, so most detailed collision checks are wasted work.

### Design decision

Keep this baseline available in the interface. After spatial partitioning is implemented, the same scenes and measurements will provide a direct before-and-after comparison rather than relying on an unsupported performance claim.

### Verification

- The application compiles with warnings enabled.
- Both automated physics test suites continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Implement a uniform spatial grid that sends only nearby bodies to narrow-phase collision detection, then compare its candidate counts against this baseline.

## 2026-08-29 — Add uniform-grid spatial partitioning

### Objective

Reduce wasted collision checks by considering only bodies that occupy at least one common region of space.

### Approach

The world is divided into equally sized square cells. Each circle is registered in every cell touched by its bounds. Bodies that never share a cell cannot collide, so they are removed before the detailed circle collision calculation.

### Comparison controls

- Switch between the original all-pairs search and the uniform grid at runtime.
- Display the total possible pairs separately from the candidates produced by the selected search.
- Adjust grid cell size to observe how partition size affects candidate count and runtime.
- Reuse the same 100-body and 300-body scenes for a fair comparison.

### Correctness safeguards

- Register large circles in multiple cells so collisions across cell boundaries are not missed.
- Deduplicate body pairs when two circles share more than one cell.
- Test distant bodies, nearby bodies, and bodies spanning a grid boundary.

### Verification

- The complete project compiles with warnings enabled.
- Circle dynamics, collision response, and uniform-grid tests pass.
- `git diff --check` reports no whitespace errors.

### Next step

Record before-and-after measurements, then visualize the grid and occupied cells to make the optimization understandable on screen.

### Measured comparison

Measurements were captured on the same machine, scenes, and 50-pixel cell size:

| Bodies | Search | Candidate checks | Physics step |
| ---: | --- | ---: | ---: |
| 100 | All pairs | 4,950 | 0.798 ms |
| 100 | Uniform grid | 378 | 0.359 ms |
| 300 | All pairs | 44,850 | 7.704 ms |
| 300 | Uniform grid | 3,705 | 2.233 ms |

The grid removed approximately 92% of candidate checks. In the 300-body scene, the measured physics-step time decreased by approximately 71%.

The UI now reports candidate reduction directly and can draw the grid behind the bodies. This connects the algorithm's numerical effect to an immediate visual explanation.

### Repository hygiene

ImGui's local window-layout file is disabled and ignored because panel positions are personal runtime state, not project source code.

### Next step

Commit the verified grid milestone, then add force accumulation so users can apply forces without directly rewriting body acceleration.

## 2026-08-29 — Add force accumulation

### Objective

Allow multiple independent systems to apply forces to a body during the same physics step.

### Previous limitation

Each body had one acceleration value. A future spring, wind field, or attraction system would have to rewrite that value and could accidentally erase another effect.

### New model

- Forces are added to a per-body accumulator during a physics step.
- The accumulated force is converted to acceleration using inverse mass.
- Base acceleration, such as uniform gravity, is combined with force-based acceleration.
- The accumulator is cleared after integration so temporary forces must be deliberately applied each step.

The laboratory includes a horizontal wind-force control. Because circle mass is proportional to radius squared, the same force produces greater acceleration in smaller circles than in larger circles.

### Verification

- Tests confirm that multiple forces combine.
- Tests confirm that mass affects the resulting velocity change.
- Tests confirm that forces clear after integration instead of leaking into later steps.
- All existing collision, body, and broad-phase tests continue to pass.

### Next step

Add object selection and an inspector that displays the selected body's mass, position, and velocity and allows it to be deleted.
