cmake_minimum_required(VERSION 3.27)

project(OpenQuantTerminal
    VERSION 0.1.0
    DESCRIPTION "Self-hosted quantitative finance research terminal"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network Sql)

qt_standard_project_setup()

add_executable(open-quant-terminal
    src/main.cpp
    src/app/AppCompositionRoot.cpp
    src/app/AppCompositionRoot.h
    src/application/DataExplorerQueries.h
    src/application/CatalogQueries.h
    src/core/data/CatalogData.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/core/data/CanonicalData.cpp
    src/core/data/CanonicalData.h
    src/core/data/MarketData.cpp
    src/core/data/MarketData.h
    src/core/storage/SqliteMarketDataStore.cpp
    src/core/storage/SqliteMarketDataStore.h
    src/core/services/MarketDataIngestionService.cpp
    src/core/services/MarketDataIngestionService.h
    src/core/services/EconomicDataIngestionService.cpp
    src/core/services/EconomicDataIngestionService.h
    src/core/services/FredEconomicRefreshService.cpp
    src/core/services/FredEconomicRefreshService.h
    src/core/storage/SqliteCanonicalDatasetStore.cpp
    src/core/storage/SqliteCanonicalDatasetStore.h
    src/core/services/ProviderNormalizationService.cpp
    src/core/services/ProviderNormalizationService.h
    src/core/services/YahooMarketDataRefreshService.cpp
    src/core/services/YahooMarketDataRefreshService.h
    src/core/providers/ProviderResponse.cpp
    src/core/providers/ProviderResponse.h
    src/core/providers/ProviderAdapter.cpp
    src/core/providers/ProviderAdapter.h
    src/core/providers/FredAdapter.cpp
    src/core/providers/FredAdapter.h
    src/core/providers/YahooFinanceAdapter.cpp
    src/core/providers/YahooFinanceAdapter.h
    src/core/providers/YahooRequestBuilder.cpp
    src/core/providers/YahooRequestBuilder.h
    src/core/providers/YahooClient.cpp
    src/core/providers/YahooClient.h
    src/core/providers/YahooPipeline.cpp
    src/core/providers/YahooPipeline.h
    src/core/providers/FredRequestBuilder.cpp
    src/core/providers/FredRequestBuilder.h
    src/core/providers/FredClient.cpp
    src/core/providers/FredClient.h
    src/core/providers/FredPipeline.cpp
    src/core/providers/FredPipeline.h
)

target_link_libraries(open-quant-terminal
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
)

