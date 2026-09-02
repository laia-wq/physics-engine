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

## 2026-08-29 — Add persistent selection and body inspection

### Objective

Let users examine and manage individual simulation objects without interrupting the rest of the scene.

### Interaction changes

- A body remains selected after the mouse is released instead of being selected only while dragged.
- Selection and dragging are represented as separate application states.
- The selected body is highlighted and its mass, centre position, velocity, radius, and restitution are displayed.
- The selected body can be removed with a panel button, Delete, or Backspace.

### Environmental controls

Gravity and wind are now two-dimensional controls with X and Y components. Users can create horizontal, vertical, or diagonal fields while the engine preserves the physical distinction between mass-independent gravity and mass-sensitive force.

### Why this matters

Persistent inspection makes the laboratory useful for experimentation rather than only observation. Separating selection from dragging also prepares the interface for editing future rectangles, static bodies, and material properties.

### Verification

- The application compiles with warnings enabled.
- All body, collision, and broad-phase tests continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Add named preset scenes and begin a minimal Canvas mode that presents the same physics with a cleaner visual style.

### Interface refinement

Environmental controls can intentionally oppose each other, but the original panel made cancellation look accidental. Gravity and wind now have independent enable and reset controls, while the selected-body inspector displays their combined acceleration. This is important because the same wind force creates different acceleration for bodies with different masses.

The laboratory panel now uses collapsible sections and a smaller default size. Core playback controls remain visible, while environment, selection, spawning, debug tools, and performance details can be expanded as needed.

## 2026-08-29 — Add vector pads and demonstration presets

### Objective

Make two-dimensional fields direct to control and make important behaviors repeatable.

### Vector-pad interaction

Gravity and wind now use square control surfaces rather than separate horizontal sliders. The centre represents zero, direction from the centre represents field direction, and distance represents strength. Diagonal input changes X and Y together in one gesture.

Gravity and wind remain separate pads because they have different relationships to mass.

### Preset scenes

- Classic restores the original three-body gravity scene.
- Head-on creates an elastic two-body collision without environmental fields.
- Zero-G drift demonstrates motion and collisions without gravity or wind.
- Particle rain creates a repeatable many-body falling scene.

Presets configure both scene contents and environmental state, making demonstrations more reliable for testing, videos, and interviews.

### Verification

- The application compiles with warnings enabled.
- All body, collision, and broad-phase tests continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Begin Canvas mode with a hidden laboratory panel, curated colour palettes, and optional motion trails.

## 2026-08-29 — Introduce Canvas mode

### Objective

Present the same simulation as an expressive interactive artwork without weakening or duplicating the underlying engineering system.

### Presentation modes

- Laboratory mode retains controls, inspection, debug rendering, and measurements.
- Canvas mode hides the laboratory panel and renders bodies with curated additive colour palettes.
- `Tab` switches modes, `T` toggles trails, and `P` cycles palettes.

### Motion-trail technique

Canvas mode draws into a persistent off-screen texture. Each frame adds the current bodies and covers older pixels with a slightly transparent background colour. Previous positions therefore fade gradually instead of disappearing immediately.

This is strictly a rendering effect. Body state, forces, collision response, fixed-timestep integration, and broad-phase selection are shared unchanged between both modes.

### Verification

- The graphical application compiles with warnings enabled.
- All physics and spatial-grid tests continue to pass.
- `git diff --check` reports no whitespace errors.

### Next step

Visually tune palettes and trail persistence, then begin axis-aligned rectangular rigid bodies.

## 2026-08-29 — Add interactive point fields

### Objective

Let users shape Canvas motion directly and explore collective patterns produced by position-dependent fields.

### Field model

- Attraction accelerates bodies toward a movable point.
- Repulsion reverses the radial field and pushes bodies away.
- A perpendicular component produces vortex motion around the point.
- Softening limits acceleration near the centre and avoids a numerical singularity.

The field follows an inverse-square-inspired model. It is suitable for classical gravitational, electrostatic, orbital, and vortex demonstrations, but is not presented as a quantum simulation.

### Canvas interaction

