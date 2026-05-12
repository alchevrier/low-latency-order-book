#include <benchmark/benchmark.h>
#include <llob/order_book.hpp>
#include <llob/market_data_event.hpp>
#include <llob/pinned_thread.hpp>
#include <algorithm>
#include <vector>
#include <random> 
#include <cmath>
#include <pthread.h>

// TODO: matching latency benchmark — isolated then concurrent

// Computes the p-th percentile of v.
// ComputeStatistics passes a sorted-or-unsorted vector of per-repetition mean
// times (ns). We sort a copy and index into it.
static double Percentile(const std::vector<double>& v, double pct) {
    std::vector<double> sorted{v};
    std::sort(sorted.begin(), sorted.end());
    const std::size_t idx = static_cast<std::size_t>(
        std::ceil(pct / 100.0 * static_cast<double>(sorted.size()))) - 1;
    return sorted[std::min(idx, sorted.size() - 1)];
}

constexpr int N = 50;
constexpr int64_t MIN_PRICE = 90;
constexpr int64_t MAX_PRICE = 110;

static void BM_MatchingIsolated(benchmark::State& state) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    static llob::OrderBook<N> order_book;
    static bool initialised = [&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int64_t> dist(MIN_PRICE, MAX_PRICE);

        for (int i = 0; i < N; ++i) {
            llob::MarketDataEvent event{dist(rng), 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
            order_book.add(event);
        }

        return true;
    }();

    if (!initialised) {
        state.SkipWithError("OrderBook initialisation failed");
        return;
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(order_book.best_bid());
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * sizeof(int64_t));
}

static void BM_MatchingConcurrent(benchmark::State& state) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    llob::OrderBook<N> order_book;
    bool initialised = [&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int64_t> dist(MIN_PRICE, MAX_PRICE);

        for (int i = 0; i < N; ++i) {
            llob::MarketDataEvent event{dist(rng), 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
            order_book.add(event);
        }

        return true;
    }();

    if (!initialised) {
        state.SkipWithError("OrderBook initialisation failed");
        return;
    }

    std::atomic<bool> stop{false};
    llob::PinnedThread writer{1, [&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int64_t> dist(MIN_PRICE, MAX_PRICE);

        while (!stop.load(std::memory_order_relaxed)) {
            llob::MarketDataEvent event{dist(rng), 10, 1, 1, llob::Side::Bid, llob::EventType::Add};
            order_book.add(event);
        }
    }};

    for (auto _ : state) {
        benchmark::DoNotOptimize(order_book.best_bid());
    }

    stop.store(true, std::memory_order_relaxed);
    // PinnedThread destructor joins automatically
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * sizeof(int64_t));
}

// 10,000 repetitions x 100 iterations = 1,000,000 total appends
// 1M x 64B = 64MB — well within the 128MB segment capacity
// Each repetition reports the mean time for 100 iterations — approximates
// individual call latency. 10,000 data points make p99.99 meaningful (1 point).
BENCHMARK(BM_MatchingIsolated)
    ->Repetitions(10'000)
    ->Iterations(100)
    ->ComputeStatistics("p99",    [](const std::vector<double>& v) { return Percentile(v, 99.0);  })
    ->ComputeStatistics("p99.9",  [](const std::vector<double>& v) { return Percentile(v, 99.9);  })
    ->ComputeStatistics("p99.99", [](const std::vector<double>& v) { return Percentile(v, 99.99); })
    ->ComputeStatistics("p100",   [](const std::vector<double>& v) { return Percentile(v, 100.0); })
    ->ReportAggregatesOnly(true); // suppress 10,000 per-repetition lines

BENCHMARK(BM_MatchingConcurrent)
    ->Repetitions(1'000)
    ->Iterations(100)
    ->ComputeStatistics("p99",    [](const std::vector<double>& v) { return Percentile(v, 99.0);  })
    ->ComputeStatistics("p99.9",  [](const std::vector<double>& v) { return Percentile(v, 99.9);  })
    ->ComputeStatistics("p99.99", [](const std::vector<double>& v) { return Percentile(v, 99.99); })
    ->ComputeStatistics("p100",   [](const std::vector<double>& v) { return Percentile(v, 100.0); })
    ->ReportAggregatesOnly(true); // suppress 10,000 per-repetition lines

BENCHMARK_MAIN();
