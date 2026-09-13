# Engineering Log

This log records important decisions, evidence, problems, and lessons from development.

## 2026-09-13 — Extract rolling terrain and retire portrait code

- Removed the unused portrait generator after it had already been removed from the public preset gallery.
- Moved the full rolling-terrain and city generator into the artistic-presets library, including projection, hills, buildings, windows, depth metadata, paths, boundary meshes, and optional diagonals.
- Reduced `main.cpp` from 3,914 to 3,061 lines across the portrait removal and terrain extraction.
- Added checks for dense terrain geometry, valid connections, stable boundary anchors, substantial building geometry, deterministic reconstruction, and diagonal topology.

### Engineering significance

Historical portrait work remains documented in this log, but unused production code no longer adds maintenance cost. The largest showcase scene is now independently constructible and testable, while `main.cpp` only selects its experiment settings.

## 2026-09-13 — Prevent physics backlog lockup

- Diagnosed a heavy scene rendering at 4 FPS with 10.799 ms physics steps, exceeding the 8.33 ms budget required for 120 Hz simulation.
- Capped automatic physics work at two fixed substeps per rendered frame.
- Discards overdue whole steps when the cap is reached, while retaining the fractional accumulator remainder for stable stepping.
- Added a visible overload message in Performance when protection activates.

### Engineering significance

An unlimited fixed-timestep catch-up loop can enter a feedback cycle commonly called the spiral of death: slow physics creates a backlog, processing the backlog delays rendering, and that delay creates still more backlog. The cap preserves input and interface responsiveness under overload without reducing scene geometry.

## 2026-09-13 — Extract particle presentation and windblown tree

- Moved the graphical particle wrapper, material names, colours, field response, and breaking response into an application presentation module.
- Moved the full windblown-tree generator into an artistic-presets library: hill mesh, layered trunk, irregular branches, foliage clusters, anchors, and leaf attachments.
- Kept only the tree's experiment-level settings in `main.cpp`, including wind, constraint iterations, collisions, and restart state.
- Added automated checks for valid connections, stable anchors, fragile foliage, detailed mesh size, and deterministic reconstruction after mutation.
- Kept per-particle material and shape helpers inline in their new header. These functions run thousands of times per frame in detailed scenes, so retaining inlining avoids a responsiveness regression while preserving the module boundary.
- Reduced `main.cpp` from 4,310 to 3,893 lines.

### Engineering significance

The tree is now an independently constructible scene rather than several hundred lines inside the UI application. Separating `CircleView` also creates a shared presentation boundary for later rendering and artistic-preset extraction.

## 2026-09-13 — Extract the particle-mesh apple

- Moved the apple's nonlinear silhouette profile, projected depth spacing, body mesh, structural stem, materials, anchors, and connection graph into the preset library.
- Extended data-only preset definitions with material, group, and fixed-outline metadata so artistic scenes retain their physical and visual meaning outside `main.cpp`.
- Added checks for all 1,674 particles, 6,425 connections, flexible fruit material, fixed stem tip, and valid topology.
- Reduced `main.cpp` from 4,469 to 4,310 lines.

### Engineering significance

The apple now documents the project's central visual technique in reusable code: nonlinear spacing and connected projected points create apparent volume without a 3D renderer. Its interface loader contains only scene-level physics settings.

## 2026-09-13 — Extract connected preset topology

- Moved spring-chain, soft-body lattice, and radial-web particle placement and connection generation into the preset-construction library.
- Added one application adapter that converts data-only particle and connection definitions into rendered simulation objects.
- Preserved configurable chain stiffness, damping, spacing, lattice dimensions, and web ring/spoke counts.
- Added topology checks for body and connection counts, pinned anchors, positive rest lengths, and valid connection indices.
- Reduced `main.cpp` from 4,592 to 4,469 lines.

### Engineering significance

The preset library now owns both independent-particle scenes and connected graph structures. The interface only chooses parameters and applies the resulting scene, which is a cleaner separation between UI decisions and scene construction.

## 2026-09-13 — Extract foundational preset construction