- A compact Canvas panel exposes gravity, wind, radial strength, and vortex strength.
- `C` hides or reveals Canvas controls.
- Right-click moves the point field directly in the scene.
- A visible ring marks the current field location.

### Scale

Stress scenes now include 600 and 1,000 field-driven bodies and automatically enable the spatial grid. Body-to-body collisions remain available as an experimental toggle, while a future lightweight particle mode will target 10,000 bodies without claiming full rigid-body collision behavior at that scale.

### Next step

Visually validate the new fields and large scenes, tune stability, and then begin axis-aligned rectangular rigid bodies.

### Visual-test corrections

Basic presets now explicitly disable point fields, preventing attraction or vortex state from leaking into later gravity experiments. Population buttons also restore a known gravity-only environment instead of silently preserving a previous field preset.

Body-count controls are prominent in both Laboratory and Canvas modes. The 600- and 1,000-body scenes default to collisions disabled for smooth mixed-size, field-driven visualization. Full collisions can be re-enabled as an experimental option, but dense piles remain limited by the current single-pass contact solver.

Point placement now has an explicit button followed by a click in the scene, while Canvas right-click placement remains available.

### Further interaction corrections

The Canvas field marker can now be grabbed and dragged continuously with the left mouse button. This makes the effect of a moving attractor or vortex observable in real time and removes dependence on right-click support.

Wind now uses an aerodynamic size model instead of a uniform-flow option. Exposed width grows with radius while default mass grows with radius squared, so smaller bodies accelerate more strongly without the extreme difference caused by applying one identical force to every body. A sensitivity slider exaggerates or softens this effect without changing collision mass.

Population generation supports small, medium, and large body types. Each type has a radius and a percentage of the population; percentages are normalized automatically when a scene is regenerated. Individually spawned bodies can either derive mass from radius or use a separately specified mass, including zero for a static body.

Field dragging now works in both modes, uses a larger invisible grab area around the marker, and begins immediately when the move/drag placement control is used.

The size-mixture editor now groups radius and population percentage under each ball type. Editing one percentage proportionally rebalances the other two, so the population total always remains exactly 100%.

Laboratory and Canvas now use one shared physics-control panel. Switching views changes only the rendering style; bodies, forces, fields, presets, inspection, materials, and performance settings remain continuously available. `C` hides or restores the panel in either view, while Canvas-only trail and palette settings appear in a small section of the same panel.

Restart semantics now use a saved body snapshot from the most recently loaded scene or generated population. `R` and the Restart button restore those starting positions and velocities while preserving the current gravity, wind, point-field, and material controls. The Classic preset remains the explicit way to return to the original three-ball demonstration.

The shared panel exposed an ImGui identifier collision between the Vortex preset button and Vortex field-strength slider. Their visible labels remain unchanged, but each now has a distinct hidden identifier so ImGui can track them independently.

## 2026-08-29 — Multiple and oscillating point fields

### Goal

Replace the single global point field with a reusable collection so several attractors, repulsors, and vortices can interact in one experiment.

### Implementation

- Each field stores its own position, radial strength, vortex strength, enabled state, oscillation amount, and frequency.
- Every body receives the vector sum of all enabled field accelerations.
- Coloured field rings can be selected and dragged in Laboratory or Canvas view.
- The shared panel can add, select, edit, disable, and delete individual fields.
- Cyan rings represent radial attraction, magenta rings represent repulsion, and yellow rings indicate vortex behavior. The selected ring is thicker.
- Oscillation scales a field sinusoidally. Amounts below 1 pulse without reversing; amounts above 1 can alternate between attraction and repulsion.

### Engineering significance

The field system now demonstrates superposition and removes a single-instance design limitation. Per-field state makes later field types, serialization, and deterministic replay possible without adding another set of global variables for every feature.

### Interaction tools and temporary forces

Left-click behavior is now explicit rather than contextually spawning bodies. Select/throw prioritizes body inspection and does nothing on empty space; Spawn body creates particles intentionally; Move fields edits permanent field markers; and Force pulse creates a short-lived field at the click position. Pulse radial strength, vortex strength, and lifetime are adjustable, with attract, repel, and vortex shortcuts. Temporary fields use the same force-superposition calculation as permanent fields and are removed automatically or when the experiment restarts.
