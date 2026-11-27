# Open Quant Terminal — Project Baseline

## Mission
Build a self-hosted quantitative-finance terminal inspired by the capabilities observed in Fincept Terminal. The initial milestone is a modular, full-feature implementation; hardware-specific optimization is a later phase.

This project is independent from the Jarvis trading-bot project.

## Reference baseline
- Upstream reference: https://github.com/Fincept-Corporation/FinceptTerminal
- Audited upstream revision: `914134de5fd227fe66969c4591ac9ca7e461cdf4`
- Primary audit focus: `fincept-qt`, especially Economics panels.
- Existing audit is static source inspection; it does not establish live provider availability, redistribution rights, runtime correctness, or build success.

## Engineering principles
1. Preserve provider-specific raw responses and provenance before normalization.
2. Normalize through explicit, versioned provider adapters and typed contracts.
3. Keep UI panels thin and domain logic testable independently.
4. Isolate external provider integrations from domain models and UI.
5. Inject clock, network, and provider responses for deterministic tests.
6. Never commit credentials, tokens, personal data, or machine-specific configuration.
7. Prefer small modules and clear boundaries over unnecessary agent orchestration.
8. Keep the full-feature implementation separate from any future Lite profile.

## Initial logical boundaries
- `app/`: application entry point and composition.
- `core/`: shared types, configuration, logging, time, and errors.
- `providers/`: provider clients, operation definitions, adapters, provenance.
- `domain/`: canonical observations, events, and analytical results.
- `features/`: user-facing feature modules and panel orchestration.
- `storage/`: cache, raw payload storage, and migrations.
- `tests/`: unit, contract, fixture, and integration tests.
- `docs/`: architecture decisions, provider contracts, and implementation plans.

These are logical boundaries, not a mandate to create empty directories. Confirm language, framework, and build requirements against the audited source before committing to implementation structure.

## Status
- [x] GitHub repository created.
- [x] Development branch created from `main`.
- [ ] Source tree and build system selected.
- [ ] Architecture decisions documented.
- [ ] First executable vertical slice implemented and tested.

## Working agreement
All development happens on feature branches. Do not merge into `main` without review. Back implementation claims with source inspection or executed tests; do not describe static findings as runtime verification.