- Added a dedicated preset-construction library for Classic, stress, head-on collision, zero-gravity, rain, and orbit scenes.
- Replaced their repeated particle-building loops in `main.cpp` with one small adapter that turns preset definitions into live bodies.
- Added automated checks for every foundational preset's body count and requested stress-scene size.
- Added a restart/load check that mutates a running preset, regenerates it, and confirms its initial position and velocity return independently.
- Reduced `main.cpp` from 4,677 to 4,592 lines.

### Engineering significance

Preset definitions no longer depend on the graphical application loop. This establishes the boundary needed to move the larger connected and artistic presets out incrementally without performing a risky all-at-once rewrite.

## 2026-09-12 — Extract mathematical surface geometry

- Moved the equations and projection rules for the torus, heart, Mobius strip, sphere, wave, double well, and spacetime well into a dedicated mathematical-surface module.
- Centralized each surface's grid dimensions and edge-wrapping rules alongside its geometry.
- Kept particle creation and spring connections in the preset layer, separating the question "where is this 3D point projected?" from "how is it represented by the physics engine?"
- Reduced `main.cpp` from approximately 4,800 to 4,677 lines without changing the preset controls or appearance.

### Engineering significance

The mathematical formulas no longer live inside the application loop. They can now be understood, reused, and tested independently while the preset loader remains responsible for turning projected points into connected particles.

## 2026-09-12 — Extract and test point-field physics

- Moved the point-field data model and acceleration calculation out of the interface-heavy `main.cpp` and into `physics-core`.
- Preserved radial attraction and repulsion, tangential vortex motion, oscillation, material response, and charge-sensitive behavior.
- Added focused automated tests for every one of those field behaviors, including disabled fields.
- Deliberately skipped scene saving and particle emitters because neither is required by the finished wireframe-focused scope.

### Engineering significance

The application now asks the physics library for field acceleration instead of containing the formula inside its UI loop. This makes the behavior reusable and testable without opening the graphical application.

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

## 2026-09-01 — Charged particle populations

### Goal

Give particles an independent signed property so one field can produce different motion across a mixed population without changing gravity, collision mass, or material behavior.

### Implementation

- Circle bodies store a charge from negative through neutral to positive; new bodies default to neutral.
- Population controls generate automatically balanced percentages of positive, neutral, and negative particles.
- Spawn controls set charge independently from radius and mass.
- Charge-sensitive fields reverse their response for negative particles, affect positive particles in the original direction, and exert no electric-style force on neutral particles.
- Electric-style acceleration also accounts for inverse mass, so a lighter charged body responds more strongly than a heavier body with the same charge.
- Charge-aware force pulses use the same temporary-field lifetime system.
- Optional charge colouring uses warm colours for positive particles, cool blue for negative particles, and the normal palette for neutral particles.
- The Charge separation preset provides a controlled demonstration with collisions disabled so the three populations remain visually legible.

### Verification

An automated body test confirms that particles begin neutral and that changing charge does not alter inverse mass. Existing integration, collision, and broad-phase tests remain part of the regression suite.

## 2026-09-02 — Mutual electrostatic interactions

### Goal

Extend charge beyond external fields so charged particles can exert equal-and-opposite forces on one another.

### Implementation

- A physics-core electrostatic pair function applies a softened inverse-square force.
- Like charges repel, opposite charges attract, and pairs containing a neutral particle are skipped.
- Force is equal and opposite, while resulting acceleration differs according to each particle's inverse mass.
- The simulation calculates all base accelerations, accumulates every electrostatic pair, and only then integrates bodies. This avoids order-dependent partially updated motion.
- Strength and softening are adjustable, and the panel reports the number of long-range pair checks.
- A Mutual charges preset starts 150 mixed-charge particles without gravity, wind, external fields, or collisions.
- Unrelated presets explicitly disable mutual electrostatics to prevent state leakage.

### Performance note

Long-range electrostatics currently evaluates every particle pair, producing quadratic growth. The UI warns above 300 bodies. This creates a measured baseline for a later Barnes-Hut or cutoff-based approximation rather than disguising the cost.

### Verification

