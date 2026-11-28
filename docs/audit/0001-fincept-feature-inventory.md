# OQT Phase 1 — Fincept Terminal Feature & Module Inventory

- Audit date: 2026-11-28
- Reference repository: https://github.com/Fincept-Corporation/FinceptTerminal
- Pinned reference revision: `914134de5fd227fe66969c4591ac9ca7e461cdf4`
- Reference scope: `fincept-qt/src` directory inventory, selected screen/service/UI directory listings, pinned root README, pinned root LICENSE.
- OQT baseline: `main` at `726cd6892638d6282b6c0aa135081056bde8f1e8`
- Method: static source-tree inventory and targeted file/header inspection. This is not a runtime audit of every Fincept screen or a validation of every provider endpoint.

## 1. Audit interpretation and caveats

The reference tree contains 57 screen-domain directories under `fincept-qt/src/screens`, 55 service-domain directories under `fincept-qt/src/services`, plus shared application, core, datahub, network, Python, storage, trading, algorithm-engine, MCP, and UI infrastructure.

A directory or source file proves that a code area exists in the pinned tree; it does not by itself prove that every screen is complete, enabled in the free edition, backed by a currently working endpoint, or usable without a subscription/API key. The pinned README explicitly markets some capabilities (real-time data, live broker, backtest/algo execution, multi-agent research) as Enterprise features and describes Quantcept as a separate product. Treat product-tier availability and live provider entitlement as separate verification tasks.

The README's “800+ primary data sources” is a vendor statement, not a count independently verified in this audit.

## 2. Reference feature inventory

### A. Market overview and market data
Observed screen areas: `dashboard`, `markets`, `watchlist`, `asia_markets`, `screener`.
Observed service areas include `services/markets`, `services/instruments`, `services/sectors`, `services/connectivity`, `services/feeds`.
Observed shared UI includes chart, table, dashboard canvas/widget registry, navigation and workspace components.

Scope represented by the source tree:
- Market overview, quote/market pulse and dashboard widgets.
- Instrument/symbol lookup, market columns, watchlists and alerts.
- Time-series and candle chart components, tables and heatmaps.
- FX rates, market performance, search and composite risk service areas.
- Region/market-specific views.

### B. Economics and macro research
Observed screen: `screens/economics`, with panels for FRED, Federal Reserve, IMF, World Bank, OECD, ECB, BIS, BLS, BEA, Census, EIA, Eurostat, ONS, Statistics Canada, WTO, UN Comtrade, UNESCO, ILO, NBER, CFTC, fiscal data, central banks, Trading Economics, DBnomics/AkShare-related panels, macro models, and economic calendar.
Observed services: `services/economics`, `services/dbnomics`, `services/akshare`, `services/gov_data`, `services/fincept_data`.

Scope represented:
- Multi-provider macroeconomic series and dataset discovery.
- Economic calendar and macro events.
- Cross-country / regional statistical datasets.
- Macro analysis and table-oriented exploration.

Provider names here are source-tree observations, not confirmation of API terms, access, or live status.

### C. Equity research and screening
Observed screens: `equity_research`, `screener`, `company_snapshot` service.
Observed equity research source files include overview, analyst, data, financials/statements, ratios, valuation/DCF, technicals, earnings, dividends, filings, ownership, peers, options, news and sentiment tabs.

Scope represented:
- Company profile and financial statement research.
- Ratio, valuation and DCF analysis.
- Earnings, dividends, filings, ownership, peers and analyst coverage.
- Technical analysis, sentiment/news context and equity screening.

### D. Portfolio and performance
Observed screens: `portfolio`, `portfolio_monitor`.
Observed service area: `services/portfolio`.
Observed UI files include portfolio ledger, transaction panels, blotter, performance charts, heatmaps, correlation, allocation, risk/stress and insights views.

Scope represented:
- Portfolio holdings, transactions and ledger.
- Allocation, performance, attribution/metrics, correlation and risk.
- Import/export and portfolio monitoring.
- Dashboard widgets for portfolio summary and risk.

### E. Trading, brokers and execution
Observed screens: `equity_trading`, `crypto_trading`, `fno`, `algo_trading`, `action_center`.
Observed shared source areas: `src/trading` (broker interfaces/registry, accounts, exchange sessions, data streams, orders, risk gates, paper trading, smart order engine), `src/algo_engine` (strategy execution, indicators, backtesting, deployment, risk limits).
The pinned README advertises live broker and algorithmic execution among Enterprise capabilities.

Scope represented:
- Broker/exchange abstraction and account sessions.
- Quotes, order entry, order books, positions and account streams.
- Order validation, risk gates, approval and paper-trading support.
- Strategy builder, scanner, alerts, deployment and monitoring.
- Options/F&O chain, payoff and strategy construction.

