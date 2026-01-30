# GUMS - A Prototypical CDCL SAT SOLVER

GUMS is an independently implemented **Conflict-Driven Clause Learning (CDCL) SAT solver**.
The project was developed as a **learning an exploration tool** to gain hands-on experience with 
the internal mechanisms of modern SAT solvers and their core algorithmic components.

The solver is **not intended to compete with highly optimized state-of-the-art solvers**, 
but rather to provide a clear and comprehensible implementation of essential CDCL techniques.

---

## Motivation

The primary motivation behind GUMS was to deepen the practical understanding of 
SAT solving beyond theoretical descriptions.
By implementing a CDCL solver from scratch, the project focuses on the interaction 
between decision heuristics, propagation, conflict analysis, and restart strategies.

The solver served as a preparatory project for further research on variable selection
heuristics in CDCL solvers.

---

## Implemented Techniques

GUMS implements a selection of standard techniques commonly found in modern CDCL solvers:

### Core CDCL Components

- Conflict-Driven Clause Learning (CDCL)
- Two-Watched Literals (2WL)
- Unit Propagation (implemented in a commented and explanatory manner)
- Luby-based restart strategy
- Clause database reduction (clause deletion)
- Phase saving

### Variable Selection Heuristics

- Random variable selection
- Jeroslow-Wang heuristics
- EVSIDS (Exponential VSIDS)

---

## Scope and Limitations
- GUMS is a **prototypical solver** designed for clarity and experimentation.
- Performance optimization was not the primary goal.
- Advanced heuristics and large-scale optimizations found in industrial sovlers are intentionally omitted.

---

## Usage

### Prerequisites
- **CMake** (3.31 or newer)
- A **C++20** compatible compiler (e.g., GCC, Clang, or MSVC)

### Building the project

GUMS uses CMake to provide a platform-independent build process.
To build the solver, execute the following commands from the project root directory:

```bash
# Create and enter the build directory
mkdir -p build && cd build

# Generate build files and compile
cmake ..
make

# Run
./cdcl_solver
```

---

## Examples

The `examples` directory contains three small SAT instances intended for testing and demonstration purposes.

- `4Queens.cnf` and `Sudoku1_ohne.cnf` originate from the author's Bachelor's thesis 
*"Vergleich von SAT-Solvern anhand ausgewählter Beispiele"* and were reused here
as representative, well-understood benchmark instances.
- The remaining example serves as a minimal test case for basic solver functionality.

These instances ar not intended for performance evaluation, but rather to illustrate
the behavior of the solver on small, structured problems.

## Licence

This project is released for educational and research purposes.
