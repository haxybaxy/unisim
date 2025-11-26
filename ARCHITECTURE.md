# Unisim Architecture

## Overview

Unisim is a modular n-body physics simulation application built with C++ and GTK4. The architecture is designed to be pluggable, performant, and extensible.

## Project Structure

```
unisim/
├── src/
│   ├── main.cpp                 # Application entry point
│   ├── simulation/              # Core physics simulation
│   │   ├── vector3d.hpp        # 3D vector math
│   │   ├── body.hpp            # Physical body representation
│   │   ├── universe.hpp        # Container for all bodies
│   │   ├── integrators/         # Numerical integrators
│   │   │   ├── integrator.hpp  # Abstract base
│   │   │   ├── euler.cpp       # Euler method
│   │   │   ├── verlet.cpp      # Verlet integrator
│   │   │   └── leapfrog.cpp    # Leapfrog integrator
│   │   └── force_computers/    # Force computation methods
│   │       ├── force_computer.hpp  # Abstract base
│   │       └── brute_force.cpp     # O(N²) brute force
│   ├── rendering/              # Visualization
│   │   ├── renderer.hpp        # Abstract base
│   │   ├── renderer_2d.cpp     # 2D top-down view
│   │   └── renderer_3d.cpp     # 3D perspective view
│   ├── initial_conditions/     # Initial condition generators
│   │   ├── initializer.hpp     # Abstract base
│   │   └── random.cpp          # Random distribution
│   ├── compute/                # Compute backends
│   │   ├── compute_backend.hpp # Abstract base
│   │   ├── cpu_backend.hpp     # CPU implementation
│   │   └── gpu_backend.cpp     # GPU (Metal) stub
│   └── ui/                     # User interface
│       ├── main_window.cpp     # Main application window
│       ├── viewport.cpp        # Simulation viewport
│       └── control_panel.cpp   # Control widgets
└── CMakeLists.txt
```

## Design Principles

### 1. Pluggable Architecture

All major components use abstract base classes with virtual functions:

- **Integrator**: `Integrator` base class for numerical methods
- **ForceComputer**: `ForceComputer` base class for force computation
- **Renderer**: `Renderer` base class for visualization
- **Initializer**: `Initializer` base class for initial conditions
- **ComputeBackend**: `ComputeBackend` base class for CPU/GPU

New implementations can be added by:

1. Inheriting from the base class
2. Implementing virtual methods
3. Registering in `MainWindow::setup_simulation()`

### 2. Modern C++ Practices

- **RAII**: All resources managed automatically
- **Smart Pointers**: `std::shared_ptr` for shared ownership
- **Move Semantics**: Universe is movable but not copyable
- **const Correctness**: Methods marked const where appropriate
- **Namespaces**: All code in `unisim` namespace

### 3. Separation of Concerns

- **Simulation**: Pure physics, no UI dependencies
- **Rendering**: Visualization only, uses Cairo
- **UI**: GTK4 widgets, delegates to simulation/renderer
- **Compute**: Backend abstraction for future GPU acceleration

## Component Details

### Simulation Layer

#### Vector3D

3D vector with standard operations (+, -, \*, /, dot, cross, normalize).

#### Body

Represents a physical body with:

- Position (`Vector3D`)
- Velocity (`Vector3D`)
- Acceleration (`Vector3D`)
- Mass (`double`)
- Radius (`double`) - for visualization

#### Universe

Container for bodies with STL-like interface. Movable but not copyable for performance.

#### Integrators

**EulerIntegrator**: First-order, simple but inaccurate

```cpp
v(t+dt) = v(t) + a(t)*dt
x(t+dt) = x(t) + v(t)*dt
```

**VerletIntegrator**: Second-order, symplectic, better energy conservation

```cpp
x(t+dt) = 2*x(t) - x(t-dt) + a(t)*dt²
```

**LeapfrogIntegrator**: Second-order, symplectic, velocity-centered

```cpp
v(t+dt/2) = v(t) + a(t)*dt/2
x(t+dt) = x(t) + v(t+dt/2)*dt
v(t+dt) = v(t+dt/2) + a(t+dt)*dt/2
```

#### Force Computers

**BruteForce**: O(N²) computation, computes forces between all pairs. Includes softening parameter to prevent singularities.

**Barnes-Hut**: O(N log N) octree that approximates distant groups of bodies as a single center of mass, controlled by an opening-angle parameter.

**FastMultipole**: O(N) multipole expansion (monopole + dipole) with dual-tree traversal and local expansions that delivers higher accuracy than Barnes-Hut while keeping runtime linear.

### Rendering Layer

**Renderer2D**: Top-down or side view, projects 3D to 2D, draws bodies as circles with velocity vectors.

**Renderer3D**: Simple orthographic projection with depth shading. TODO: Proper perspective projection.

### Initial Conditions

**RandomInitializer**: Generates random positions, velocities, and masses within configurable bounds.

**Future**: Galaxy formation, circular orbits, custom configurations.

### Compute Backends

**CpuBackend**: Always available, uses standard CPU computation.

**GpuBackend**: Stub for future Metal implementation. Will offload force computation to GPU.

## UI Layer

### MainWindow

- Manages simulation state
- Coordinates UI components
- Handles simulation loop (60 FPS)
- Manages pluggable components

### Viewport

GTK4 drawing area that renders the universe using the selected renderer.

### ControlPanel

Provides controls for:

- Play/Pause simulation
- Reset simulation
- Number of bodies
- Time step (dt)
- Integrator selection
- Force method selection
- Renderer selection (2D/3D)
- Initial condition selection

## Extension Points

### Adding a New Integrator

1. Create `src/simulation/integrators/my_integrator.hpp`:

```cpp
class MyIntegrator : public Integrator {
    void step(Universe& universe, double dt) override;
    const char* name() const override { return "MyIntegrator"; }
};
```

2. Implement in `.cpp` file
3. Register in `MainWindow::setup_simulation()`

### Adding a New Force Method

1. Create `src/simulation/force_computers/my_method.hpp`:

```cpp
class MyForceMethod : public ForceComputer {
    void compute_forces(Universe& universe) override;
    const char* name() const override { return "MyMethod"; }
};
```

2. Implement force computation
3. Register in `MainWindow::setup_simulation()`

### Adding a New Renderer

1. Inherit from `Renderer`
2. Implement `render()` method
3. Register in `MainWindow::setup_simulation()`

## Performance Considerations

- **Universe** uses `std::vector<Body>` for cache-friendly memory layout
- **BruteForce** is O(N²) - suitable for N < 1000
- **Future optimizations**:
  - Barnes-Hut for O(N log N)
  - GPU acceleration for large N
  - Spatial partitioning
  - Parallel force computation

## Build System

CMake with:

- C++17 standard
- GTK4 via pkg-config
- Metal framework linking (for future GPU support)
- Compile commands generation for IDE support

## Future Enhancements

1. **Barnes-Hut Tree**: O(N log N) force computation
2. **Fast Multipole Method**: O(N) force computation
3. **GPU Acceleration**: Metal compute shaders
4. **More Initializers**: Galaxy, circular orbits, custom
5. **3D Rendering**: Proper perspective projection
6. **Performance Profiling**: Built-in timing/metrics
7. **Save/Load**: Serialize simulation state
8. **Export**: Video/image export