A dedicated automated test checks like-charge repulsion, opposite-charge attraction, and neutral-pair skipping.

## 2026-09-02 — Damped springs and particle connections

### Goal

Introduce connected particle systems without requiring polygonal rigid bodies.

### Implementation

- A reusable physics-core spring applies Hooke restoring force plus velocity damping along the connection axis.
- Stretched springs pull endpoints together, compressed springs push them apart, and damping opposes relative motion.
- Springs store body indices, rest length, stiffness, and damping.
- The simulation accumulates spring acceleration before integrating any body, preserving consistent step ordering.
- Users can select one body, mark it as the first endpoint, select another body, and connect them at their current separation.
- Deleting a body removes attached springs and safely renumbers remaining connection indices.
- Scene loading clears incompatible connections, while Restart restores the spring network snapshot.
- Connections render pale near equilibrium, warm under tension, and blue under compression in both Laboratory and Canvas views.
- The Spring chain preset creates 18 connected particles with one fixed anchor under gravity.

### Verification

A dedicated automated suite verifies stretched-spring attraction, compressed-spring repulsion, damping against separation, and safe handling of coincident endpoints.

## 2026-09-02 — Anchors and soft-body lattice

### Goal

Use the spring system to demonstrate how connected particles create larger structures and coupled motion.

### Implementation

- Selected particles can be pinned, giving them zero inverse mass so forces cannot accelerate them, or unpinned using radius-derived mass.
- Pinned particles receive a gold outline and can still be repositioned with the drag tool.
- The Soft-body lattice preset connects a 9-by-6 particle grid with horizontal, vertical, and diagonal springs. Diagonals provide shear resistance so the mesh deforms instead of collapsing like an unbraced grid.
- The lattice's upper corners are pinned, creating a hanging deformable sheet.
- Connected presets save their particle and spring state for deterministic Restart behavior.

### Engineering significance

Complex motion emerges from the same tested pairwise spring rule used for manual connections. This demonstrates compositional simulation design: chains and deformable meshes are different network topologies rather than separate hard-coded physics effects.

## 2026-09-02 — Procedural connected structures

### Goal

Make large spring experiments practical to create, visually distinct, and expressive in Canvas mode.

### Implementation

- Generated populations now default to one uniform 5-pixel particle size, while the mixture controls remain available for deliberate variation.
- The chain generator accepts up to 1,000 particles and folds long chains into the visible simulation area.
- Lattice rows, columns, and spacing are adjustable, supporting meshes from a small patch to 1,000 particles.
- Chains, lattices, and radial webs provide visibly different connected structures from the same underlying force model.
- A radial spring web adds concentric and radial connections around a pinned centre, creating a new deformable topology.
- Spring connections are drawn into the persistent Canvas texture, so the structure itself leaves fading traces alongside its particles.
- Presets are grouped by classical, connected-system, and point-field experiments with no more than three buttons per row.
- Manual two-body connection remains available as an advanced tool for small custom edits.

### Engineering significance

The same damped-spring force now supports several topologies at very different scales. Separating topology generation from force calculation makes the system easier to expand with future membranes, webs, and other soft structures without duplicating the underlying physics.

### Verification

The application compiles successfully and all five automated physics suites pass.

## 2026-09-02 — Structure controls and dense-scene interaction

### Goal

Make each procedural generator understandable, keep anchors selectable in dense structures, and make ordinary point fields visually useful without requiring particle charge.

### Implementation

- The generator panel now separates chain, lattice, and web settings according to the topology they affect.
- Chains use particle count; lattices use columns, rows, and spacing; radial webs use independently adjustable ring and spoke counts.
- A full-screen lattice preset creates a 40-by-30 mesh and automatically fits its spacing to the simulation area.
- Body selection checks pinned anchors before other overlapping particles, keeping corner anchors accessible after a dense lattice deforms.
- Charge-independent point fields receive a calibrated visual response multiplier. They remain mass-independent acceleration fields, but attraction, repulsion, and vortex motion are now visible at practical control values.
- Charge-sensitive fields retain their signed, mass-dependent response: positive and negative particles move oppositely, while neutral particles ignore them.

