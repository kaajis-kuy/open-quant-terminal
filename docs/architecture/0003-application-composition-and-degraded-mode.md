# ADR 0003 — Application composition and degraded module startup

- Status: Implemented; automated verification pending.
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

Added tests cover separate database initialization, same-path rejection, independent module failure, missing FRED key handling, and mock-server FRED-to-SQLite persistence. Build and execution are pending in an environment with Qt 6.8 development packages.
