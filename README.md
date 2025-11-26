# Unisim - N-Body Physics Simulation

A modular, extensible n-body physics simulation application built with C++ and GTK4. Features pluggable integrators, force computation methods, renderers, and compute backends.

## Features

- **Multiple Integrators**: Euler, Verlet, Leapfrog (symplectic)
- **Force Computation**: Brute-force O(N²) (Barnes-Hut and FMM planned)
- **Rendering**: 2D top-down view and 3D perspective (with depth shading)
- **Initial Conditions**: Random distribution (more options planned)
- **Pluggable Architecture**: Easy to add new integrators, force methods, renderers
- **CPU/GPU Backend Abstraction**: Ready for GPU acceleration (Metal on macOS)

## Prerequisites

On macOS, install GTK4 using Homebrew:

```bash
brew install gtk4
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

From the build directory:

```bash
./unisim
```

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed architecture documentation.

## Project Structure

```
src/
├── simulation/        # Core physics (Vector3D, Body, Universe)
├── integrators/       # Numerical integrators (Euler, Verlet, Leapfrog)
├── force_computers/   # Force computation methods (BruteForce)
├── rendering/         # Visualization (2D, 3D renderers)
├── initial_conditions/# Initial condition generators
├── compute/          # Compute backends (CPU, GPU abstraction)
└── ui/               # GTK4 user interface
```

## Usage

1. **Start Simulation**: Click "Play" to start the simulation
2. **Adjust Parameters**: 
   - Change number of bodies
   - Adjust time step (dt)
   - Select different integrator
   - Switch between 2D/3D rendering
3. **Reset**: Click "Reset" to regenerate initial conditions

## Extending

The architecture is designed for easy extension:

- **New Integrator**: Inherit from `Integrator` and implement `step()`
- **New Force Method**: Inherit from `ForceComputer` and implement `compute_forces()`
- **New Renderer**: Inherit from `Renderer` and implement `render()`
- **New Initializer**: Inherit from `Initializer` and implement `initialize()`

See `ARCHITECTURE.md` for detailed extension examples.

## Future Enhancements

- Additional GPU kernels (pressure forces, SPH)
- More initial condition generators
- Improved 3D rendering
- Performance profiling
- Save/load simulation state

## License

MIT
