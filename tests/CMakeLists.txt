add_executable(tests
    test_concepts.cpp
    test_spsc_queue.cpp
    test_mpsc_queue.cpp
    test_pinned_thread.cpp
    test_order_book.cpp
)
target_link_libraries(tests PRIVATE llob GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)
