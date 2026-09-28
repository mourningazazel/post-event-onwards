# N013 — Bicycles, firearms, bites and armour, locks, no animals, no real-world words

- Date: 2026-09-28
- Source: owner, in session (answers to the content-backlog questions)

> I am fine with bicycles if you think you can do the travel well - I am thinking due to lack of
> ability to stop, a bicycle will travel you two steps at a time, so there is a chance you'll just
> run into something, maybe stopping takes 1 extra turn of 2 tiles. Firearms are realistically
> common, but zombies are killed by destroying the head or disabling entirely, so shooting them
> full of bullets just draws noise. Since there are enemies with sound mechanics within larger
> groups, shooting one off will cause that enemy to move your way (even if untriggered), moving
> through the crowd to find the source of the sound, but only triggering when they hear "human"
> noises, which we can figure out later. Pepper spray and guns do absoultely nothing, but it would
> be funny to include them anyways. Bites do not infect the player as this was a demonic zombie
> thing, so they can get damaged fine and have health mechanics. I think clothing should play a
> major piece in defense, as even a thick jacket will stop things like bites, and i think that
> damage mechanics should be body part specific, so if the damage roll says a bite on the neck,
> not wearing anything on the neck could be an issue, even if the roll is hard to succeed by the
> enemy. Locks should be only pickable by someone with a picking set, which might be hard to find
> as its just a random item that most people wouldnt have, but there should be other ways to gain
> entry, including just bashing off the doorknob, but know that some doors will have other locks
> that make entry less feasible with brute strength. Animals are all dead - i dont want to bother
> programming that in. Humans are all that are left, and that's an odd point in the story that all
> the animals are just randomly dead. No birds, squirrels, nothing. But i think you should be able
> to hear crows in the background of the game, yet never know where they are. Ambiance. You can
> make Dumpster just a Trash Bin. The idea that none of the common marketing/brand words match up
> to the world is a part of the immersion, as you aren't tying it back to your real world randomly
> on item finds.

## What this settles

- **Bicycles are rideable.** They carry the rider **2 tiles per turn**. Stopping takes **one
  extra turn (2 more tiles)**, and there is a real chance of running into things.
- **Firearms are realistically common.** Zombies die only from **head destruction** or **total
  disablement**; body shots mostly just make noise.
  - A gunshot moves **hearing-capable units** toward the source, even untriggered ones, pushing
    through crowds.
  - They become **triggered** only by **"human" noises**. The classification is to be designed
    later.
- **Pepper spray and stun guns do nothing** to the dead. They're kept anyway, for humour.
- **Bites don't infect** (resolves G04 D4.6: no zombification of the player through bites).
  Damage is **body-part specific**.
  - A hit roll picks a body part.
  - **Clothing coverage and armour on that part** decide the outcome: a thick jacket stops bites.
  - A hard-to-land neck bite on a **bare neck** is deadly.
- **Locks can be picked only with a lockpick set**, a rare item.
  - Other entry exists: **bash off the doorknob**, pry, break glass.
  - **Some doors carry extra locks** (deadbolts, security chains, bars) that make brute force much
    less feasible.
- **No animals, anywhere.** They're inexplicably all dead: no birds, no squirrels. Only
  **ambient crow calls** in the background, never locatable.
- **"Dumpster" becomes "trash bin"**, and more broadly **no real-world brand or marketing words**
  in the world. It's part of the immersion.

## Acted on in

- `content/registry`:
  - `feature.rideable`
  - `feature.lock` extra-lock params
  - actions `ride`, `bash_lock`
  - `pick_lock` requires a lockpick set
- `content/items`: bicycle rideable; `lockpick_set`; improvised lockpick capability removed; door
  lock kinds; pepper spray and stun gun descriptions; `dumpster` → `trash_bin`
- Tests: bicycle, lock, bite-coverage and entry chains, with negative controls
- `docs/design/scent-mobs.md` (gunshots and hearing units), `purposes.md`, G04 D4.6, content-backlog
