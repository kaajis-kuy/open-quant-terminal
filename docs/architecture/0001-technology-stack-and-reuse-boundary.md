# ADR 0001: Technology Stack and Upstream Reuse Boundary

- Status: Accepted for the first implementation milestone
- Date: 2026-11-27
- Decision owners: project maintainer and implementation engineer

## Context

The project aims to provide a self-hosted quantitative-finance terminal with feature coverage informed by a static audit of Fincept Terminal. The audited upstream revision is `914134de5fd227fe66969c4591ac9ca7e461cdf4`.

The upstream `fincept-qt` application is a native C++20 / Qt 6 desktop application. Its CMake configuration sets C++20, CMake 3.27+, Qt 6.8.3 as its reference version (with a configurable 6.8 minor pin), and an embedded Python analytics runtime. Its test guidance uses Qt Test and requires each test target to list only the source files it needs.

The upstream root LICENSE is GNU Affero General Public License v3 (AGPL-3.0). This is a material constraint when considering reuse, modification, distribution, or offering a modified version as a network service.

## Decisions

### 1. Initial technology direction

For the full-feature implementation, use the same broad technology family to preserve compatibility with the audited UI and integration patterns:

- C++20 for the desktop application and domain-facing services.
- Qt 6.8.x for the native UI, networking, charts, and local SQL integration.
- CMake 3.27+ as the build system.
- Python 3.11 as an optional analytics/provider runtime where justified by existing provider ecosystems.

This is a compatibility decision, not a claim that every upstream dependency or implementation detail must be copied. Exact Qt patch pinning and optional modules will be finalized when the minimal build target is specified.

### 2. Keep the implementation independently structured

Use the upstream repository as a behavioral and architectural reference. Do not copy source files, large code fragments, assets, branding, or proprietary service integrations into this repository as part of the initial clean implementation.

Implement independently from documented behavior, public provider specifications, and our own tests. If any upstream code is later considered for reuse, first record its exact origin, license, copyright notices, modifications, and distribution obligations; obtain legal review where needed.

### 3. Separate full and Lite profiles

The first milestone targets feature completeness on a capable desktop. A future Lite profile for constrained hardware must be a separately measured configuration that disables or substitutes expensive features without forking the domain contracts.

### 4. Testing seam

Use small, deterministic tests for pure request builders, provider adapters, canonical models, and parsing logic. Follow the upstream test principle of explicit source lists per test target; do not link the whole application into a unit test. Use fixtures and injected clocks/network responses instead of live API calls in unit tests.

### 5. Repository workflow

- Develop on feature branches.
- Keep `main` untouched until a reviewed pull request is ready.
- Never commit credentials or provider secrets.
- Distinguish static audit, fixture tests, live integration tests, and full application builds in status reports.

## Consequences

- The team can reuse the same C++/Qt/Python ecosystem and reduce translation risk when reproducing native behavior.
- The project avoids accidental source-level copying before license obligations are understood.
- The initial architecture remains modular and allows a later Lite profile.
- Build reproducibility depends on pinning the exact Qt modules, compiler, and Python packaging strategy in a subsequent build-system decision.

## Evidence

- Audited upstream CMake: https://github.com/Fincept-Corporation/FinceptTerminal/blob/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/CMakeLists.txt
- Audited upstream test conventions: https://github.com/Fincept-Corporation/FinceptTerminal/blob/914134de5fd227fe66969c4591ac9ca7e461cdf4/fincept-qt/tests/README.md
- Upstream root license: https://github.com/Fincept-Corporation/FinceptTerminal/blob/914134de5fd227fe66969c4591ac9ca7e461cdf4/LICENSE
- Upstream getting-started guide (current web reference; verify against pinned revision before treating as source baseline): https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/GETTING_STARTED.md
