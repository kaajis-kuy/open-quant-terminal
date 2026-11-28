# ADR 0002 — Economic dataset persistence

## Status
Proposed implementation on `feature/fred-economic-persistence`.

## Context
FRED observations are canonical economic time-series data. They are not OHLCV candles and should not be forced into the market-bar schema. Economic datasets also carry series identifiers, observation periods, units, frequency, geography, seasonal adjustment, measure type, currency metadata, provenance, and quality metadata.

## Decision
- Persist canonical economic datasets through `SqliteCanonicalDatasetStore`.
- Keep the economic store separate from `SqliteMarketDataStore`; use a dedicated database file such as `economic-data.sqlite`. Each store currently owns its SQLite schema version.
- Store dataset metadata and observations in one transaction.
- Upsert observations using their natural identity so repeated refreshes do not create duplicates or erase older periods.
- Store missing observation values as SQL NULL; do not coerce them to zero.
- Preserve source URL, attribution, retrieval timestamp, quality status, partial/truncated flags, missing-value counts, warnings, and pagination cursor.
- Optionally retain the provider's raw JSON response and redacted request arguments in a separate table.
- Keep the ingestion service provider-neutral; it accepts a valid `AdapterResult` containing a canonical dataset.

## Consequences
- FRED persistence is independently testable and does not depend on live credentials.
- Market OHLCV and economic observations have separate schemas and lifecycle boundaries.
- The application composition layer must open the economic database and inject its store into `EconomicDataIngestionService`.
- Future providers can reuse the same canonical economic dataset contract.

## Verification status
The implementation includes deterministic Qt tests for persistence, missing values, partial quality metadata, upsert idempotency, and reopening the database. A successful build/test run is still required before this ADR is accepted.
