# OQT Development Roadmap — Full-Feature Baseline

- Date: 2026-11-28
- Status: Architecture approved; R1 implementation in progress.
- Baseline: main `726cd6892638d6282b6c0aa135081056bde8f1e8`.
- Product rule: full-feature edition first; Lite Edition follows after the baseline is stable and measured.

## Prioritization principles

- Complete vertical slices, not isolated screens.
- Build on current FRED/Yahoo/data contracts without rewriting them unnecessarily.
- Every new feature requires provider/data rights review, tests and observable quality/provenance.
- Keep the core terminal useful without AI, cloud accounts, paid APIs or a GPU.
- Live execution and custody-like capabilities are separate high-risk milestones.
- Avoid speculative infrastructure; benchmark before adding a server database, plugin framework or agent orchestration.

## Roadmap

### R0 — Governance and documentation baseline
**Priority: P0 / prerequisite**
- Decide and publish OQT repository license.
- Establish provider/data licensing review checklist and provider registry metadata.
- Consider main branch protection with required CI checks and review.
- Correct stale `docs/BUILDING.md` and link current test targets.
- Record the final Phase 2 architecture decisions in ADRs after approval.

Exit criteria: license/policy decisions documented; workflow and docs match actual repository; no source-code implementation required for the policy decisions.

### R1 — Application query boundary and first research workflows
**Priority: P1 / IN PROGRESS — architecture approved 2026-11-28**
- Add application-level query/use-case boundary for existing market and economic read methods.
- Add instrument/dataset selection model and bounded query requests.
- Expose refresh service state, progress, cancellation and errors to application layer.
- Add a basic navigable shell and feature entry points.
- Implement Market Explorer (OHLCV chart + table + source/freshness/status).
- Implement Economic Data Explorer (series metadata + observations chart/table + quality state).
- Use mock HTTP and temporary SQLite for deterministic end-to-end tests.

Exit criteria:
- A user can select a supported symbol/series, load persisted data, refresh when configured, cancel a request, and see provenance/quality.
- UI never calls a provider client or SQLite store directly.
- Both workflows work without live API access in CI through fixtures/mock server.

### R2 — Shared research UI and market workspace
**Priority: P2**
- Reusable chart/table/time-range controls.
- Instrument search/catalog and provider capability display.
- Watchlists and local alerts.
- Dashboard layout and reusable tiles.
- Market overview, FX rates and market pulse as data coverage permits.

Exit criteria: user can create/manage watchlists and compose a simple market dashboard from persisted provider data.

### R3 — Expanded economics and macro research
**Priority: P2**
- Economic provider catalog and dataset search.
- Add providers one at a time (e.g. World Bank, IMF, OECD, ECB, national statistics) after terms and API contracts are checked.
- Economic calendar and event model.
- Cross-provider dataset identity, units, frequency, vintage/revisions and geography normalization.

Exit criteria: at least two independently implemented economic providers conform to canonical dataset contract, with provenance and data-quality tests.

### R4 — Equity research and screening
**Priority: P3**
- Instrument/company identity and symbol mapping.
- Fundamentals, statements, ratios, corporate actions, earnings and dividends.
- Valuation/DCF tools with transparent assumptions.
- Screener criteria and reproducible result snapshots.
- Company overview and peers/ownership/news only where reliable data sources exist.

Exit criteria: one end-to-end equity research workflow with source attribution, currency/unit handling and deterministic calculations.

### R5 — Portfolio analytics
**Priority: P3**
- Local portfolio ledger, positions/lots and transactions.
- Import/export with schema validation.
- Valuation, allocation, returns, drawdown, correlation and risk summaries.
- Corporate action and FX treatment documented.
- No broker credentials required for manual/imported portfolios.

Exit criteria: portfolio metrics reconcile to ledger inputs and are covered by deterministic test fixtures.

### R6 — Derivatives and fixed-income analytics
**Priority: P4**
- Instrument/contract definitions and curve data.
- Options chain and payoff analytics.
- Pricing/scenario/Greeks where model assumptions are explicit.
- QuantLib integration only after build/package/licensing compatibility review.
- Surface analytics and fixed-income/credit analytics.

Exit criteria: model calculations have known-answer tests, input validation and visible assumptions.

### R7 — Backtesting and strategy research
**Priority: P4**
- Event/time-series data alignment and survivorship/look-ahead safeguards.
- Strategy definition and deterministic simulation.
- Fees, spread, slippage, fills, corporate actions and position sizing.
- Performance/risk metrics, Monte Carlo and walk-forward/optimization.
- Paper-trading adapter and deployment monitoring after simulator validation.

Exit criteria: reproducible backtests from immutable input snapshots with no look-ahead leakage in tested cases.

