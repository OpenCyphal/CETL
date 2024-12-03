#include <cetl/pmr/function.hpp>

#include <benchmark/benchmark.h>

namespace
{

void BM_CetlFn_call(benchmark::State& state)
{
    cetl::pmr::function<int64_t(int64_t), 16> fn = [](int64_t i) {
        benchmark::DoNotOptimize(i);
        return i;
    };
    for (auto _ : state)
    {
        int64_t    res = 0;
        const auto lim = state.range();
        for (int i = 0; i < lim; ++i)
        {
            res += fn(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

void BM_StdFn_call(benchmark::State& state)
{
    std::function<int64_t(int64_t)> fn = [](int64_t i) {
        benchmark::DoNotOptimize(i);
        return i;
    };
    for (auto _ : state)
    {
        int64_t    res = 0;
        const auto lim = state.range();
        for (int i = 0; i < lim; ++i)
        {
            res += fn(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

void BM_Lambda_call(benchmark::State& state)
{
    auto fn = [](int64_t i) {
        benchmark::DoNotOptimize(i);
        return i;
    };
    for (auto _ : state)
    {
        int64_t    res = 0;
        const auto lim = state.range();
        for (int i = 0; i < lim; ++i)
        {
            res += fn(i);
        }
        benchmark::DoNotOptimize(res);
    }
    (void) fn;
}

}  // namespace

BENCHMARK(BM_CetlFn_call)->Arg(1000);
BENCHMARK(BM_StdFn_call)->Arg(1000);
BENCHMARK(BM_Lambda_call)->Arg(1000);

BENCHMARK_MAIN();
