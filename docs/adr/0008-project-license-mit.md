# ADR-0008: Project license — MIT

- Status: Accepted
- Date: 2026-09-27

## Context

ADR-0004 committed the project to open source and permissive dependencies, but left the project's
own license open. The options considered were GPL-3.0-or-later, MPL-2.0, MIT and Apache-2.0.

## Decision

- The project's code is licensed under the **MIT License** (`LICENSE`).

## Consequences

- Anyone may reuse, modify and redistribute the code, including in closed-source products, as long
  as they keep the copyright notice.
- All current and candidate dependencies (MIT, BSD, Zlib, Apache-2.0 or MIT) are compatible.
- Binary releases must ship the dependencies' notices (`THIRD_PARTY_NOTICES`, per ADR-0004).
- Original art, fonts and text assets may be licensed separately when they exist; that is a
  future decision.
