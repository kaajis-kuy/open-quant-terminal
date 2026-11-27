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

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

add_executable(open-quant-terminal
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/core/data/CanonicalData.cpp
    src/core/data/CanonicalData.h
    src/core/data/MarketData.cpp
    src/core/data/MarketData.h
    src/core/providers/ProviderResponse.cpp
    src/core/providers/ProviderResponse.h
    src/core/providers/ProviderAdapter.cpp
    src/core/providers/ProviderAdapter.h
    src/core/providers/FredAdapter.cpp
    src/core/providers/FredAdapter.h
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
    set_tests_properties(tst_main_window PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