### Engineering significance

Generator parameters now map directly to structure topology, reducing ambiguous controls. The selection rule deliberately prioritizes semantically important anchors when visual overlap makes ordinary draw-order selection unreliable.

### Verification

The application compiles successfully and all automated physics suites pass.

## 2026-09-09 — Iterative distance constraints

### Goal

Add a shape-preserving connection mode needed for stable ropes and recognizable particle structures.

### Implementation

- A reusable physics-core distance constraint corrects two particles toward a requested separation.
- Corrections are divided according to inverse mass, so lighter particles move farther and pinned particles remain fixed.
- Connection stiffness controls how much of the positional error is corrected in each pass.
- The simulation can repeat the complete constraint network from 1 to 20 times per physics step. Later passes correct errors introduced when neighbouring connections moved a shared particle.
- Constraint-driven velocity is reconstructed from the corrected position change, allowing the positional solution to influence subsequent motion.
- Existing chains, lattices, and webs can switch between elastic spring forces and rigid distance constraints without duplicating their topology.
- A Constraint lattice preset provides a direct comparison with the spring-based Soft-body lattice.

### Engineering significance

This introduces iterative position-based constraint solving and exposes its central tradeoff: more solver passes improve shape preservation while increasing computation. Reusing the existing connection graph keeps topology separate from the method used to enforce it.

### Verification

A sixth automated suite verifies exact full-stiffness correction, proportional partial correction, inverse-mass handling for pinned endpoints, and safe coincident endpoints. The application builds without warnings and all six suites pass.

## 2026-09-10 — Particle groups and material regions

### Goal

Allow one connected particle scene to contain regions with different physical roles, providing the foundation for trunks, branches, leaves, foreground objects, and fixed backgrounds.

### Implementation

- Every displayed particle now stores a material and a non-negative group identifier alongside its physics body.
- Structural particles receive 35 percent of ordinary wind and field response and tolerate 1.6 times the global breaking strain.
- Flexible particles use the normal environmental response and breaking threshold.
- Fragile particles receive 140 percent environmental response and break at 40 percent of the global threshold.
- Fixed particles receive no environmental response and have zero inverse mass.
- A connection joining different materials uses the more fragile endpoint's failure scale.
- Selected particles expose editable material and group controls. Pinning assigns Fixed material, while unpinning a Fixed particle restores Flexible material.
- Material colouring is mutually exclusive with charge colouring and works in both Laboratory and Canvas rendering.
- The Material regions preset creates structural, flexible, and fragile bands in one constrained lattice, with two fixed anchors, breakable connections, and horizontal wind.
- Core integration now guarantees that zero-inverse-mass particles ignore velocity, acceleration, and accumulated force.

### Engineering significance

Group identity describes which logical part of an artwork a particle belongs to, while material describes how that part behaves. Keeping those concepts separate will let a future tree contain several leaf groups that all share the same fragile material, or several structural objects with different identities.

### Verification

The project builds without warnings and all six automated suites pass. The body-dynamics suite now includes a regression test proving that fixed particles remain stationary under velocity, acceleration, and applied force.

## 2026-09-10 — Region-based connection breaking

### Goal

Allow users to detach physical regions with one continuous gesture, supporting future leaf shedding, erosion, and interactive particle sculpting.

### Implementation

- A Cut connections interaction tool uses an adjustable circular brush from 5 to 60 pixels.
- Pressing and dragging continuously tests every live connection against the brush and removes intersecting connections.
- Distance-to-segment testing cuts a line when the brush overlaps any point along it, rather than requiring the cursor to hit an endpoint.
- Cutting never deletes particles or changes their material, group, velocity, or remaining connections, allowing severed pieces to continue moving naturally.
- The tool works identically with elastic springs and rigid distance constraints because both reuse the same connection graph.
- A visible translucent brush outline shows the affected region in both Laboratory and Canvas modes.
- A counter reports manually cut connections separately from automatic strain failures.
- Restart restores the saved connection graph and clears the cut count; loading a new connected structure also clears the count.

