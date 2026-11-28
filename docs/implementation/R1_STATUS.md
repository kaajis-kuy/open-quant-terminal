# R1 implementation status

- Architecture decisions P-ARCH-01 through P-ARCH-07 were accepted by the maintainer on 2026-11-28.
- Current increment: application query/read-model boundary, initial read-only explorers, and SQL-backed pagination for both market and economic observations.
- Query services are composed from existing SQLite stores through AppCompositionRoot; widgets do not access concrete storage classes or provider clients directly.
- Market pagination queries newest timestamps first with SQL LIMIT/OFFSET and one extra row for hasMore, then restores chronological order within each page.
- Economic pagination queries the newest stable-key slice first with SQL LIMIT/OFFSET and one extra row for hasMore, then restores ascending order within each page.
- Existing unpaged store APIs remain available for backward compatibility.
- Page size is bounded to 1..5000; negative offsets and unsupported offset ranges are rejected.
- Tests cover market/economic first page, subsequent page, exhausted page, invalid page parameters, and application read-model page metadata.
- Exact-head CI for pagination fix commit `b399659d76114e1436550dff789267174621bcfd` passed: Configure, Build, and Run tests all succeeded in run [#46131502733](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46131502733).

## Next increment: persisted catalog discovery

- Contract documented in [0004-catalog-discovery.md](../architecture/0004-catalog-discovery.md).
- Implemented SQL-backed market instrument and economic dataset catalog queries using aggregate reads over persisted metadata/observations.
- Added catalog data models, bounded 1..500 paging, stable ordering, bound search parameters, and hasMore metadata.
- Added application catalog query boundaries and AppCompositionRoot wiring, preserving the UI/storage/provider separation.
- Added storage tests for counts, date ranges, search filtering, empty results and invalid page bounds; added composition/degraded-mode tests.
- Exact-head CI for PR #7 head `064e421894cafaf46a802770a58695c049a13ba1` passed in run [#46133422695](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46133422695).
- Catalog discovery is local-only and read-only; it makes no provider network requests.

## Chart views increment

- Added a lightweight custom Qt Widgets chart surface; no additional charting dependency is required.
- Market Explorer renders OHLC candlesticks and a volume panel from the market read model.
- Economic Explorer renders numeric observations as time-series lines grouped by series ID.
- Both charts consume the exact same application query results as their corresponding tables; no direct database/provider access was added to widgets.
- UI regression test checks that both chart surfaces are present.
- Exact-head CI for chart views passed on `90deb7c964768be4902843a4f7df2665892cfa18` in run [#46133949762](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46133949762).

## Refresh & task lifecycle increment

- Added Yahoo market and FRED economic refresh actions to their respective explorers through the application-facing `RefreshTaskController` interface implemented by `AppCompositionRoot`.
- Refresh operations remain asynchronous through the existing refresh services; UI does not invoke provider clients, refresh-service implementations, or storage directly.
- Added a shared task status label, indeterminate activity progress indicator, and cancellation control.
- Task lifecycle reports running, cancelling, completed-and-persisted, cancelled, and actionable failure states.
- Completion reloads the relevant explorer and persisted catalog page.
- FRED credentials continue to be read only inside AppCompositionRoot from `OQT_FRED_API_KEY`; the UI callback never receives or stores the secret.
- Yahoo provider symbols from catalog entries are kept separate from canonical instrument IDs so persisted refreshes remain keyed consistently.
- The current provider clients do not expose reliable byte/record progress events; the UI therefore uses an indeterminate progress indicator rather than inventing a percentage. More granular provider progress can be added when supported by the transport layer.
- Added UI coverage for idle task status, disabled cancellation, disabled unavailable refresh actions, and progress indicator reset.
- Exact-head CI for this increment is pending; do not claim verification until the workflow for the latest PR head passes.

## Integration-test increment in progress

- Extended the Yahoo refresh-service integration test to verify the complete mock HTTP provider → Yahoo pipeline/normalization → ingestion → SQLite → application `MarketExplorerQuery` read-model path.
- Extended the FRED refresh-service integration test to verify the mock HTTP provider → FRED pipeline/normalization → ingestion → SQLite → application `EconomicDataExplorerQuery` read-model path.
- These tests use loopback mock HTTP servers and temporary SQLite databases; they do not call live Yahoo/FRED endpoints or require credentials.
- Existing Yahoo cancellation and provider-failure cases continue to assert that unsuccessful requests do not persist a dataset.
- Exact-head CI for this test increment is pending.

## FRED quality and failure coverage increment in progress

- Extended the FRED refresh mock server to allow deterministic HTTP status and payload selection.
- Added a partial-response round-trip test: FRED reports three total observations while returning two in the first allowed page; the test asserts Partial status, truncation metadata, persistence to SQLite, and preservation through the application economic read model.
- Added an HTTP 403 provider-failure test asserting the refresh fails and no economic dataset is persisted.
- Added a cancellation test asserting the refresh completes as failed/cancelled and no dataset is persisted.
- Tests use loopback mock HTTP and temporary SQLite only; no live FRED API or credentials are required.
- Exact-head CI for this increment is pending.

## Degraded-mode isolation test increment in progress

- Strengthened both one-database-failure composition tests: the surviving database file must be created, its corresponding refresh service must remain available, and its catalog read query must execute successfully.
- The unavailable module must expose no refresh service, explorer query, or catalog query.
- Uses temporary directories and a directory at the blocked database-file path to trigger deterministic SQLite open failure without relying on OS permission behavior.
- No production code or schema changes; the increment verifies independent module startup and read availability.
- Exact-head CI is pending.

## Remaining R1 work
- Add broader application/composition integration and UI smoke/regression tests.
- Run final exact-head CI and review all stacked PRs; no merge without maintainer approval.

This is an implementation checkpoint, not an R1 completion claim. Pagination exact-head CI is green. Live provider behavior and data entitlements remain unverified. Pull-request merge requires separate maintainer approval.