Live trading is a high-risk, separately gated domain. Source presence is not authorization to connect a real account or send live orders.

### F. Derivatives and quantitative finance
Observed screens: `derivatives`, `fno`, `quantlib`, `surface_analytics`, `ma_analytics`.
Observed shared areas include `services/quantlib`, `services/options`, `algo_engine`, and chart components.

Scope represented:
- Derivative books, curves, credit and scenario views.
- Pricing, options chains, payoff and strategy analysis.
- Volatility/surface analytics.
- QuantLib-backed fixed-income/derivative calculations.
- M&A / transaction analysis.

### G. Backtesting and systematic research
Observed screen: `backtesting`.
Observed service: `services/backtesting`.
Observed algorithm engine: candle aggregation, indicators, condition evaluation, backtest engine, execution model, performance metrics, position manager, risk limits, scanner and deployment runner.

Scope represented:
- Backtest configuration, execution assumptions and costs.
- Results, performance, risk, trades, exposure and Monte Carlo views.
- Optimization and walk-forward analysis.
- Strategy scanning, deployment and monitoring.

### H. Crypto and alternative markets
Observed screens: `crypto_center`, `crypto_trading`, `polymarket`, `alt_investments`.
Observed services include `services/crypto`, `services/polymarket`, `services/prediction`, `services/wallet`.
Scope represented:
- Crypto market/trading views and wallet-related flows.
- Token/project market, treasury, staking and supply panels.
- Prediction-market views.
- Alternative investment research.

Any wallet signing, custody, transfers, or financial execution must be explicitly excluded from read-only analytics until a separate security design and approval process exists.

### I. News, events and geopolitical context
Observed screens: `news`, `calendars`, `geopolitics`, `maritime`, `relationship_map`.
Observed services: `services/news`, `services/geopolitics`, `services/maritime`, `services/feeds`, `services/relationship_map`.
Scope represented:
- News feeds, classification, clustering, sentiment, ticker tagging and analytics.
- Economic calendar and events.
- Geopolitical events and relationship mapping.
- Maritime/vessel information.

### J. AI, automation and extensibility
Observed screens: `ai_chat`, `ai_quant_lab`, `agent_config`, `chat_mode`, `mcp_servers`, `node_editor`, `code_editor`.
Observed service areas: `services/llm`, `services/agents`, `services/agent_tasks`, `services/coding_agents`, `services/workflow`, `services/harness`, `services/notebooks`; shared `src/mcp` implementation.
Scope represented:
- LLM provider/model routing and AI chat.
- Quantitative/ML modules and research assistants.
- Agent execution, workflows and automation.
- MCP client/server integration and tool schemas.
- Node-based and code-based workflows.

These features should be added only after deterministic data/query/application interfaces exist. Agent orchestration is not a prerequisite for a reliable terminal.

### K. Data connectivity and user workspace
Observed screens: `data_sources`, `data_mapping`, `excel`, `file_manager`, `notes`, `report_builder`, `devtools`, `docs`, `launchpad`, `settings`, `setup`, `profile`, `auth`, `recovery`, `support`, `about`, `info`.
Observed services: `services/connectors`, `services/data_sources`, `services/data_mapping`, `services/file_manager`, `services/report_builder`, `services/python_cli`, `services/updater`, `services/billing`, `services/cloud`, `services/auth`-related source areas.
Scope represented:
- External source/connector catalog, connection profiles and credential bridges.
- Data mapping, import/export and tabular workflows.
- Notes, files, reports, spreadsheets and notebooks.
- Application settings, setup, profile, authentication, recovery and updates.

Cloud, billing, hosted account and enterprise services are not assumed reproducible from the open-source client tree. Build independent local functionality around public protocols; do not copy private service integrations.

## 3. OQT feature parity matrix at main baseline

Status definitions:
- **Foundation implemented:** reusable data/provider/storage infrastructure exists, but not the complete user-facing capability.
- **Partial:** one narrow provider or backend slice exists.
- **Not started:** no corresponding OQT feature module/UI observed in the audited tree.
- **Policy/verification required:** source presence or external rights/entitlements remain unresolved.

