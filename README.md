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
- Broad-phase collision detection with spatial partitioning
- Automated tests and performance benchmarks
- Debug visualization for contacts, normals, and velocities

## Architecture

The reusable `physics-core` library owns body state, integration, boundary response, and collision mathematics. The SFML application owns visual shapes and synchronizes them from the physics state. This keeps simulation logic testable without opening a window.

## Engineering notes

Design decisions and experimental results are recorded in
[`docs/engineering-log.md`](docs/engineering-log.md).

## Status

This project is under active development. The current boundary-collision demo is the baseline for the full physics engine.
