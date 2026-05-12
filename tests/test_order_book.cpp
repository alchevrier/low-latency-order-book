#include <gtest/gtest.h>
#include <llob/order_book.hpp>
#include <llob/market_data_event.hpp>
#include <thread>
#include <immintrin.h>
#include <cstdlib> 
#include <ctime> 

TEST(OrderBook, SingleAddBidBestBid)
{
    llob::OrderBook<1> order_book;
    llob::MarketDataEvent event{100, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};

    order_book.add(event);

    ASSERT_EQ(order_book.best_bid(), 100);
}

TEST(OrderBook, MultipleBidBestBidIsHighest)
{
    llob::OrderBook<10> order_book;
    llob::MarketDataEvent event_1{100, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_2{99, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_3{101, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_4{102, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_5{98, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_6{97, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_7{108, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_8{94, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_9{100, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_10{99, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};

    order_book.add(event_1);
    order_book.add(event_2);
    order_book.add(event_3);
    order_book.add(event_4);
    order_book.add(event_5);
    order_book.add(event_6);
    order_book.add(event_7);
    order_book.add(event_8);
    order_book.add(event_9);
    order_book.add(event_10);

    ASSERT_EQ(order_book.best_bid(), 108);
}

TEST(OrderBook, CapacityFullWorstBidIsLowestReplaced)
{
    llob::OrderBook<1> order_book;
    llob::MarketDataEvent event_1{100, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_2{99, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
    llob::MarketDataEvent event_3{101, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};

    order_book.add(event_1);
    order_book.add(event_2);

    ASSERT_EQ(order_book.best_bid(), 100);

    order_book.add(event_3);

    ASSERT_EQ(order_book.best_bid(), 101);
}

TEST(OrderBook, SingleAddAskBestAsk) {
    llob::OrderBook<1> order_book;
    llob::MarketDataEvent event{100, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};

    order_book.add(event);

    ASSERT_EQ(order_book.best_ask(), 100);
}

TEST(OrderBook, MultipleAskBestAskIsLowest)
{
    llob::OrderBook<10> order_book;
    llob::MarketDataEvent event_1{100, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_2{99, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_3{101, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_4{102, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_5{98, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_6{97, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_7{108, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_8{94, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_9{100, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_10{99, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};

    order_book.add(event_1);
    order_book.add(event_2);
    order_book.add(event_3);
    order_book.add(event_4);
    order_book.add(event_5);
    order_book.add(event_6);
    order_book.add(event_7);
    order_book.add(event_8);
    order_book.add(event_9);
    order_book.add(event_10);

    ASSERT_EQ(order_book.best_ask(), 94);
}

TEST(OrderBook, CapacityFullWorstAskIsHighestReplaced)
{
    llob::OrderBook<1> order_book;
    llob::MarketDataEvent event_1{100, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_2{99, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};
    llob::MarketDataEvent event_3{101, 10, 1, 1, llob::Side::Ask, llob::EventType::Add};

    order_book.add(event_1);
    order_book.add(event_2);

    ASSERT_EQ(order_book.best_ask(), 99);

    order_book.add(event_3);

    ASSERT_EQ(order_book.best_ask(), 99);
}

TEST(OrderBook, AddAndBestBidInDifferentThread)
{
    constexpr int N = 1024;
    constexpr int64_t MIN_PRICE = 90;
    constexpr int64_t MAX_PRICE = 110;
    llob::OrderBook<50> order_book;
    std::atomic<bool> ready{false};

    std::srand(std::time(nullptr)); 

    std::thread producer([&order_book, &ready]() {
        for (int i = 0; i < N; ++i) {
            int64_t price = (std::rand() % (MAX_PRICE - MIN_PRICE + 1)) + MIN_PRICE;
            llob::MarketDataEvent event_1{price, 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
            order_book.add(event_1);

            ready.store(true, std::memory_order_release);
        }
    });

    std::thread consumer([&order_book, &ready]() {
        while (!ready.load(std::memory_order_acquire)) {
            _mm_pause();
        }
        for (int i = 0; i < N; i++) {
            auto best_bid = order_book.best_bid();
            ASSERT_TRUE(best_bid >= MIN_PRICE && best_bid <= MAX_PRICE);
        }
    });

    producer.join();
    consumer.join();
}