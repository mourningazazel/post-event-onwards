# Research spikes

Throwaway measurement code. **Not** part of the game codebase.

## gpu-compute-probe/

Checks that SDL_GPU creates a Vulkan device and runs a compute shader on this machine.

```sh
glslangValidator -V relax.comp -o relax.spv
gcc gpuprobe.c -o gpuprobe $(pkg-config --cflags --libs sdl3)
gcc computeprobe.c -o computeprobe $(pkg-config --cflags --libs sdl3)
./gpuprobe && ./computeprobe
```

Result on 2026-09-27 (Apple M1, Asahi, Mesa 26.2.3 Honeykrisp, SDL 3.4.16):

- Vulkan device created.
- 1,048,576-element compute dispatch plus readback: **0.9 ms**, results correct.

## scent-movement/

Blind scent-following mob movement with five processing-order variants (A–E).

```sh
g++ -std=c++20 -O2 -mcpu=native scentbench.cpp -o scentbench && ./scentbench
```

Results and interpretation are in `docs/design/scent-mobs.md`.

## parallel-fields/

The scent update as a dense pull (whole stage, active tiles, tiles on threads) and the Dead
reading a flow-direction grid, each checked bit for bit against `peo_core`.

```sh
cmake --preset headless-release && cmake --build --preset headless-release
g++ -std=c++20 -O3 -march=x86-64-v3 -I src/core/include \
    docs/research/spikes/parallel-fields/fieldbench.cpp \
    build/headless-release/src/core/libpeo_core.a -pthread -o fieldbench
./fieldbench [threads] [case-name filter]
```

Results and interpretation are in `docs/research/parallel-and-gpu.md`.
