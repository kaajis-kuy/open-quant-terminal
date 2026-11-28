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
- Exact-head CI for PR #9 head `4bdf863b36c5683e0849348b894d9b8eeae043ca` was confirmed by maintainer; this record does not independently assert a GitHub workflow run.

## Integration-test increment in progress

- Extended the Yahoo refresh-service integration test to verify the complete mock HTTP provider → Yahoo pipeline/normalization → ingestion → SQLite → application `MarketExplorerQuery` read-model path.
- Extended the FRED refresh-service integration test to verify the mock HTTP provider → FRED pipeline/normalization → ingestion → SQLite → application `EconomicDataExplorerQuery` read-model path.
- These tests use loopback mock HTTP servers and temporary SQLite databases; they do not call live Yahoo/FRED endpoints or require credentials.
- Existing Yahoo cancellation and provider-failure cases continue to assert that unsuccessful requests do not persist a dataset.
- Exact-head CI for PR #10 head `5300e58f16997c15dbd32114c3388289fe36e2a1` passed: Linux / Qt 6.8.3 Configure, Build, and Run tests all succeeded in run [#46136104316](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46136104316).

## FRED quality and failure coverage increment in progress

- Extended the FRED refresh mock server to allow deterministic HTTP status and payload selection.
- Added a partial-response round-trip test: FRED reports three total observations while returning two in the first allowed page; the test asserts Partial status, truncation metadata, persistence to SQLite, and preservation through the application economic read model.
- Added an HTTP 403 provider-failure test asserting the refresh fails and no economic dataset is persisted.
- Added a cancellation test asserting the refresh completes as failed/cancelled and no dataset is persisted.
- Tests use loopback mock HTTP and temporary SQLite only; no live FRED API or credentials are required.
- Exact-head CI for PR #11 head `6f9ff4ca758f8b99ba0b60ff90e8b2d4117eb9f6` passed: Linux / Qt 6.8.3 Configure, Build, and Run tests all succeeded in run [#46137432383](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46137432383).

## Degraded-mode isolation test increment in progress

- Strengthened both one-database-failure composition tests: the surviving database file must be created, its corresponding refresh service must remain available, and its catalog read query must execute successfully.
- The unavailable module must expose no refresh service, explorer query, or catalog query.
- Uses temporary directories and a directory at the blocked database-file path to trigger deterministic SQLite open failure without relying on OS permission behavior.
- No production code or schema changes; the increment verifies independent module startup and read availability.
- Exact-head CI for PR #12 head `3ebcb31b212ce6c53915ff2f90628a5bfb2d62a5` passed: Linux / Qt 6.8.3 Configure, Build, and Run tests all succeeded in run [#46138166744](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46138166744).

## Refresh UI regression increment in progress

- Added UI tests for market refresh success/persistence, provider failure with actionable error text, and economic refresh cancellation.
- Added coverage that market and economic refresh controls operate independently while both tasks are active.
- Added degraded-module UI coverage: when one application explorer query is unavailable, its corresponding refresh action is disabled while the other module's refresh action remains enabled.
- Fixed MainWindow refresh-action enablement to require both the refresh controller and the corresponding module query/read-model. This prevents offering a refresh action for a database module that failed startup in degraded mode.
- Tests use a fake RefreshTaskController and query fixtures; no live providers or credentials.
- Exact-head CI for PR #13 head `b6b38aef94582e2ba926e2e0048fe607974a8a15` had no workflow run at review time. This status update will create a new PR head and trigger the required CI.

## Remaining R1 work
- Verify the newly triggered exact-head CI for PR #13 and fix any failures.
- Add any remaining UI smoke/regression coverage identified by CI/review.
- Run final exact-head CI and review all stacked PRs; no merge without maintainer approval.

This is an implementation checkpoint, not an R1 completion claim. Pagination exact-head CI is green. Live provider behavior and data entitlements remain unverified. Pull-request merge requires separate maintainer approval.


## Refresh UI regression CI verification — 2026-11-28

- Corrected the workflow trigger by adding `test/**` to the `.github/workflows/ci.yml` push branch filter. The prior configuration included only `main` and `feature/**` for push, while pull_request targeted only `main`; PR #13 targets a feature branch and uses a test branch head, so neither event matched.
- The workflow correction is committed on the PR #13 branch. Exact-head CI for `7a8d75311519cee11a068c6f2d5a1c00615c8af7` passed in run [#46139410044](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46139410044): Configure, Build, and Run tests all succeeded.
- CTest result: 20/20 tests passed, 0 failed; total test time 7.00 seconds.
- This verifies the workflow-fix commit and the PR #13 code as of that SHA. A subsequent documentation-only commit will trigger another exact-head CI run; the latest-head status must be checked separately before declaring the PR's final verification complete.


## Latest-head CI verification — 2026-11-28

- PR #13 current head `687fa4b09be5adf69634960c9cd89e9933f93b45` completed exact-head GitHub Actions run [#46139791292](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46139791292) successfully.
- Linux / Qt 6.8.3: Configure PASS, Build PASS, Run tests PASS.
- CTest: 20/20 passed, 0 failed; total test time 7.11 seconds.
- This supersedes the earlier pending status for the documentation commit. The trigger fix and PR #13 code are verified on the latest PR head.
- Remaining R1 exit work is a final acceptance/review audit across the stacked PR series, checking current-head CI evidence and unresolved live-provider/licensing/runtime limitations. PRs remain unmerged; maintainer approval is required for any merge.


