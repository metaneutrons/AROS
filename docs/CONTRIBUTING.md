# Contributing to AROS AArch64

## PR Workflow

When submitting fixes upstream:

1. Create fix branch from `master`
2. Open PR against **`metaneutrons/AROS` master** (our fork)
3. Wait for Copilot code review, address all findings
4. Only then open PR against **`aros-development-team/AROS` master** (upstream)

This catches bugs before they reach upstream reviewers.

## Branch Structure

- `aarch64` — main development branch (all features merged)
- `master` — synced with upstream
- `fix/*` — individual bugfix branches for upstream PRs

## Commit Messages

Use descriptive messages with component prefix:
```
component/subcomponent: Short description

Longer explanation if needed.
```

## Testing

- QEMU: See `docs/QEMU.md` for setup and known issues
- Hardware: RPi4, RPi5, CM5 (eMMC)
