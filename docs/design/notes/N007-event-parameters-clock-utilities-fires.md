# N007 — Per-world event, the global clock, zombie decay, utilities, fires

- Date: 2026-09-27
- Source: owner, in session (answers to the six N005 follow-up questions)

> here are your answers to your follow up questions from earlier. I think it should be per world,
> that would be an interesting take on the mechanic. Along the same lines, i think "how many people
> turned" is a good point too, and we will present details of this in a beginning splash screen
> describing the event, including the time of day and how many people turned. I think it should be
> anywhere from 99% turned, to only 80% turned. And this would be reflected in the gameplay by the
> amount of "breakage" or scarcity of supplies, prevalence of barricades etc. You will find dead
> bodies littering the world, depending on how many people turned, with the lower amount of turned
> people producing more dead bodies via more survivors dying while the zombies survived. Your 2nd
> question. you should absoultely find items based off of how many people survived. I like the idea
> of notes and encampments you find, but also keep in mind that things like encampments would not
> only have a concentrated amount of useful supplies, but also be deserted for a reason - they were
> overran, or in rare scenarios, their owner died abroad and now their encampment is ripe for the
> taking. question 3. Yes i would say the zombies are different, but in specific ways. I think the
> damage they can inflict would be much the same, but regular run-of-the-mill zombies would be
> easier to damage and maim. I want to have limb/appendage-based damage mechanics, and depending on
> how long its been, gated by months and a global "days since event" timer, how easy it is to damage
> those specific parts. Helping the idea of a difficulty system tied to how long its been. Sooner
> after event, harder diffiuclty but increased rewards, longer out, lower difficulty but less
> rewards. 4. I think overgrowth needs to be realistic, but i think so does environmental damage. On
> generation of the terrain, keep the time ranges in mind, but also just tie it to the global timer,
> so if a player goes to a 'new' area and its been a year of gametime since the event, on a 1 week
> playthrough, it generates accordingly. you can just use that timer as the way to decide between
> the 3 modes, making it a natural mechanic and not a pre-programmed starting scenario. Please try
> to make it so its a gradient, so you could in theory plug in 5 years and the world would still
> generate and match accordingly. question 5., On week 1, matching a "neighborhood" mechanic of city
> generation, power may or may not be working for buildings. However, as the global time since the
> event goes on, teh chances of neighborhoods having power, reduces. Since i do not want the
> transition from city to wilderness being immediate, as the cities grow sparse, the chance of power
> being active grows even lower. Water shoudl have a much higher chance of working, and very sparse
> establishments should have a high chance of working regardless of time. (being well driven).
> question 6. I think fires should be a random event in world generation that affect either a single
> building or larger series of buildings. When a building is burned, the size of the burn area
> should determine the intensity of the burn damage. Severe burns would leave missing walls,
> (needing z level checks for structural consistency, collapsing contents from above z levels onto
> the ground floor), charred items that either work, dont work or barely work, etc, and depending on
> how long since the event, a scent of smoke that would cover up or modify existing scent tiles.

## What this settles

- **Per-world event parameters:**
  - time of day and day of the event
  - **turned fraction, 80–99%**

  Both are shown on an opening splash screen. A lower turned fraction means more survivors, which
  means more breakage, scarcity and barricades, and **more dead bodies** (survivors died, zombies
  didn't).
- **Survivor traces:**
  - Found items reflect how many survived.
  - **Notes.**
  - **Encampments:** concentrated useful supplies, but **deserted for a reason**. Usually overrun;
    rarely, the owner died elsewhere.
- **Zombies over time:**
  - They inflict much the same damage.
  - **Limb/appendage-based damage.** Body parts become easier to damage and maim as the global
    days-since-event rises (gated by months).
  - **Difficulty and reward follow time:** early is harder with richer rewards; late is easier
    with fewer.
- **One global clock, a gradient:** "days since event" drives all aftermath generation, including
  realistic overgrowth and environmental damage.
  - Starting stages are just starting clock values.
  - New areas generate at the **current** clock.
  - Any value (e.g. 5 years) must work.
- **Utilities:**
  - **Power is per neighbourhood.** Its probability falls with time **and** with sparseness, so
    there is no sudden city/wilderness break.
  - **Water** is much more likely to work.
  - **Very sparse properties** (wells) are likely to work regardless of time.
- **Fires** are world-generation events on one building or a cluster.
  - Intensity scales with the burned area.
  - Severe burns remove walls. That needs **structural checks across z-levels**, with upper
    floors and contents collapsing onto the floor below.
  - **Charred items** end up working, barely working or broken.
  - A **smoke scent**, depending on the time since the fire, covers or modifies other scents.

## Acted on in

- [ADR-0011](../../adr/0011-global-event-clock-and-world-event.md): global event clock and
  per-world event parameters; supersedes the fixed-stage part of ADR-0009
- [`world-generation.md`](../world-generation.md): "Event parameters, clock, utilities, fires
  (N007)"
- [`purposes.md`](../purposes.md): P-WO-11 to P-WO-17, P-EN-09
- Walkthrough G05 (bodies): limb damage confirmed as the direction