### Engineering significance

The tool changes network topology at runtime while avoiding invalid particle indices. This is the minimum interaction needed to detach clusters from future layered artwork without introducing deletion or drawing systems prematurely.

### Verification

The project builds without warnings and all six automated physics suites pass. Final verification requires dragging the brush through connected presets and confirming that Restart restores their original topology.

## 2026-09-10 — Silhouette-to-particle artwork

### Goal

Prove that a recognizable, depth-styled image can be generated as a connected physical structure without manually placing every particle.

### Implementation

- The apple is generated as a 37-by-39 curved mesh rather than a coloured, filled-in silhouette.
- A sine projection compresses particles near the left and right edges while leaving more space across the visible front, similar to longitude lines wrapping around a rounded surface.
- Row width changes over the fruit's height, and a small vertical displacement bends the cross-lines around its front surface.
- Horizontal, vertical, and diagonal neighbours are connected automatically, making the mesh itself communicate shape and volume.
- The fruit uses the existing Flexible material colour. Its stem uses Structural material particles and ends in one Fixed anchor.
- No custom artwork colours or painted highlights are used; depth comes from geometry, particle density, and visible connections.
- Artwork particles use a deliberately small radius and a wider projected surface so the connection network remains visible instead of disappearing beneath adjacent circles.
- Both mesh axes use nonlinear projection: connections are longest across the visible front and progressively shorten toward the sides, top, and bottom. A small oblique offset bends the grid asymmetrically, strengthening the impression of a surface viewed in perspective.
- The projected width includes a deliberate upper-shoulder bulge, lower-half taper, and stronger central top notch so perspective variation does not erase the recognizable apple silhouette.
- After comparison with a front-view photographic reference, the procedural oval was replaced by an interpolated width profile: shallow stem cavity, broad upper-middle shoulders, sustained side fullness, and a quicker lower taper. The top notch was reduced and a subtle blossom-end dimple was added.
- The terminal profile widths remain open instead of collapsing toward a point, producing the flatter stem and blossom ends visible across multiple apple references.
- The scene uses distance constraints to preserve the silhouette while remaining responsive to fields, wind, dragging, and connection cutting.

### Engineering significance

The important step is the conversion from a parameterized form to particles, material metadata, and an automatically generated connection graph. The variable spacing provides a reusable visual language for future objects without depending on realistic textures or colours.

### Verification

The project builds without warnings and all six automated physics suites pass. Visual verification should confirm that the mesh reads as rounded before it moves, remains stable after Restart, and responds to existing fields and the cutting brush.

## 2026-09-10 — Windblown tree prototype

### Goal

Apply the particle-art structure to a recognizable landscape subject whose form and motion both communicate physical forces.

### Implementation

- Multiple windswept-tree and branch-silhouette references were compared before construction. Shared features include a low asymmetrical canopy, trunk lean, and branches extending with the prevailing wind.
- A six-row hill mesh compresses particle spacing toward its sides, expands it near the viewer, and uses unequal depth bands. Its lower row is anchored.
- The trunk consists of five tapering, diagonally connected strands with nonlinear spacing across and along its surface. Several internal structural anchors prevent the trunk from swaying like grass.
- Five major branches grow from different trunk heights as tapered three-strand meshes. Their segment lengths change along each branch rather than forming uniform chains.
- Each branch supports a denser curved leaf mesh with its own group identifier and Fragile material. Deliberate internal gaps and separation between clusters use negative space to reveal branches and break up the canopy.
- Foliage was revised after broader reference comparison: smaller particle marks occupy substantially larger projected clusters, several holes are explicitly carved from each cluster, and only alternating diagonals are connected. The result prioritizes open sprays and visible branch gaps over solid oval nets.
- Major branch meshes receive fixed support points every four segments. This keeps their structural silhouette nearly stationary while fragile foliage remains free to deform under a gentler default wind.
- The canopy now uses sixteen differently sized sprays distributed along the interiors and tips of all five branches, instead of limiting foliage to five endpoints.
- Deterministic multi-frequency curves vary the trunk centreline, taper, branch direction, and segment length. This removes mirror-like regularity while keeping Restart reproducible.
- The scene begins with gentle horizontal wind. Material response keeps the hill fixed, moves the structural wood moderately, and moves fragile foliage most strongly.
- Existing connection cutting can detach portions of foliage without adding a separate tree-only interaction.