| Reference domain | OQT current evidence | Status | Gap / next step |
| --- | --- | --- | --- |
| App shell | `MainWindow` welcome shell; composition root | Partial | Navigation, screen routing, workspace and real research screens |
| Market data | Canonical OHLCV, Yahoo adapter/pipeline, SQLite, refresh service | Foundation implemented | Instrument catalog, source registry, read/query use cases, UI, provider alternatives |
| Economics | Canonical datasets, FRED adapter/pipeline, SQLite, refresh service | Partial | Dataset catalog/search, multi-provider adapters, read/query use cases, UI and calendar |
| Charts/tables | No OQT chart/table feature modules observed | Not started | Reusable time-series/chart/table components |
| Watchlists/alerts | No OQT feature modules observed | Not started | Local watchlists, alerts and persistence |
| Equity research/fundamentals | No OQT feature modules observed | Not started | Company identity, fundamentals, statements, ratios, valuation, filings |
| Portfolio analytics | No OQT feature modules observed | Not started | Transactions, holdings, performance, allocation and risk |
| Derivatives/options | No OQT feature modules observed | Not started | Pricing, curves, options chains and scenario analytics |
| Backtesting | No OQT feature modules observed | Not started | Deterministic engine, cost/slippage model, metrics and experiment store |
| Strategy/algo tools | No OQT feature modules observed | Not started | Strategy DSL/model, scanner, paper deployment; live execution separately gated |
| Broker/exchange integration | No OQT broker/execution modules observed | Not started | Provider/broker capability matrix, sandbox/paper first |
| Crypto/alternative markets | No OQT feature modules observed | Not started | Read-only market data and provenance first |
| News/calendar/geopolitics | No OQT feature modules observed | Not started | Feed adapters, event model, deduplication and UI |
| AI/LLM/agents/MCP | No OQT feature modules observed | Not started | Optional, provider-neutral analytics/assistant interfaces |
| Data-source catalog/mapping | Generic provider adapter registry only | Foundation only | Connector registry, credential profiles, mapping and import/export |
| Python analytics/notebooks | No OQT runtime observed | Not started | Isolated worker protocol and controlled environment |
| Reports/files/notes/workspace | No OQT feature modules observed | Not started | Local workspace and export formats |
| Auth/cloud/billing | No OQT feature modules observed | Policy/verification required | Decide standalone local-first scope; do not assume private service parity |
| Packaging/update/telemetry | Not audited in depth | Unverified | Separate operational audit |

## 4. Provider, access and licensing boundary

- The pinned Fincept root README identifies the project as AGPL-3.0 and distinguishes public open-source software from separately marketed Enterprise capabilities.
- The source tree includes numerous named provider/service areas. Their existence does not validate endpoint access, API terms, pricing, redistribution rights, rate limits, or current operation.
- The reference README describes Quantcept separately from Fincept Terminal; treat it as an external product, not an OQT subsystem.
- OQT currently has FRED and Yahoo Finance integrations. FRED uses an environment-provided API key; Yahoo Finance is unofficial and must not be treated as a guaranteed production feed.
- Do not copy Fincept source files, assets, UI branding, private endpoints, credentials, or connector catalog contents. Continue independent implementation from public provider specifications and observed behavior.
- Before adding each provider, record its official docs, authentication, rate limits, attribution, storage/redistribution terms, commercial-use restrictions, and fallback strategy.
- OQT repository metadata at the audited baseline reports no declared license/root LICENSE. The maintainer must choose an OQT license before external distribution. This is a project decision, not inferred from the Fincept reference license.

## 5. Audit completion and remaining validation

**Phase 1 inventory outcome:** the broad screen/service/module taxonomy is inventoried and mapped to OQT's current baseline.

**Not claimed complete:** line-by-line behavioral audit of all 57 screen folders, every service implementation, all connector definitions, all providers, every upstream license, or free/enterprise feature gating. Those require scoped follow-up audits before implementing the relevant domains.

Recommended next audit slices:
1. Market data/instrument identity/charting and data-source contracts.
2. Economics provider catalog and dataset discovery.
3. Equity research/fundamentals and portfolio data contracts.
4. Derivatives/backtesting/strategy execution boundaries.
5. AI/Python/MCP and workspace capabilities.
6. Auth/cloud/enterprise gating, packaging and provider/legal terms.

## 6. Pinned source references

- [Fincept Terminal README at audited revision](https://github.com/Fincept-Corporation/FinceptTerminal/blob/914134de5fd227fe66969c4591ac9ca7e461cdf4/README.md)
- [Fincept AGPL-3.0 license at audited revision](https://github.com/Fincept-Corporation/FinceptTerminal/blob/914134de5fd227fe66969c4591ac9ca7e461cdf4/LICENSE)
- [fincept-qt source tree](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src)
- [Screens](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src/screens)
- [Services](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src/services)
- [Shared UI](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src/ui)
- [Trading and execution source](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src/trading)
- [Algorithm engine source](https://github.com/Fincept-Corporation/FinceptTerminal/tree/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/src/algo_engine)
- [OQT main baseline](https://github.com/kaajis-kuy/open-quant-terminal/tree/726cd6892638d6282b6c0aa135081056bde8f1e8)
