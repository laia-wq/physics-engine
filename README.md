# Physics Engine

A 2D physics engine built from scratch in C++17 and visualized with SFML 3.

The project is an engineering sandbox for learning how motion, collision detection,
collision response, spatial partitioning, and numerical simulation work beneath a game engine.

## Current features

- Gravity and velocity-based motion
- Frame-rate-independent physics at 120 simulation steps per second
- Circle-circle overlap detection with collision highlighting
- Mass-aware impulse response and penetration correction
- Physics model separated from SFML rendering
- Automated tests for body dynamics and boundary response
- Circular bodies with configurable radius and restitution
- Collision response against the window boundaries
- Simple floor friction and resting-bounce suppression
- Three-body demonstration scene
- Click empty space to spawn a circle
- Drag and release circles to throw them
- Press `Space` to pause or resume
- Press `N` to advance one physics step while paused
- Press `R` to reset the simulation
- On-screen laboratory panel with live statistics and controls for gravity,
  spawn radius, restitution, and floor friction
- Panel buttons to pause, single-step, reset, or clear the simulation
- Optional debug drawing for body velocities, contact points, and collision normals
- Live collision-search measurements and reproducible 100- and 300-body stress scenes
- Optional uniform-grid broad phase that rejects distant collision pairs
- Visible grid overlay and live candidate-reduction percentage
- Per-body force accumulation with an interactive wind-force demonstration
- Persistent object selection with mass, position, velocity, and material data
- Delete selected bodies from the panel or keyboard
- Two-dimensional gravity and wind controls for diagonal environmental fields
- Compact, collapsible laboratory panel with field toggles and reset controls
- Selected-body net acceleration display for understanding combined fields
- Two-dimensional vector pads for intuitive field direction and strength
- Named classic, head-on collision, zero-gravity, and particle-rain scenes
- Canvas mode with additive colour palettes and optional fading motion trails
- Press `Tab` to switch modes, `T` for trails, and `P` to cycle palettes
- Interactive inverse-square attraction, repulsion, vortex, and orbital presets
- Right-click point-field placement and hideable Canvas controls
- Spatial-grid stress scenes for 600 and 1,000 field-driven bodies, with experimental collisions
- Prominent population controls and optional body collisions in both modes
- Explicit field-placement mode for trackpads and other one-button input
- Draggable point fields and adjustable aerodynamic wind sensitivity
- Three-size population mixtures with adjustable radii and clear percentages
- Independent radius and mass controls for individually spawned bodies
- One shared physics panel in Laboratory and Canvas views; press `C` to hide it
- Restart the currently loaded experiment with `R` without changing its fields
- Multiple independently selectable, draggable, and editable point fields
- Per-field oscillation controls for pulsing and alternating force patterns
- Explicit select, force-pulse, body-spawn, and field-editing interaction tools
- Positive, neutral, and negative particle charges with adjustable mixtures
- Charge-sensitive permanent fields, temporary pulses, and separation preset
- Optional particle-to-particle electrostatics with live pair-count profiling
- Tested like-charge repulsion, opposite-charge attraction, and neutral behavior
- Damped Hooke-law springs with interactive particle connections
- Restartable spring-chain preset with visible tension and compression
- Pinnable anchors, scalable chains and soft-body lattices, and a radial spring web
- Connected-system trails in Canvas mode and uniformly small default particles
- Adjustable web topology, pinned-anchor selection, and a 1,200-particle full-screen lattice
- Optional strain-based spring failure and a tearable full-screen lattice

## Requirements

- A C++17-compatible compiler
- CMake 3.22 or newer
- SFML 3

CMake downloads pinned versions of Dear ImGui and ImGui-SFML the first time the
project is configured. They provide the control-panel widgets; all simulation
and collision physics remain implemented in this repository.

## Build and run

```sh
cmake -S . -B build
cmake --build build
./build/physics-engine
```

## Roadmap

- Rotating rectangular rigid bodies
- Stable friction and resting contacts
- Automated performance benchmark executable
- Automated tests and performance benchmarks
- Multiple simultaneous contact solving

## Architecture

The reusable `physics-core` library owns body state, integration, boundary response, and collision mathematics. The SFML application owns visual shapes and synchronizes them from the physics state. This keeps simulation logic testable without opening a window.

## Engineering notes

Design decisions and experimental results are recorded in
[`docs/engineering-log.md`](docs/engineering-log.md).

## Status

This project is under active development. The current boundary-collision demo is the baseline for the full physics engine.
