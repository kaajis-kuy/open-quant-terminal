# ADR 0002 — Economic dataset persistence

## Status
Implemented and verified on feature/fred-economic-persistence; awaiting pull-request merge.

## Context
FRED observations are canonical economic time-series data. They are not OHLCV candles and should not be forced into the market-bar schema. Economic datasets also carry series identifiers, observation periods, units, frequency, geography, seasonal adjustment, measure type, currency metadata, provenance, and quality metadata.

## Decision
- Persist canonical economic datasets through SqliteCanonicalDatasetStore.
- Keep the economic store separate from SqliteMarketDataStore; use a dedicated database file such as economic-data.sqlite. Each store currently owns its SQLite schema version.
- Store dataset metadata and observations in one transaction.
- Upsert observations using their natural identity so repeated refreshes do not create duplicates or erase older periods.
- Store missing observation values as SQL NULL; do not coerce them to zero.
- Preserve source URL, attribution, retrieval timestamp, quality status, partial/truncated flags, missing-value counts, warnings, and pagination cursor.
- Optionally retain the provider's raw JSON response and redacted request arguments in a separate table.
- Keep the ingestion service provider-neutral; it accepts a valid AdapterResult containing a canonical dataset.

## Consequences
- FRED persistence is independently testable and does not depend on live credentials.
- Market OHLCV and economic observations have separate schemas and lifecycle boundaries.
- The application composition layer opens the economic database and injects its store into EconomicDataIngestionService.
- Future providers can reuse the same canonical economic dataset contract.

## Verification
- Deterministic Qt tests cover persistence, missing values, partial quality metadata, upsert idempotency, database reopen/recovery, integrity checks, and backup/restore.
- Local X230 verification for the Session 97 implementation: 20/20 tests passed.
- GitHub Actions run [#286](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46124352594) completed successfully on implementation commit 6bc55592941f55052c2ca360910ed88a7b8f162d, targeting PR base main: Configure, Build, and Run tests all passed.
- The FRED refresh integration test uses a local mock HTTP server and temporary SQLite storage. It does not establish that a live external FRED API request has been tested.