### Engineering significance

One connection solver now represents terrain, tapered structural members, branching topology, and deformable foliage. Material response creates differential motion from one global force, while groups preserve semantic regions for later scene tools.

### Verification

Build and run all automated suites. Visually confirm that the stationary scene reads as a tree on a hill, foliage moves more than the trunk, the structure remains stable under its default wind, cutting can detach leaves, and Restart reconstructs the complete scene.

## 2026-09-11 — Reference-driven wireframe portrait

### Goal

Test whether recognizable human form can emerge from connection direction, unequal spacing, facial planes, and negative space rather than colour or uniformly dense particles.

### Implementation

- One hundred real front-view portrait thumbnails were reviewed to establish the range of natural facial variation. The generic average was then rejected in favour of the user-provided reference photograph.
- Landmark proportions were measured from that reference: crown, hairline, temples, eye and brow centres, nose, nostrils, lips, cheek width, jaw corners, chin, neck, and shoulder spread.
- A nonlinear head-width profile follows the subject's high cheekbones, narrow lower jaw, rounded chin, and close pulled-back hairline.
- Subtle deterministic asymmetry replaces the earlier artificial three-quarter projection while connection rows continue to bend around facial volume.
- Local mathematical deformations stretch topology around the brow, cheek planes, nose bridge, and lips instead of representing those features with additional colour.
- Elliptical negative spaces interrupt the mesh at the eyebrows, eyes, nostrils, and mouth, causing neighbouring connection loops to outline the reference features.
- Alternating diagonals reduce visual filling and help the horizontal and vertical facial flow remain readable.
- A widening neck-and-shoulder mesh anchors the portrait and continues the stretched-spacing language below the face.

### Engineering significance

The portrait is a reference-driven topology experiment rather than a pixel conversion. Semantic landmarks alter a reusable mesh, demonstrating how geometry and connectivity can encode a specific coherent face without textures.

### Verification

Build and run all automated tests. Visually inspect the stationary portrait before applying forces: head silhouette, eye placement, nose bridge, mouth, jaw, and near-versus-far cheek spacing should remain readable. Then verify controlled deformation, cutting, Canvas mode, and Restart.

## 2026-09-11 — Projected 3D wireframe terrain

### Goal

Create a real three-dimensional terrain surface and project it into the engine's two-dimensional physics world.

### Implementation

