if(CRAB_BUILD_TESTS)
    # Find or fetch doctest
    find_package(doctest QUIET)
    if(NOT doctest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.11
        )
        FetchContent_MakeAvailable(doctest)
    endif()
    
    # Basic compilation test (no doctest needed)
    add_executable(crab_basic_test basic_test.cpp)
    target_link_libraries(crab_basic_test PRIVATE crab::crab)
    add_test(NAME CrabBasicTest COMMAND crab_basic_test)
endif()
