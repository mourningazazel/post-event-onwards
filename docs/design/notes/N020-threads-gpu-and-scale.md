# N020 — Threads, the GPU, and going bigger

- Date: 2026-10-01
- Source: owner, in a project thread opened for the question; answered after reading
  [research/parallel-and-gpu.md](../../research/parallel-and-gpu.md)

> OK. I would like to talk over some concepts. When we discuss this, i want you to look at what is
> currently implemented, and look at the specific calculations and what they are doing / using
> from the PC, and then i want you to explore out of which major bottleneck calculations you are
> doing, what can be either offloaded into the gpu (if it doesn't give immediate performance with
> what we have now, what CAN it allow us to do. you know the intent behind the game, what could
> some level of gpu involvement allow us to accomplish?), as well as what can we put into multiple
> threads? The math of what we are doing with like the scent grid and path finding, i want to
> think of if there is any way to either do calculations in parallel across multiple threads,
> offload certain calculations into another thread to reduce latency on the original thread,
> allow a different approach to performance roadblocks like scent dispersion/buildup, or the path
> finding mechanics. It is a big, open ended question i encourage creativity with. What we have
> right now is a great base to start from with what we know is possible. What i want to know now,
> is can we take the same concepts and apply them to a scale much larger? While we don't need
> millions of dead just wandering around, if we can make the path finding or scent grid have
> impressive improvements, we can add harder to implement with performance, systems, and expand
> gameplay. Something to keep in mind is that what we have currently is a great system for
> performance, its deterministic, it can be built entirely from seeds, etc, but since we have so
> many objects, so many dead, such a big area, there might not always be systems we can
> accommodate with enough ease to allow any sort of serious slowdown from the game "running".
> Just future thinking here.

Answers to the research's three questions:

> Q1: B
> Q2: C, and here is why. The biggest we can do while utilizing hardware is the intent - if we
> can go bigger, do it. If we can make the norm 1024x1024 or even 2048x2048 grids, that helps
> keep a realistic flow of units across a city, without relying too much on random placement
> across a map border. If you have a 0.5km map grid, you will have 512 tiles along a border. Even
> if we split up the aggregates on the other side for "wandering in" zombies, it would end up a
> much more even distribution than if the map extended on down the street. When we do scents,
> we'd be able to enable longer reaches with the scents, that could travel across longer
> distances after buildup, rather than relying on aggregates to adjust spawn numbers. That fits
> the intent more if it is something we can obtain. If we can accomplish 1 out of 3 performance
> goals with the addition of GPU, i think that is a win. Also, keep in mind that players *will* be
> sleeping and players *will* be passing time quite frequently. Remember, going through drawers
> will be easy, so passing game time will need to be quick.
> Q3: C

Acted on in: decisions D-035, D-036, D-037; [ADR-0014](../../adr/0014-threads-and-gpu-compute-in-core.md);
queue PEO-078 to PEO-084.
