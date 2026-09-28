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
