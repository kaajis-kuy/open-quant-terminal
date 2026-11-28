# ADR 0003 — Application composition and degraded module startup

- Status: Implemented and verified.
- Date: 2026-11-28.

## Context

Provider pipelines, ingestion services, and SQLite stores existed as independently testable components, but the desktop entry point did not instantiate or connect them. FRED retrieval therefore had no application-level path into economic persistence.

## Decisions

1. `AppCompositionRoot` owns stores, pipelines, ingestion services, and refresh coordinators. `MainWindow` remains a UI layer and does not own provider or database lifecycle.
2. Persistent files live under `QStandardPaths::AppDataLocation` by default:
   - `market-data.sqlite` for canonical OHLCV data.
   - `economic-data.sqlite` for canonical economic datasets.
3. The two databases are opened independently. A failure disables only the affected module; the desktop shell and the other module remain available. Identical resolved database paths are rejected before opening either store.
4. FRED refresh orchestration follows the same provider-pipeline → canonical ingestion → SQLite boundary as Yahoo market refresh.
5. The FRED API key is read from `OQT_FRED_API_KEY` when a refresh is requested. It is passed in memory to `FredClient`; it is not written to persistence or application diagnostics.
6. Provider integration tests use a local mock HTTP server and temporary SQLite files. They do not require live provider credentials.

## Consequences

- Startup is resilient to an individual data-store failure.
- Market and economic schemas and database lifecycles remain independent.
- The UI can report module availability without owning infrastructure.
- The FRED key must be configured in the process environment before a refresh can start.

## Verification

The Session 97 implementation was verified with:
- Local X230 test run: 20/20 tests passed, 0 failures.
- GitHub Actions run [#282](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46122625087) on commit `b0884c2828a4a6f918dadeb03eb47d482c6f67ad`: Linux / Qt 6.8.3 configure, build, and test steps all succeeded.
- The FRED refresh integration test uses a local mock HTTP server and temporary SQLite storage; it does not require a live FRED API key.

This verification covers the Session 97 branch head above; it does not assert that a live external FRED request has been tested.
