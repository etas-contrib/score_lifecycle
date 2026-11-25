/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
/*
 * This process will act as a CPU load. This is useful for performance testing, where a realistic CPU load is required
 * to represent a real world calculation.
 *
 * An optional parameter can be passed to configure the maximum prime to calculate (--max-prime).
 * The process will continuously calculate primes until the timeout is reached (--timeout).
 * Primes are calculated using a memory-constrained sieve algorithm.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

#include <score/stop_token.hpp>

#include "tests/utils/test_helper/arg_parser.hpp"

// Define types for configurable parameters
using HelpType = bool;
using PrimeType = std::uint64_t;
using TimeoutType = std::uint32_t;
using IterationsType = std::uint64_t;

// Define default values
static constexpr PrimeType DEFAULT_MAX_PRIME = 1'000'000;
static constexpr TimeoutType DEFAULT_TIMEOUT_SECONDS = 0;
static constexpr TimeoutType DEFAULT_ITERATIONS = 0;
static constexpr std::size_t MEMORY_LIMIT = 1024 * 1024;

namespace score::mw::lifecycle::test
{
template <>
inline std::uint64_t ConvertArg<std::uint64_t>(const char* raw_value)
{
    return std::strtoull(raw_value, nullptr, 10);
}

template <>
inline std::uint32_t ConvertArg<std::uint32_t>(const char* raw_value)
{
    return static_cast<std::uint32_t>(std::strtoul(raw_value, nullptr, 10));
}
}  // namespace score::mw::lifecycle::test

namespace
{
/// @brief Program argument for help menu
struct HelpArg
{
    static constexpr std::string_view name = "--help";
    using type = HelpType;
};

/// @brief Program argument for maximum limit of prime calculation
struct MaxPrimeArg
{
    static constexpr std::string_view name = "--prime-max";
    using type = PrimeType;
    static constexpr type default_value = DEFAULT_MAX_PRIME;
};

/// @brief Program argument for timeout in seconds
struct TimeoutArg
{
    static constexpr std::string_view name = "--timeout";
    using type = TimeoutType;
    static constexpr type default_value = DEFAULT_TIMEOUT_SECONDS;
};

/// @brief Program argument for number of iterations
struct IterationsArg
{
    static constexpr std::string_view name = "--iterations";
    using type = IterationsType;
    static constexpr type default_value = DEFAULT_ITERATIONS;
};
}  // namespace

// Standard Sieve of Eratosthenes to find primes up to limit
void simpleSieve(int limit, std::vector<int>& primes, const score::cpp::stop_token& stop_token)
{
    std::vector<bool> isPrime(limit + 1, true);
    for (int p = 2; p * p <= limit; p++)
    {
        if (stop_token.stop_requested())
        {
            return;
        }
        if (isPrime[p])
        {
            for (int i = p * p; i <= limit; i += p)
            {
                isPrime[i] = false;
            }
        }
    }
    for (int p = 2; p <= limit; p++)
    {
        if (isPrime[p])
        {
            primes.push_back(p);
        }
    }
}

std::uint64_t calcPrimes(PrimeType n, const score::cpp::stop_token& stop_token)
{
    std::uint64_t number_of_primes = 0U;

    int limit = static_cast<int>(std::sqrt(n)) + 1;
    if (limit > static_cast<int>(MEMORY_LIMIT))
    {
        limit = MEMORY_LIMIT;
    }

    std::vector<int> primes;
    primes.reserve(limit);

    simpleSieve(limit, primes, stop_token);

    if (stop_token.stop_requested())
    {
        return 0;
    }

    long long low = limit + 1;
    long long high = 2 * limit;

    // Count primes found in the initial simple sieve
    number_of_primes += primes.size();

    // Process one segment of size 'limit' at a time
    while (low <= static_cast<long long>(n))
    {
        if (stop_token.stop_requested())
        {
            return 0;
        }

        if (high >= static_cast<long long>(n))
        {
            high = n;
        }

        // isPrime array for the current segment [low, high]
        // Index i in this array represents status of the number (low + i)
        std::vector<bool> isPrime(high - low + 1, true);

        // Mark multiples of primes found in simpleSieve
        for (size_t i = 0; i < primes.size(); i++)
        {
            if (stop_token.stop_requested())
            {
                return 0;
            }

            int p = primes[i];

            // Find the minimum number in [low, high] that is a multiple of p
            long long base = (low / p) * p;
            if (base < low)
            {
                base += p;
            }
            if (base == p)
            {
                // If the multiple is the prime itself, jump to the next multiple
                base += p;
            }

            // Mark all multiples of p in [low, high] as composite (false)
            for (long long j = base; j <= high; j += p)
            {
                isPrime[j - low] = false;
            }
        }

        // Count primes found in the current segment
        for (long long i = low; i <= high; i++)
        {
            if (isPrime[i - low])
            {
                number_of_primes++;
            }
        }

        // Slide the window to the next segment
        low = low + limit;
        high = high + limit;
    }

    return number_of_primes;
}

int main(int argc, char** argv)
{
    auto [help, max_prime, timeout_seconds, iterations] =
        score::mw::lifecycle::test::ParseArgs<HelpArg, MaxPrimeArg, TimeoutArg, IterationsArg>(argc, argv);

    if (help)
    {
        std::printf(
            "Usage: ./cpu_load_process [--help] [--prime-max <max_prime>] [--timeout <seconds> | "
            "--iterations <count>]\n");
        std::printf("  --help         Show this help message\n");
        std::printf(
            "  --prime-max    Maximum prime number to calculate (default: %llu)\n",
            static_cast<unsigned long long>(DEFAULT_MAX_PRIME));
        std::printf(
            "  --timeout      Timeout in seconds (default: %u if no iterations specified)\n", DEFAULT_TIMEOUT_SECONDS);
        std::printf("  --iterations   Number of iterations to run\n");
        std::printf("\nNote: Only one of --timeout or --iterations can be specified.\n");
        return 0;
    }

    // Validate that only one of timeout or iterations is specified
    const bool timeout_specified = timeout_seconds > 0;
    const bool iterations_specified = iterations > 0;

    if (timeout_specified && iterations_specified)
    {
        std::fprintf(stderr, "Error: Cannot specify both --timeout and --iterations\n");
        return 1;
    }

    // Default to timeout mode if neither specified
    if (!timeout_specified && !iterations_specified)
    {
        timeout_seconds = DEFAULT_TIMEOUT_SECONDS;
    }

    score::cpp::stop_source stop_source;
    score::cpp::stop_token stop_token = stop_source.get_token();

    std::thread timeout_thread;
    const bool use_timeout_mode = timeout_seconds > 0;

    if (use_timeout_mode)
    {
        std::cout << "Segmented Sieve of Eratosthenes (Max Prime = " << max_prime << ", Timeout = " << timeout_seconds
                  << "s)\n";

        // Start a thread that will request stop after the timeout
        timeout_thread = std::thread([&stop_source, timeout_seconds]() {
            std::this_thread::sleep_for(std::chrono::seconds(timeout_seconds));
            stop_source.request_stop();
        });
    }
    else
    {
        std::cout << "Segmented Sieve of Eratosthenes (Max Prime = " << max_prime << ", Iterations = " << iterations
                  << ")\n";
    }

    std::uint64_t last_successful_result = 0U;
    std::uint64_t iteration_count = 0U;

    // Continuously calculate primes until timeout or iteration limit
    while (!stop_token.stop_requested())
    {
        std::uint64_t result = calcPrimes(max_prime, stop_token);
        if (result > 0)
        {
            last_successful_result = result;
            iteration_count++;

            // Check if we've reached the iteration limit (iterations mode only)
            if (!use_timeout_mode && iteration_count >= iterations)
            {
                break;
            }
        }
    }

    if (timeout_thread.joinable())
    {
        timeout_thread.join();
    }

    std::cout << "Done! Completed " << iteration_count << " iterations. Last result: " << last_successful_result
              << " primes\n";
    return 0;
}