target_include_directories(open-quant-terminal
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

option(OQT_BUILD_TESTS "Build Open Quant Terminal tests" OFF)

if(OQT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

    add_executable(tst_main_window
        tests/tst_main_window.cpp
        src/app/MainWindow.cpp
        src/app/MainWindow.h
    )

    target_link_libraries(tst_main_window
        PRIVATE
            Qt6::Test
            Qt6::Widgets
    )

    target_include_directories(tst_main_window
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_main_window COMMAND tst_main_window)

    add_executable(tst_canonical_data
        tests/tst_canonical_data.cpp
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
    )

    target_link_libraries(tst_canonical_data
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_canonical_data
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_canonical_data COMMAND tst_canonical_data)

    add_executable(tst_market_data
        tests/tst_market_data.cpp
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
    )

    target_link_libraries(tst_market_data
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_market_data
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_market_data COMMAND tst_market_data)

    add_executable(tst_sqlite_market_data_store
        tests/tst_sqlite_market_data_store.cpp
        src/core/storage/SqliteMarketDataStore.cpp
        src/core/storage/SqliteMarketDataStore.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
    )

    target_link_libraries(tst_sqlite_market_data_store
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Sql
    )

    target_include_directories(tst_sqlite_market_data_store
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_sqlite_market_data_store COMMAND tst_sqlite_market_data_store)

    add_executable(tst_market_data_ingestion_service
        tests/tst_market_data_ingestion_service.cpp
        src/core/services/MarketDataIngestionService.cpp
        src/core/services/MarketDataIngestionService.h
        src/core/storage/SqliteMarketDataStore.cpp
        src/core/storage/SqliteMarketDataStore.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
    )

    target_link_libraries(tst_market_data_ingestion_service
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Sql
    )

    target_include_directories(tst_market_data_ingestion_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_market_data_ingestion_service COMMAND tst_market_data_ingestion_service)

    add_executable(tst_provider_normalization_service
        tests/tst_provider_normalization_service.cpp
        src/core/services/ProviderNormalizationService.cpp
        src/core/services/ProviderNormalizationService.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/providers/FredAdapter.cpp
        src/core/providers/FredAdapter.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_provider_normalization_service
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_provider_normalization_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_provider_normalization_service COMMAND tst_provider_normalization_service)

    add_executable(tst_provider_response
        tests/tst_provider_response.cpp
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
    )

    target_link_libraries(tst_provider_response
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_provider_response
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_provider_response COMMAND tst_provider_response)

    add_executable(tst_provider_adapter
        tests/tst_provider_adapter.cpp
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_provider_adapter
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_provider_adapter
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_provider_adapter COMMAND tst_provider_adapter)

    add_executable(tst_fred_adapter
        tests/tst_fred_adapter.cpp
        src/core/providers/FredAdapter.cpp
        src/core/providers/FredAdapter.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_fred_adapter
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_fred_adapter
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_fred_adapter COMMAND tst_fred_adapter)

    add_executable(tst_yahoo_finance_adapter
        tests/tst_yahoo_finance_adapter.cpp
        src/core/providers/YahooFinanceAdapter.cpp
        src/core/providers/YahooFinanceAdapter.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
    )

    target_link_libraries(tst_yahoo_finance_adapter
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_yahoo_finance_adapter
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_yahoo_finance_adapter COMMAND tst_yahoo_finance_adapter)

    add_executable(tst_fred_request_builder
        tests/tst_fred_request_builder.cpp
        src/core/providers/FredRequestBuilder.cpp
        src/core/providers/FredRequestBuilder.h
    )

    target_link_libraries(tst_fred_request_builder
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_fred_request_builder
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_fred_request_builder COMMAND tst_fred_request_builder)

    add_executable(tst_yahoo_request_builder
        tests/tst_yahoo_request_builder.cpp
        src/core/providers/YahooRequestBuilder.cpp
        src/core/providers/YahooRequestBuilder.h
    )

    target_link_libraries(tst_yahoo_request_builder
        PRIVATE
            Qt6::Test
            Qt6::Core
    )

    target_include_directories(tst_yahoo_request_builder
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_yahoo_request_builder COMMAND tst_yahoo_request_builder)

    add_executable(tst_yahoo_client
        tests/tst_yahoo_client.cpp
        src/core/providers/YahooClient.cpp
        src/core/providers/YahooClient.h
        src/core/providers/YahooRequestBuilder.cpp
        src/core/providers/YahooRequestBuilder.h
    )

    target_link_libraries(tst_yahoo_client
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Network
    )

    target_include_directories(tst_yahoo_client
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_yahoo_client COMMAND tst_yahoo_client)

    add_executable(tst_yahoo_pipeline
        tests/tst_yahoo_pipeline.cpp
        src/core/providers/YahooPipeline.cpp
        src/core/providers/YahooPipeline.h
        src/core/services/ProviderNormalizationService.cpp
        src/core/services/ProviderNormalizationService.h
        src/core/providers/YahooClient.cpp
        src/core/providers/YahooClient.h
        src/core/providers/YahooRequestBuilder.cpp
        src/core/providers/YahooRequestBuilder.h
        src/core/providers/YahooFinanceAdapter.cpp
        src/core/providers/YahooFinanceAdapter.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_yahoo_pipeline
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Network
    )

    target_include_directories(tst_yahoo_pipeline
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_yahoo_pipeline COMMAND tst_yahoo_pipeline)

    add_executable(tst_fred_client
        tests/tst_fred_client.cpp
        src/core/providers/FredClient.cpp
        src/core/providers/FredClient.h
        src/core/providers/FredRequestBuilder.cpp
        src/core/providers/FredRequestBuilder.h
    )

    target_link_libraries(tst_fred_client
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Network
    )

    target_include_directories(tst_fred_client
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_fred_client COMMAND tst_fred_client)

    add_executable(tst_fred_pipeline
        tests/tst_fred_pipeline.cpp
        src/core/providers/FredPipeline.cpp
        src/core/providers/FredPipeline.h
        src/core/services/ProviderNormalizationService.cpp
        src/core/services/ProviderNormalizationService.h
        src/core/providers/FredClient.cpp
        src/core/providers/FredClient.h
        src/core/providers/FredRequestBuilder.cpp
        src/core/providers/FredRequestBuilder.h
        src/core/providers/FredAdapter.cpp
        src/core/providers/FredAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_fred_pipeline
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Network
    )

    target_include_directories(tst_fred_pipeline
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_fred_pipeline COMMAND tst_fred_pipeline)

    add_executable(tst_yahoo_market_data_refresh_service
        tests/tst_yahoo_market_data_refresh_service.cpp
        src/core/services/YahooMarketDataRefreshService.cpp
        src/core/services/YahooMarketDataRefreshService.h
        src/core/services/MarketDataIngestionService.cpp
        src/core/services/MarketDataIngestionService.h
        src/core/storage/SqliteMarketDataStore.cpp
        src/core/storage/SqliteMarketDataStore.h
        src/core/providers/YahooPipeline.cpp
        src/core/providers/YahooPipeline.h
        src/core/services/ProviderNormalizationService.cpp
        src/core/services/ProviderNormalizationService.h
        src/core/providers/YahooClient.cpp
        src/core/providers/YahooClient.h
        src/core/providers/YahooRequestBuilder.cpp
        src/core/providers/YahooRequestBuilder.h
        src/core/providers/YahooFinanceAdapter.cpp
        src/core/providers/YahooFinanceAdapter.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_yahoo_market_data_refresh_service
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Network
            Qt6::Sql
    )

    target_include_directories(tst_yahoo_market_data_refresh_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_yahoo_market_data_refresh_service
             COMMAND tst_yahoo_market_data_refresh_service)

    add_executable(tst_economic_data_ingestion_service
        tests/tst_economic_data_ingestion_service.cpp
        src/core/services/EconomicDataIngestionService.cpp
        src/core/services/EconomicDataIngestionService.h
        src/core/storage/SqliteCanonicalDatasetStore.cpp
        src/core/storage/SqliteCanonicalDatasetStore.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_economic_data_ingestion_service
        PRIVATE
            Qt6::Test
            Qt6::Core
            Qt6::Sql
    )

    target_include_directories(tst_economic_data_ingestion_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_test(NAME tst_economic_data_ingestion_service
             COMMAND tst_economic_data_ingestion_service)

    add_executable(tst_fred_economic_refresh_service
        tests/tst_fred_economic_refresh_service.cpp
        src/core/services/FredEconomicRefreshService.cpp
        src/core/services/FredEconomicRefreshService.h
        src/core/services/EconomicDataIngestionService.cpp
        src/core/services/EconomicDataIngestionService.h
        src/core/storage/SqliteCanonicalDatasetStore.cpp
        src/core/storage/SqliteCanonicalDatasetStore.h
        src/core/providers/FredPipeline.cpp
        src/core/providers/FredPipeline.h
        src/core/providers/FredClient.cpp
        src/core/providers/FredClient.h
        src/core/providers/FredRequestBuilder.cpp
        src/core/providers/FredRequestBuilder.h
        src/core/providers/FredAdapter.cpp
        src/core/providers/FredAdapter.h
        src/core/services/ProviderNormalizationService.cpp
        src/core/services/ProviderNormalizationService.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )

    target_link_libraries(tst_fred_economic_refresh_service
        PRIVATE Qt6::Test Qt6::Core Qt6::Network Qt6::Sql
    )
    target_include_directories(tst_fred_economic_refresh_service
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME tst_fred_economic_refresh_service
             COMMAND tst_fred_economic_refresh_service)

    add_executable(tst_app_composition_root
        tests/tst_app_composition_root.cpp
        src/app/AppCompositionRoot.cpp
        src/app/AppCompositionRoot.h
        src/core/services/FredEconomicRefreshService.cpp
        src/core/services/FredEconomicRefreshService.h
        src/core/services/YahooMarketDataRefreshService.cpp
        src/core/services/YahooMarketDataRefreshService.h
        src/core/services/EconomicDataIngestionService.cpp
        src/core/services/EconomicDataIngestionService.h
        src/core/services/MarketDataIngestionService.cpp
        src/core/services/MarketDataIngestionService.h
        src/core/services/ProviderNormalizationService.cpp
        src/core/services/ProviderNormalizationService.h
        src/core/storage/SqliteCanonicalDatasetStore.cpp
        src/core/storage/SqliteCanonicalDatasetStore.h
        src/core/storage/SqliteMarketDataStore.cpp
        src/core/storage/SqliteMarketDataStore.h
        src/core/providers/FredPipeline.cpp
        src/core/providers/FredPipeline.h
        src/core/providers/FredClient.cpp
        src/core/providers/FredClient.h
        src/core/providers/FredRequestBuilder.cpp
        src/core/providers/FredRequestBuilder.h
        src/core/providers/FredAdapter.cpp
        src/core/providers/FredAdapter.h
        src/core/providers/YahooPipeline.cpp
        src/core/providers/YahooPipeline.h
        src/core/providers/YahooClient.cpp
        src/core/providers/YahooClient.h
        src/core/providers/YahooRequestBuilder.cpp
        src/core/providers/YahooRequestBuilder.h
        src/core/providers/YahooFinanceAdapter.cpp
        src/core/providers/YahooFinanceAdapter.h
        src/core/providers/ProviderAdapter.cpp
        src/core/providers/ProviderAdapter.h
        src/core/providers/ProviderResponse.cpp
        src/core/providers/ProviderResponse.h
        src/core/data/CanonicalData.cpp
        src/core/data/CanonicalData.h
        src/core/data/MarketData.cpp
        src/core/data/MarketData.h
    )
    target_link_libraries(tst_app_composition_root
        PRIVATE Qt6::Test Qt6::Core Qt6::Network Qt6::Sql
    )
    target_include_directories(tst_app_composition_root
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME tst_app_composition_root COMMAND tst_app_composition_root)

    set_tests_properties(tst_main_window PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
