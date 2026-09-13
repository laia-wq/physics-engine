# Controls Guide

## Getting started

1. Choose a scene from **Preset Gallery**.
2. Choose an **Interaction tool** in **Physics Controls**.
3. Click or drag inside the scene.
4. Press `R` whenever you want to restore the loaded preset.

Physics Controls and Preset Gallery are shared by Laboratory and Canvas. Press
`C` to hide or restore both windows, and `H` for the compact in-app guide.

## Interaction tools

| Tool | Use |
|---|---|
| Select / throw | Click a particle to inspect it; drag and release to throw it |
| Force pulse | Click to create a temporary attractor, repulsor, or vortex |
| Spawn body | Click to add a particle using the chosen radius, mass, material, restitution, and charge |
| Move fields | Drag a coloured field ring to reposition it in real time |
| Cut connections | Drag across connection lines to detach parts of a mesh |

## Views and shortcuts

| Input | Action |
|---|---|
| `Tab` | Switch Laboratory / Canvas |
| `C` | Show or hide Physics Controls and Preset Gallery |
| `H` | Show or hide Quick Help |
| `Space` | Pause or resume |
| `N` | Advance one fixed physics step while paused |
| `R` | Restore and resume the current preset |
| `Delete` / `Backspace` | Remove the selected particle |
| `T` | Toggle Canvas motion trails |
| `P` | Select the next Canvas palette |

## Important concepts

- **Gravity** gives every movable particle the same acceleration.
- **Wind** is size-sensitive, so smaller and lighter particles respond more.
- **Restitution** controls bounciness from `0` (little rebound) to `1` (most rebound).
- **Rigid distance constraints** keep connected particles approximately the same
  distance apart, making structures less stretchy than springs.
- **Breaking strain** controls how far a connection may stretch before tearing.
- **Charge-sensitive fields** send positive and negative particles in opposite
  directions while neutral particles ignore the field.
- **Spatial grid** checks nearby particles instead of every possible pair. It
  helps large scenes but has overhead in small ones.

## Repeatable experiments

Restart always restores bodies and connections, removes temporary force pulses,
clears selections and editing state, resets simulation time and counters, and
resumes the scene. Loading a different preset replaces the current experiment;
**Clear** removes all current bodies and connections.
