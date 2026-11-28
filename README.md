# Open Quant Terminal

A self-hosted quantitative-finance research terminal. The project is being developed as an independent implementation, using public provider specifications and a documented feature audit as references.

> **Status: foundation and provider-response contract implemented.** Linux CI with Qt 6.8.3 has successfully configured, built, and run the current Qt Test suite. This is not yet a functional finance terminal or live provider integration.

## Project goals

- Build a modular desktop terminal for economic, market, and quantitative research.
- Preserve raw provider payloads and provenance, then normalize data through versioned adapters.
- Keep UI, domain logic, provider integrations, and storage separated.
- Make core behavior testable without live API credentials or network access.
- Target a full-feature desktop implementation first; evaluate a constrained-hardware Lite profile later.

This repository is independent from the Jarvis trading-bot project.

## Technology direction

- C++20
- Qt 6.8.x
- CMake 3.27+
- Python 3.11 when an analytics/provider runtime is justified

The initial application currently depends only on Qt Widgets. Python, database, charting, and network modules will be added when a vertical slice requires them.

## Documentation

- [Project baseline](docs/PROJECT_BASELINE.md)
- [ADR 0001 — Technology stack and upstream reuse boundary](docs/architecture/0001-technology-stack-and-reuse-boundary.md)
- [Build and test instructions](docs/BUILDING.md)

## Current implementation

- [x] Minimal Qt application entry point.
- [x] Main window shell with welcome content and ready status.
- [x] Qt Test smoke cases for window identity, content, and status.
- [x] Compile and execute tests in Linux CI with Qt 6.8.3.
- [x] Provider response envelope decoder with deterministic tests.
- [ ] First provider-backed data vertical slice (fixture first, live integration later).

## Development workflow

- Work on feature branches; keep `main` stable.
- Add tests for pure request construction, parsing, adapters, and domain logic.
- Use deterministic fixtures for unit tests; do not require live provider APIs.
- Never commit API keys, tokens, personal data, or local machine configuration.
- Report static inspection, automated tests, live integration tests, and builds as separate evidence levels.

## Reference

The initial feature and behavior audit references [Fincept Terminal](https://github.com/Fincept-Corporation/FinceptTerminal) at source revision `914134de5fd227fe66969c4591ac9ca7e461cdf4`. The reference is not a claim of code reuse or product affiliation.

Review the upstream license and all applicable provider/data licenses before reusing code, assets, branding, or redistributing data. The upstream root license at the audited revision is AGPL-3.0.
