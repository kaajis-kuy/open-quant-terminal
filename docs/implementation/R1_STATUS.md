# R1 implementation status

- Architecture decisions P-ARCH-01 through P-ARCH-07 were accepted by the maintainer on 2026-11-28.
- Current increment: application query/read-model boundary, initial read-only explorers, and SQL-backed pagination for both market and economic observations.
- Query services are composed from existing SQLite stores through AppCompositionRoot; widgets do not access concrete storage classes or provider clients directly.
- Market pagination queries newest timestamps first with SQL LIMIT/OFFSET and one extra row for hasMore, then restores chronological order within each page.
- Economic pagination queries the newest stable-key slice first with SQL LIMIT/OFFSET and one extra row for hasMore, then restores ascending order within each page.
- Existing unpaged store APIs remain available for backward compatibility.
- Page size is bounded to 1..5000; negative offsets and unsupported offset ranges are rejected.
- Tests now cover market/economic first page, subsequent page, exhausted page, invalid page parameters, and application read-model page metadata.

## Remaining R1 work

- Instrument and economic dataset catalog discovery.
- Chart views backed by the same paged query models as the tables.
- Refresh controls and observable task state: progress, cancellation, completion, and actionable errors.
- Broader mock-provider-to-storage-to-application integration tests.
- UI smoke/regression tests and final exact-head CI/review.

This is an implementation checkpoint, not an R1 completion claim. The pagination implementation still requires exact-head CI verification. Live provider behavior and data entitlements remain unverified. Pull-request merge requires separate maintainer approval.
