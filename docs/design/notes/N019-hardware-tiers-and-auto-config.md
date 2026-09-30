# N019 — Hardware tiers, horde ceilings and an auto-config

- Date: 2026-09-30
- Source: owner, in the Architect review thread

> Let do some theoretical benchmarks and then put some plans in place to test those. If this
> raises the testing suites limits by a substantial amount testing these things, let's talk about
> that limit and your thoughts on it. I want to consider that this will be run on modern pcs as
> well. I want to build target systems for certain sizes of hordes and maps. How much a rtx 1080
> on a i5 can versus a 4070s on an i7. I would say get the target systems from what is commonly
> out there now and just rough estimate the increased overhead. Keep in mind memory bandwidth and
> latency on the types of calls your doing and in what amount. I want to see how far we can get
> the horde size up to on a typical pc setup, even if it runs slower than required on my Mac.

> Probably only target low medium and high end builds outside my Mac

> One for each

> Also, plan on the future for an auto config button that will adjust horde size and map size
> based on pc specifics. Get the specifics from actual benchmarks testing different capabilities
> and horde sizes / active map sizes, adjust from there. Remember, this is a game on gradients,
> not pre set static configurations. If we can freely adjust those two we can better run the game
> on other systems. I think map size and horde count are two different hardware limitations to
> account for

Acted on in: [performance-targets.md](../performance-targets.md), decision D-034, queue PEO-070 to PEO-074.
