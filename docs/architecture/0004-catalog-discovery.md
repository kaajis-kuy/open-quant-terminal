# R1 catalog discovery contract

## Purpose
Expose persisted market-series and economic-dataset metadata as searchable application read models. Catalogs discover what is already stored; they do not trigger provider requests or infer availability from remote providers.

## Boundaries
- Market catalog reads only the market SQLite database.
- Economic catalog reads only the economic SQLite database.
- Application queries own validation, paging limits, and read-model mapping.
- UI consumes application query results only; it must not access SQL stores or provider clients.
- Catalog entries identify persisted series/datasets and provide enough metadata to select an existing explorer query.

## Market instrument catalog
One row per persisted `(provider_id, instrument_id, interval)`:
- provider ID, application instrument ID, provider symbol, interval, currency
- stored candle count
- latest stored candle timestamp
- source URL, retrieval timestamp, quality status, partial/truncated flags

Catalog SQL must aggregate from `market_series` and `market_bars`, not load full OHLCV arrays. Stable order: provider ID, instrument ID, interval. Search/filter inputs must be bound SQL parameters.

## Economic dataset catalog
One row per persisted `(provider_id, dataset_id)`:
- provider ID, dataset ID, schema version
- stored observation count
- earliest/latest observation period
- source URL, attribution ID, retrieval timestamp, quality status, partial/truncated flags

Catalog SQL must aggregate from `economic_datasets` and `economic_observations`, not load full observations. Stable order: provider ID, dataset ID. Search/filter inputs must be bound SQL parameters.

## Paging and safety
- Use SQL `LIMIT` / `OFFSET`, bounded page size 1..500.
- Return `hasMore` by fetching one extra catalog row.
- Reject negative offsets and overflow.
- Empty catalog is a successful empty result, distinct from database/query failure.
- Catalog data is read-only and sourced exclusively from local persistence.
- Do not expose API keys, raw provider payloads, request arguments, or private local filesystem paths.

## UI integration
- Replace manually typed provider/instrument/interval and provider/dataset identifiers with catalog-backed selectors.
- Selecting a market entry fills the existing Market Explorer query; selecting an economic entry fills Economic Data Explorer.
- Keep a clear empty-state message when no data has been persisted.
- Preserve the current explorer read-only behavior; refresh controls and charts remain separate R1 increments.

## Tests
- Empty catalog.
- Multiple providers, instruments, intervals, and datasets.
- Accurate counts and min/max timestamps/periods.
- Stable ordering and filtering.
- First/next/exhausted pages and invalid bounds.
- Composition-root degraded mode for each database.
- UI selector population and selection-to-query wiring.

## Implementation status
This document defines the next implementation contract only. No catalog query, selector, or UI behavior is claimed implemented by this document.