### R8 — News, calendar and alternative intelligence
**Priority: P5**
- News/RSS ingestion, deduplication, classification and search.
- Event calendar and macro releases.
- Geopolitical, maritime and relationship mapping only with defensible data sources.
- Crypto/prediction/alternative asset views as optional provider modules.

Exit criteria: feed provenance, timestamp normalization, deduplication and stale-source behavior tested.

### R9 — Python analytics, notebooks and AI/automation
**Priority: P5 / optional capability**
- Isolated Python worker and versioned IPC.
- Statistical and econometric packages selected per use case.
- Notebook/report workflow and reproducible analysis artifacts.
- Optional LLM provider router, assistant tools and MCP integration.
- Agent workflows only after application APIs are stable and permissions are defined.

Exit criteria: worker crash/timeout/cancellation cannot crash UI; reproducible analytics results; AI features remain optional and cannot silently initiate trades.

### R10 — Advanced workspace, connectors and packaging
**Priority: P6**
- Data source catalog and connection profiles.
- Local files, notes, report builder, export and saved layouts.
- Plugin/connector extensibility if supported by real requirements.
- Packaging, upgrade/migration and backup workflows.
- Security review and telemetry/privacy controls.

Exit criteria: reproducible install/update, documented backups and secure credential lifecycle.

### R11 — Live execution (separate gated milestone)
**Priority: P6 / high risk**
- Only after paper trading, broker sandbox, order risk controls and account security are independently reviewed.
- Explicit live-mode switch, per-order confirmation policy, audit trail, kill switch and reconciliation.
- Provider/broker terms and regulatory implications reviewed.
- Never enabled by default and never required for the core research terminal.

Exit criteria: independent security and operational sign-off; paper/sandbox evidence; no automatic live trading enabled by installation.

### R12 — Lite Edition
**Priority: after full-feature baseline**
- Measure CPU, RAM, startup time, storage growth and query latency on ThinkPad X230.
- Define feature profiles and data-retention limits.
- Keep domain contracts and stored data compatible with full edition.
- Disable/substitute optional workloads, not fork core logic.

Exit criteria: measured resource targets and a documented compatibility matrix.

## First implementation slice recommendation

R1 is authorized. Deliver it in reviewable increments:
1. Define application query/use-case interfaces for existing market and economic storage. (in progress)
2. Define refresh task state/progress/cancel contract.
3. Add a minimal navigation shell. (initial explorer tabs implemented)
4. Deliver Market Explorer and Economic Data Explorer. (initial read-only tables implemented; refresh/chart/catalog work remains)
5. Add application integration tests and UI smoke tests. (initial query and UI tests added)

Do not start with AI agent orchestration, broker execution, a giant dashboard, a microservice split or a database migration to TimescaleDB.

## Dependencies and gating

- R0 must precede public distribution and new external data providers.
- R1 depends on current market/economic storage read methods and refresh services.
- R2 depends on R1's query/read models and chart data contract.
- R3 expands canonical economic data only after R1's dataset query contract is stable.
- R4/R5 depend on instrument identity and provider/data licensing.
- R6 depends on validated market/fundamental inputs.
- R7 depends on data integrity, point-in-time data and explicit execution assumptions.
- R9 depends on stable application APIs and a worker isolation contract.
- R11 depends on independent security review and is not part of the first feature parity milestone.
- R12 depends on a stable full-feature baseline and measured profiling.

## Definition of done per roadmap task

- Focused feature branch and small PR.
- Source audit before changes.
- Tests appropriate to the module, with no live credentials in ordinary CI.
- Build and test results on exact PR head.
- Final static review and explicit merge approval.
- Notion progress/tracker/audit/architecture/testing updated.
- No claim of feature parity until each acceptance criterion is demonstrated.


## Confirmed product objective — maximum practical feature parity

**Accepted on 2026-11-28:** OQT targets the maximum practical functional feature parity with Fincept Terminal across its broad product surface, not only quant trading. The roadmap therefore covers market/economic research, dashboards, screening/equity research, portfolio, derivatives, backtesting/algo trading, crypto/alternative markets, news/intelligence, AI/agents/MCP, and workspace/data tooling, subject to per-domain audits, lawful provider/data access, and technical feasibility.

R1 (Market Explorer + Economic Data Explorer) is the first foundation slice, not the final scope. Continue through the domain milestones and add follow-up work where the parity inventory reveals gaps. Full Feature Edition is the primary product and is built first; Lite Edition follows only after the full-feature baseline is stable and measured.

Feature parity is assessed by demonstrated user workflows and acceptance criteria, not by matching directory names or counting screens. Enterprise-only Fincept capabilities and third-party provider access must be treated as separate entitlement/legal verification items; OQT must implement its own lawful equivalents where possible.
