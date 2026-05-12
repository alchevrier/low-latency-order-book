#include <gtest/gtest.h>
#include <llob/order_book.hpp>
#include <llob/market_data_event.hpp>

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