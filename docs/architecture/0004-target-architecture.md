# OQT Target Architecture — Phase 2 Proposal

- Date: 2026-11-28
- Status: **Proposed — maintainer review required**
- Based on: OQT main `726cd6892638d6282b6c0aa135081056bde8f1e8`; Fincept reference revision `914134de5fd227fe66969c4591ac9ca7e461cdf4`.
- Scope: full-feature desktop terminal first. Lite Edition remains a later deployment/profile decision.

## 1. Architecture goals

1. Deliver broad quantitative-finance research capability without binding the product to one provider.
2. Keep UI, use cases, domain contracts, providers, persistence and analytics runtime independently testable.
3. Preserve provider raw response/provenance and explicit data-quality metadata.
4. Make slow/network/compute tasks asynchronous, cancellable and observable.
5. Keep local-first operation possible; external credentials are optional per provider.
6. Avoid copying Fincept code, assets, branding, private service integrations or data.
7. Keep full-feature architecture independent from future Lite resource profiles.

## 2. Current-state facts

- OQT uses C++20, Qt 6.8.x and CMake 3.27+.
- Current tree has `src/app`, `src/core/data`, `src/core/providers`, `src/core/services`, `src/core/storage`, and Qt Test targets.
- `AppCompositionRoot` owns FRED/Yahoo pipelines, refresh services, ingestion services and separate SQLite stores.
- Market storage supports `loadSeries` and `loadSeriesRange`; economic storage supports `loadDataset`. These are storage-level reads, not user-facing application query workflows.
- `MainWindow` is a welcome shell; there is no screen router, dashboard, charting module, feature navigation or user-facing refresh workflow.
- FRED and Yahoo are the only implemented external provider slices in the audited OQT baseline.
- No Python worker, broker execution, portfolio, backtesting, AI/agent or MCP feature modules are present in OQT's current tree.

## 3. Proposed logical layers and dependency direction

```text
Qt Desktop Shell / Feature Screens
              |
              v
Application Use Cases (commands, queries, DTOs, async task state)
              |
              v
Domain Contracts & Policies
              ^
              |
Provider Adapters / Storage Adapters / Analytics Workers
              ^
              |
Application Composition Root wires implementations to ports
```

Rules:
- Feature UI calls application-facing interfaces/use cases, never SQL stores or provider clients directly.
- Application use cases depend on domain contracts and ports, not concrete provider or SQLite classes.
- Provider adapters normalize into canonical contracts; provider-specific payloads stay outside feature UI.
- Storage adapters implement repository/query ports and do not own business rules.
- Composition root is the only place that selects concrete adapters and configures lifecycle.
- Keep modules in one desktop process by default. Introduce process boundaries only for untrusted, crash-prone, or resource-heavy runtimes.

## 4. Proposed logical modules

### 4.1 Desktop shell — `src/app`
Responsibilities:
- QApplication lifecycle, main window, navigation/screen routing, workspace layout and global status.
- Own application composition and expose feature entry points.
- Present module availability/degraded status.
- Do not perform provider requests, SQL queries or analytics in widgets.

Evolution: extend current `MainWindow` and `AppCompositionRoot`; do not replace them wholesale.

### 4.2 Feature modules — proposed `src/features/<domain>`
Initial candidates:
- `market_overview`, `market_explorer`, `economics`
- Later: `equity_research`, `screener`, `portfolio`, `derivatives`, `backtesting`, `news`, `workspace`.

Each feature owns screen composition, view state, user interaction and feature-specific presentation models. It consumes application use cases; it does not contain provider parsing or persistence SQL.

The first implementation may keep a small number of feature classes close to `src/app` until navigation and screen ownership are proven. Avoid creating empty folders for every audited Fincept screen.

### 4.3 Application layer — proposed `src/application`
Responsibilities:
- Commands: refresh market series, refresh economic dataset, create watchlist, import portfolio, run analysis.
- Queries: list available instruments/datasets, load time-series range, retrieve latest quote, retrieve portfolio snapshot.
- DTOs/read models shaped for workflows rather than database rows.
- Async task lifecycle: request ID, progress, cancellation, completion and structured failure.
- Application ports for provider selection, repositories, clock, secrets and analytics execution.

Start with read/query use cases for existing market/economic stores. This is the missing bridge between storage-level load methods and future UI screens.

### 4.4 Domain contracts — evolve `src/core/data`
Responsibilities:
- Instrument identity and symbol mapping.
- Canonical market bars, economic datasets/observations, provenance and quality.
- Later: financial statements, corporate actions, portfolio lots/transactions, derivative instruments, strategies, orders and risk measures.
- Explicit schema versions and validation rules.