- A 45-by-20 terrain surface is defined using world-space horizontal, height, and depth coordinates, retaining the landscape silhouette without oversampling the simulation.
- A perspective-camera calculation projects every 3D vertex into a 2D screen position.
- Broad mountains, a middle ridge, a valley, and restrained undulations combine into one continuous height field.
- Horizontal contour lines and depth lines form a clean quadrilateral grid; diagonal edges are omitted because they obscured the projected mountain slopes.
- Vertex size changes with depth, reinforcing the perspective projection.
- The whole terrain uses one Structural material so geometry, rather than colour bands, communicates depth and fields deform it gently.
- Seven constraint passes replace the original fourteen; together with the lower mesh density, this reduces the terrain's constraint workload to roughly one quarter of the first projected version.
- The sampled world-space width expands with depth to represent a broad ground plane inside the camera view. Projected rows retain modest convergence while filling the sides of the frame, and every vertex remains within x=36..764.
- Broad overlapping ridges and foothills span the full normalized width, while much weaker high-frequency variation keeps their contours readable.
- Three shorter, wider hills occupy the middle and foreground, creating overlapping depth layers without competing with the distant mountain silhouettes.
- Removed the separately generated side and bottom extensions after visual testing revealed mismatched cell proportions and visible seams.
- The primary terrain is now one 49-by-25 surface spanning almost the full frame. Its nearest rows smoothly fade their elevation into the fixed bottom row, so the middle, sides, and foreground use identical topology and spacing rules.
- Broad side and near-distance hills remain part of this continuous height field, while the central ridge stays lower and wider.
- Raised the virtual camera from 300 to 380 world units and compensated the horizon so the foreground still meets the bottom edge. This reveals more of the ground from an elevated, oblique viewpoint rather than looking horizontally across it.
- Broadened and strengthened two middle-distance hills and the foreground rise so terrain relief continues visibly from the distant mountains toward the viewer.
- Added three substantial near-ground hills across the left, centre, and right. Elevation now fades only across the final two foreground rows instead of flattening the lower quarter of the mesh.
- Added depth- and height-dependent lateral displacement so front-to-back grid lines curve around hills instead of remaining vertical. The displacement fades to zero at both outer boundaries to preserve the full-frame surface.
- Replaced linear depth sampling with projected-screen spacing, eliminating the dense horizon/giant foreground-cell imbalance.
- Replaced generic lateral waviness with radial deformation from five actual hills. Depth lines now bow outward on either side of each hill centre and return as the hill falls away, so both grid directions describe the same landforms.
- Expanded the terrain from 25 to 39 depth rows, giving near-ground hills enough intermediate contours to rise smoothly before the fixed bottom row.
- Raised the camera to 722 world units and moved the projection horizon above the window. The far ground now begins near the top of the frame while the foreground remains at the bottom, producing one continuous full-screen terrain view.
- Reduced constraint passes from seven to six, keeping the denser foreground at approximately the same solver cost.
- Strengthened and broadened three near hills, increased their radial line deformation, and limited flattening to the final two rows so middle and bottom relief is easier to read in both grid directions.
- Replaced the forced-flat final row with a gradual foreground transition that retains 32% of its terrain height at the nearest edge, allowing the landscape to end slightly above the screen rather than producing long straight pillars.
- Increased depth resolution from 39 to 45 rows while reducing constraint passes from six to five, improving bottom contour spacing without increasing approximate solver work.
- Extended radial deformation to the foreground and both side hills so depth lines continue curving through the lower landscape.
- Increased middle and near hill heights by roughly 30-40% and narrowed their footprints moderately, producing clearer peaks and valleys comparable to the distant mountains without changing the full-screen mesh spacing.
- Added ten separated wireframe buildings at different depths and heights, including a denser foreground row. Buildings are fixed architectural structures with straight lines; each follows the terrain along its foundation and uses stretched roof and side grids to create a 3D volume.
- Replaced rectangular building occluders with convex outlines traced from each building's actual particles, removing the oversized blank regions around their silhouettes.
- Expanded the city to twelve buildings in staggered background and foreground rows, moving architecture away from the most distorted outer terrain columns and increasing visible side depth.
- Replaced the wide path with one continuous curved route that visits the buildings in a serpentine order and terminates at their entrances rather than passing beneath them.
- Varied facade widths and increased roof depth to three divisions, making the front, side, and roof planes more distinct without relying on different colours.
- Removed four narrow buildings whose three visible planes did not read clearly. Replaced the building-to-building route with a central zigzag and a separate access branch terminating at each remaining entrance.
- Rebuilt the city-path layout around a protected central corridor: ten wide, clearly three-sided buildings occupy left and right clusters, a densely sampled main path curves through the middle, and quadratic curved branches lead from it to every entrance.
- Enlarged the projected roof and side planes, varied foreground footprint widths, and moved all buildings farther from the screen edges so all three faces remain legible.
- Added per-building depth sorting. Each complete silhouette and wireframe now draws from back to front, preventing distant building lines from showing through buildings closer to the camera.

## 2026-09-12 — Mathematical wireframe gallery

### Goal

Make presets the primary showcase and add forms whose projected grids create a strong three-dimensional illusion.

### Implementation

- Added a torus generated from its standard two-angle parametric surface.
- Added a volumetric heart made from heart-curve cross-sections that contract through depth.
- Added a smooth spacetime-well height field and a deeper black-hole embedding analogy with an event-horizon marker.
- Projected the mathematical 3D coordinates into the existing 2D particle-and-connection renderer.
- Reorganized presets in a bordered, scrollable gallery with four complexity levels and a two-column mathematical-form table.
- Clearly labels spacetime wells as visual analogies rather than literal four-dimensional simulations.