## R1 acceptance audit — 2026-11-28

### Stacked implementation verification

- Reviewed PR metadata and dependency bases for PR #5 through #13. The implementation stack is linear: #5 -> #6 -> #7 -> #8 -> #9 -> #10 -> #11 -> #12 -> #13. All remain open and unmerged; #5 through #13 are Draft.
- PR #13 head `868b10982692633a22ab7444776bb71842c382e3` contains the complete stacked implementation. Exact-head GitHub Actions run [#46155122522](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46155122522) passed Linux / Qt 6.8.3 setup, Configure, Build, and Run tests.
- Independently inspected the job results for exact-head runs: PR #6 [#46131502733](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46131502733), PR #7 [#46133422695](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46133422695), PR #8 [#46133949762](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46133949762), PR #10 [#46136104316](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46136104316), PR #11 [#46137432383](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46137432383), PR #12 [#46138166744](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46138166744), and PR #13 [#46155122522](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46155122522): each Linux / Qt 6.8.3 job completed successfully, including Configure, Build, and Run tests.
- PR #3 is a separate documentation/architecture PR targeting `main`; its exact-head run [#46128673864](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46128673864) passed. PR #5 also targets `main` and is not technically stacked on PR #3. Recommended eventual merge order is #3, then #5, then the dependent chain #6 through #13, subject to maintainer approval and fresh base/mergeability checks.
- PR #9's standalone exact-head workflow run was not independently located in the available run lookup. Its code is included in the tested PR #13 integrated head; do not represent a separate PR #9 run as independently verified.

### X230 verification

- User ran `ctest --test-dir build-x230 -R '^tst_app_composition_root$' --output-on-failure`; screenshot confirms 1/1 target passed, 0 failed.
- This includes both DB-failure isolation directions using temporary test directories. No real database was modified.
- Prior X230 smoke evidence remains: configure/build, full CTest 20/20, GUI launch, Yahoo AAPL/1d refresh and persistence, FRED fred.gdp refresh and persistence using locally configured key, and offline reload of both datasets.

### Scope limits / not yet acceptance claims

- A manual GUI run with one deliberately unavailable database has not been performed. Automated composition-root and UI tests cover degraded mode; this is not equivalent to manual GUI fault injection.
- This CI workflow validates Linux / Qt 6.8.3. X230 is Ubuntu 26.04 / Qt 6.10.2; the recorded local smoke test covers that target, but CI is not a matrix across all platforms/Qt versions.
- Live provider checks are limited to the recorded Yahoo AAPL and FRED GDP samples. Broader provider reliability, entitlement/redistribution rights, and third-party data licensing are not established by CI.
- R1 is not declared complete until the maintainer reviews the full stack, confirms merge order, and explicitly approves merges. No PR was merged in this audit.

### CI trigger update

- Workflow now runs on every push and every pull request target, plus manual `workflow_dispatch`. The trigger update is included in the passing PR #13 exact-head run above.


## R1 merge completion — 2026-11-28

- Maintainer explicitly approved integration. PR #3 and the full R1 implementation stack #5–#13 were merged into `main` in dependency order using merge commits.
- Merge commits: #3 `bc2fd30b557fb4ff67ec78021c2d215d20fee646`; #5 `49854d50f0dc7794b8ff83d9d99a1ffba208711c`; #6 `41e8f60799ab2a77309005eaa23521dd1136e340`; #7 `a2407bf543ae5713b2dbdef3c5d57473445d7999`; #8 `bfb1bf2454334835f592288e1deff9b4ea33c3d2`; #9 `06c320bd770a19d64f9bee9a5f832d2f47038ff6`; #10 `a05db267372799a0965519e170c075e759fe9868`; #11 `1c7d5c0182320cfd8e08fd245568e92ea26700ab`; #12 `27b8d95d44727208585816e995c02561a07c9bed`; #13 `bb526bcc6141460dfbd09119c8c2b729f5aebb50`.
- GitHub was queried after integration: all ten PRs (#3, #5–#13) report `merged=true` and `state=closed`. The final integrated main commit is `bb526bcc6141460dfbd09119c8c2b729f5aebb50`.
- PR #6–#13 were retargeted to `main` one at a time after their parent merged, preserving the stacked dependency order and allowing each incremental PR to be merged independently.
- Exact-head CI passed for the final PR #13 head before merge: run [#46161733221](https://github.com/kaajis-kuy/open-quant-terminal/actions/runs/46161733221), Linux / Qt 6.8.3 Configure, Build, and Run tests all succeeded. The post-merge main-branch CI run for final merge commit must still be checked separately.
- X230 evidence remains as recorded above: full 20/20 tests and the isolated AppCompositionRoot test passed, GUI launched, Yahoo/FRED sample refreshes persisted, and offline reload worked. Manual GUI fault injection with a deliberately unavailable DB was not performed.
- R1 code and architecture PRs are integrated into main. Remaining work is post-merge main CI verification, a fresh clone/build smoke check if desired, and the next R1/R2 feature acceptance scope. Do not equate merge completion with comprehensive provider, licensing, or all-platform validation.