Keep provider-specific IDs and fields in provider metadata, not as assumptions in canonical business logic. Avoid one universal record schema for OHLCV, macro series, news and trades.

### 4.5 Provider integrations — `src/core/providers`
Responsibilities:
- Provider-specific clients, request builders, pagination, rate limiting, retry/cancellation and response adapters.
- Provider capability metadata: supported instruments/operations/intervals, authentication, limits, attribution and terms.
- Registry and deterministic adapter contract tests.
- No credentials in logs, raw URLs, diagnostics or persisted request arguments.

Future provider families should be added by domain: market prices, economics, fundamentals, news/events, FX/rates, derivatives reference data, brokers and crypto. Add only after a provider/legal assessment.

### 4.6 Application/domain services — evolve `src/core/services`
Responsibilities:
- Provider-neutral ingestion, refresh orchestration, normalization, portfolio analytics, screening, risk, backtesting and news aggregation.
- Services should have narrow dependencies and explicit inputs/outputs.
- Do not turn this directory into a generic god-service; keep each use case independently testable.

As application workflows grow, new use-case orchestration belongs in `src/application`; existing provider/ingestion services can remain in `src/core/services` until a focused migration is justified.

### 4.7 Persistence — `src/core/storage`
Current:
- Separate SQLite market and economic databases, independent initialization/degraded mode, schema versioning, integrity and backup operations.

Target:
- Repository/query interfaces at the application boundary.
- SQLite for metadata, configuration, watchlists, portfolio ledgers, local datasets and moderate-size caches.
- Preserve market/economic store separation already implemented.
- Add a columnar/time-series backend (e.g. Parquet/DuckDB) only after measured OHLCV volume/query benchmarks justify it. Do not introduce TimescaleDB or a server database by default for a desktop-first product.
- Define retention, cache TTL, refresh policy, migrations, backup/restore and provenance before adding high-volume feeds.

### 4.8 Analytics runtime — future optional isolated worker
- Keep core deterministic calculations in C++ where appropriate.
- Use Python 3.11 for ecosystem-dependent statistical/ML packages only when a concrete feature needs them.
- Run long-running or third-party Python work in a separate worker process with a versioned JSON/IPC contract, timeout, cancellation, resource limits and structured results.
- UI and domain services must not block on Python execution.
- Do not embed a large model or require local GPU for baseline terminal operation.
- Start with classical statistics, vectorized numerical analytics and reproducible backtests; ML/LLM features remain optional.

### 4.9 Shared UI infrastructure — future `src/ui`
- Reusable chart, table, formatting, theme, notifications, navigation and workspace components.
- Keep charts fed by typed view models; chart widgets do not call providers.
- Build reusable components from actual first workflows, not a speculative component framework.

### 4.10 Security and operations
- Provider secrets stored through OS-backed secret storage when available; environment variables remain suitable for headless/test deployments.
- Redact secrets at request logging, error reporting and diagnostics boundaries.
- Read-only analytics by default. Live order placement, wallet signing, transfers and account mutation require a separate capability gate, explicit confirmation and audit trail.
- Add structured logging, provider health, cache age/staleness, task state and actionable errors.
- No account/cloud/billing dependency for local-first core workflows.

## 5. Canonical data flow

```text
Feature action
  -> Application command/query
  -> Provider capability selection
  -> Provider request (rate limit / timeout / retry / cancellation)
  -> Raw response envelope + redacted request metadata
  -> Versioned provider adapter
  -> Canonical contract + provenance + quality validation
  -> Provider-neutral ingestion
  -> Repository/storage transaction
  -> Application read model/query
  -> Feature view model
  -> Qt screen/chart/table
```

Requirements:
- Every persisted dataset records provider, operation, retrieval time, source/attribution, schema version and quality.
- Missing values remain missing/null, never silently coerced to zero.
- Partial/truncated responses remain explicitly marked and expose continuation metadata where supported.
- Repeated refreshes are idempotent for stable natural keys.
- Cache freshness and provider vintage/revisions are visible to downstream analytics.
- Query APIs support time range, instrument/dataset identity, interval/frequency and bounded result sizes.

## 6. First application workflows

### Workflow A — Market Explorer
- Search/select an instrument (initially a small curated symbol set; later instrument master).
- Choose provider and interval.
- Request historical OHLCV with bounded date range.
- Show data status, source, retrieval time and cache freshness.
- Display chart and table from the same query model.
- Refresh/cancel with progress and actionable errors.