### Engineering significance

These scenes separate model-space geometry from screen-space rendering: the program generates connected three-dimensional coordinates first, then projects them into two dimensions. This is the same foundational idea used by a conventional 3D rendering pipeline.

### Refinement

- Merged the separate spacetime and black-hole scenes into one adjustable distortion preset. A live depth slider regenerates the height field from a shallow depression to a deep funnel.
- Rotated the torus projection toward the viewer so its hole and front/back tube structure are easier to read.
- Added a correctly half-twisted Mobius strip, a latitude/longitude sphere, an interference-wave height field, and a two-minimum double-well field.
- Moved presets out of Physics Controls into a dedicated window that persists unchanged between Laboratory and Canvas modes.
- Reordered the gallery so interactive fields are Level 3 and the curated artwork scenes are Level 5; removed the portrait from the gallery.
- Refined the Mobius strip with 56 longitudinal samples, cleaner cross-strip spacing, stronger continuous boundary curves, and an oblique projection that exposes both the rear arc and half-twist.

## 2026-09-12 — Screenshot export

- Added an F12 shortcut and control-panel button that capture the fully rendered application window.
- Creates a local screenshots directory automatically and uses collision-safe millisecond timestamps.
- Reports the absolute saved path or a clear failure message in Physics Controls.
- Hiding controls with C before capture produces clean portfolio and README artwork.

## 2026-09-12 — Triangulated mathematical meshes

- Added one alternating diagonal to each interior mathematical-surface cell.
- Alternation avoids biasing the mesh visually in one direction while revealing how quadrilateral grids can be decomposed into triangles.
- Added a Mathematical Forms checkbox that regenerates the active preset immediately, allowing direct comparison with the cleaner horizontal/vertical grid.
- Added a separate Terrain diagonals option for Rolling Terrain. It triangulates only the hill grid, leaving buildings, windows, roofs, and paths rectangular for visual separation.
- Added live Wave height and Double-well depth controls. Each slider immediately regenerates its corresponding mathematical height field, matching the interaction used by Spacetime distortion.
- Added layered building rendering: terrain draws first, black building silhouettes hide the ground behind them, then facade edges and window grids draw on top. This provides simple 2D occlusion in both Laboratory and Canvas modes.
- The foreground row is invisibly anchored, while the remaining surface can still respond to fields, cutting, and dragging.

### Engineering significance

Perspective is now derived from actual vertex depth rather than hand-adjusted screen spacing. After projection, the existing 2D spring solver can still animate, deform, and break the mesh.

### Verification

Build and run all automated tests. With forces at zero, confirm that the foreground reaches the bottom of the window, rows converge toward the horizon, several unequal hills and a valley are readable, and the mountain sides remain clean. Then test gentle field deformation, cutting, Canvas mode, and Restart.

## 2026-09-02 — Breakable spring materials

### Goal

Let connected structures fail under excessive deformation and make zero acceleration the neutral starting environment.

### Implementation

- Gravity now starts and resets to `(0, 0)` for the application, generated populations, chains, and lattices. Named experiments such as Particle Rain can still deliberately configure gravity.
- The redundant connected-system preset and its dedicated construction code were removed.
- Breakable connections can be enabled for any spring structure.
- Breaking strain measures extension relative to original length: a value of `0.65` breaks a spring after it stretches 65 percent beyond rest length.
- Broken connections are removed after spring forces are evaluated, and a live counter reports material failures since the structure was loaded.
- Restart restores the original connection network and clears the failure counter.
- A Tearable lattice preset loads the 1,200-particle full-screen mesh with spring failure enabled.

### Engineering significance

Strain is dimensionless, so the same threshold works consistently for connections with different rest lengths. This introduces a simple material-failure model while preserving the existing tested Hooke-and-damping force calculation.

### Verification

The application compiles successfully and all automated physics suites pass.
