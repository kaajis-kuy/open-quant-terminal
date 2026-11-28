# Open Quant Terminal

A self-hosted quantitative-finance research terminal. The project is being developed as an independent implementation, using public provider specifications and a documented feature audit as references.

> **Status: application composition and local provider-persistence foundations implemented; verification pending.** This is not yet a complete finance terminal. Provider refreshes are initiated by application services and require provider configuration; no live API is called automatically at startup.

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

The current vertical slice uses Qt Widgets, Network, and SQL. SQLite market and economic stores are separate and initialized independently.

## Documentation

- [Project baseline](docs/PROJECT_BASELINE.md)
- [ADR 0001 — Technology stack and upstream reuse boundary](docs/architecture/0001-technology-stack-and-reuse-boundary.md)
- [ADR 0002 — Economic dataset persistence](docs/architecture/0002-economic-dataset-persistence.md)
- [ADR 0003 — Application composition and degraded mode](docs/architecture/0003-application-composition-and-degraded-mode.md)
- [Build and test instructions](docs/BUILDING.md)

## Current implementation

- [x] Minimal Qt application entry point.
- [x] Main window shell with welcome content and ready status.
- [x] Qt Test smoke cases for window identity, content, and status.
- [x] Compile and execute tests in Linux CI with Qt 6.8.3.
- [x] Provider response envelope decoder with deterministic tests.
- [x] FRED canonical economic dataset SQLite store and provider-neutral ingestion service (build/test verification pending).
- [x] Yahoo OHLCV SQLite store and refresh service (build/test verification pending for the latest branch).
- [x] Application composition root with independent market/economic startup and degraded-mode status.
- [x] FRED refresh orchestration from provider pipeline through canonical SQLite persistence.
- [ ] Full build and integration-test verification on the updated branch.

## Runtime configuration

- Application data is stored under Qt `QStandardPaths::AppDataLocation`.
- Market OHLCV data uses `market-data.sqlite`; economic observations use `economic-data.sqlite`.
- Configure the FRED key as `OQT_FRED_API_KEY` in the process environment before requesting an economic refresh. The application reads it at refresh time and does not persist or log it.
- If one SQLite module fails to initialize, the other module remains available and the desktop shell continues in degraded mode.

## Development workflow

- Work on feature branches; keep `main` stable.
- Add tests for pure request construction, parsing, adapters, and domain logic.
- Use deterministic fixtures for unit tests; do not require live provider APIs.
- Never commit API keys, tokens, personal data, or local machine configuration.
- Report static inspection, automated tests, live integration tests, and builds as separate evidence levels.

## Reference

The initial feature and behavior audit references [Fincept Terminal](https://github.com/Fincept-Corporation/FinceptTerminal) at source revision `914134de5fd227fe66969c4591ac9ca7e461cdf4`. The reference is not a claim of code reuse or product affiliation.

Review the upstream license and all applicable provider/data licenses before reusing code, assets, branding, or redistributing data. The upstream root license at the audited revision is AGPL-3.0.