### Workflow B — Economic Data Explorer
- Browse/search known FRED series initially.
- Show series metadata, units, frequency, geography and source.
- Load observations, show missing/partial quality and chart/table.
- Trigger FRED refresh only when key is configured.
- Later generalize to additional economic providers behind the same canonical dataset query.

These workflows exercise the full path from UI through application use cases, provider, normalization, persistence, read query and chart. They are a stronger first milestone than a static dashboard backed by hard-coded sample values.

## 7. Testing architecture

- Unit: domain validation, request builders, adapters, calculations and query policies.
- Contract: each provider adapter conforms to the shared canonical contract and redaction requirements.
- Storage integration: migrations, idempotency, range queries, backup/restore and recovery.
- Application integration: mock provider + temporary storage + use case result.
- UI: screen smoke tests and deterministic model/view-model tests; avoid network in widget tests.
- End-to-end: Market Explorer and Economic Explorer with mock server.
- Live integration: optional manual/scheduled tests with dedicated secrets; never required for ordinary CI.
- Performance: benchmark realistic bar counts, dataset size, query latency and memory before selecting heavier storage.

## 8. Proposed repository evolution

Do not mass-reorganize the current tree. Add boundaries incrementally:

```text
src/
  app/                  # existing bootstrap and shell
  application/          # use cases, ports, read models (new, incremental)
  core/
    data/               # existing canonical contracts; expand carefully
    providers/          # existing provider clients/adapters
    services/           # existing ingestion/refresh services
    storage/            # existing SQLite stores and repositories
  features/             # user-facing feature modules (new, incremental)
  ui/                   # shared presentation components (new, incremental)
  analytics/            # optional isolated worker contract (later)
tests/
  ...                   # focused unit, contract, integration, UI targets
docs/
  audit/
  architecture/
```

This is a logical target, not an instruction to create empty directories or move all current files. Introduce each directory only when its first real implementation lands.

## 9. Proposed decision register

| ID | Proposal | Status |
| --- | --- | --- |
| P-ARCH-01 | Modular monolith as initial deployment; avoid microservices | Proposed |
| P-ARCH-02 | UI depends on application use cases, not provider/storage classes | Proposed |
| P-ARCH-03 | Add application query/read-model boundary before broad UI work | Proposed |
| P-ARCH-04 | Preserve separate market/economic stores; defer columnar DB until measured | Proposed |
| P-ARCH-05 | Python analytics runs in an isolated optional worker when justified | Proposed |
| P-ARCH-06 | Read-only by default; live execution is separate and explicitly gated | Proposed |
| P-ARCH-07 | Full-feature baseline first; Lite is a resource profile after capability baseline | Proposed |

No proposal above is an accepted implementation decision until the maintainer approves it.

## 10. Open decisions for maintainer review

1. OQT license: choose a license compatible with the intended independent implementation and distribution plan. Do not copy Fincept code under the assumption that clean-room implementation alone resolves all legal questions.
2. Provider policy: approve providers one by one after authentication, rate limits, attribution and redistribution terms are documented.
3. Trading scope: confirm whether OQT's parity target includes paper trading only initially, with live execution deferred behind a separate explicit approval and security milestone.
4. Python runtime: approve the isolated worker only when the first Python-dependent feature is selected; do not add Python packages now.
5. UI workflow priority: recommended first vertical slice is Market Explorer + Economic Data Explorer.

## 11. Evidence and references

- [OQT main baseline](https://github.com/kaajis-kuy/open-quant-terminal/tree/726cd6892638d6282b6c0aa135081056bde8f1e8)
- [OQT AppCompositionRoot](https://github.com/kaajis-kuy/open-quant-terminal/blob/726cd6892638d6282b6c0aa135081056bde8f1e8/src/app/AppCompositionRoot.h)
- [OQT market storage query API](https://github.com/kaajis-kuy/open-quant-terminal/blob/726cd6892638d6282b6c0aa135081056bde8f1e8/src/core/storage/SqliteMarketDataStore.h)
- [OQT economic storage query API](https://github.com/kaajis-kuy/open-quant-terminal/blob/726cd6892638d6282b6c0aa135081056bde8f1e8/src/core/storage/SqliteCanonicalDatasetStore.h)
- [OQT current MainWindow](https://github.com/kaajis-kuy/open-quant-terminal/blob/726cd6892638d6282b6c0aa135081056bde8f1e8/src/app/MainWindow.cpp)
- [Fincept source tree at pinned revision](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src)
- [Fincept AGPL-3.0 license at pinned revision](https://github.com/Fincept-Corporation/FinceptTerminal/blob/914134de5fd227fe66969c4591ac9ca7e461cdf4/LICENSE)
