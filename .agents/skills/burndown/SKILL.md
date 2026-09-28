---
name: burndown
description: Builder: work the queue in effort order S → M → L → XL until stopped, skipping items that need a decision.
---
# /burndown (Builder)

Same loop as `/start-work`, different order: all actionable `S` items first,
then `M`, `L`, `XL`. Skip anything `AwaitingUser` or without a brief. Stop
when the queue is empty, the user interrupts, or three items in a row bounce
back from review (then ask what is wrong before continuing).
