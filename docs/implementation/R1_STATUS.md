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
- Exact-head CI for chart views is pending; this increment is not verified until its workflow completes.

## Remaining R1 work
- Refresh controls and observable task state: progress, cancellation, completion, and actionable errors.
- Broader mock-provider-to-storage-to-application integration tests.
- UI smoke/regression tests and final exact-head CI/review.

This is an implementation checkpoint, not an R1 completion claim. Pagination exact-head CI is green. Live provider behavior and data entitlements remain unverified. Pull-request merge requires separate maintainer approval.
