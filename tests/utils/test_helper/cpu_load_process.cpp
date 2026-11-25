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
 * ./cpu_load_process --
 *
 * This process will act as a CPU load. This is useful for performance testing, where a realistic CPU load is required
 * to represent a real world calculation.
 *
 * Algorithms:
 * - Exit code 0: all `expected_count` posts were observed
 *   (or --succeed-on-timeout was set, and we timed out).
 * - Exit code 1: usage error.
 * - Exit code 2: timed out before `expected_count` posts were observed; the
 *   number of posts actually observed so far is printed to stderr.
 */

#include <charconv>
#include <fcntl.h>
#include <getopt.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <time.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <system_error>

using HelpType = bool;
using PrimeType = std::uint64_t;
static constexpr PrimeType DEFAULT_MAX_PRIME = 10000;

namespace
{
/// @brief Program argument for help menu
struct HelpArg
{
    static constexpr std::string_view name = "--help";
    using type = HelpType;
    static constexpr type default_value = false;
};

/// @brief Program argument for maximum limit of prime calculation
struct PrimeCalcMaxLimit
{
    static constexpr std::string_view name = "--prime-max";
    using type = PrimeType;
    static constexpr type default_value = 0U;
};
}  // namespace

int main(int argc, char** argv)
{
    HelpType help = false;
    PrimeType max_prime = DEFAULT_MAX_PRIME;
    auto [help, max_prime] = score::mw::lifecycle::test::ParseArgs<HelpArg, PrimeType>(argc, argv);

    if (help)
    {
        std::printf("Usage: ./pcu_load_process [--help] [--max-prime] <max_prime>\n");
        return 0;
    }

    calcPrime(max_prime)

        return 0;
}

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

// Standard Sieve of Eratosthenes to find primes up to sqrt(limit)
template <std::size_t MemoryLimit>
void simpleSieve(int limit, std::vector<int>& primes)
{
    std::vector<bool> isPrime(MemoryLimit + 1, true);
    for (int p = 2; p * p <= MemoryLimit; p++)
    {
        if (isPrime[p])
        {
            for (int i = p * p; i <= limit; i += p)
            {
                isPrime[i] = false;
            }
        }
    }
    for (int p = 2; p <= MemoryLimit; p++)
    {
        if (isPrime[p])
        {
            primes.push_back(p);
        }
    }
}

template <std::size_t MemoryLimit>
void calcPrime(long long n)
{
    std::vector<int> primes;
    primes.reserve(MemoryLimit);
    simpleSieve(MemoryLimit, primes);

    long long low = MemoryLimit;
    long long high = 2 * MemoryLimit;

    // Output primes found in the initial simple sieve
    std::cout << "Primes in initial segment [2, " << limit - 1 << "]:\n";
    for (int p : primes)
    {
        std::cout << p << " ";
    }
    std::cout << "\n\n";

    // Process one segment of size 'limit' at a time
    while (low < n)
    {
        if (high >= n)
        {
            high = n;
        }

        // isPrime array for the current segment [low, high]
        // Index i in this array represents status of the number (low + i)
        std::vector<bool> isPrime(high - low + 1, true);

        // Mark multiples of primes found in simpleSieve
        for (size_t i = 0; i < primes.size(); i++)
        {
            int p = primes[i];

            // Find the minimum number in [low, high] that is a multiple of p
            long long base = (low / p) * p;
            if (base < low)
            {
                base += p;
            }
            if (base == p)
            {  // If the multiple is the prime itself, jump to the next multiple
                base += p;
            }

            // Mark all multiples of p in [low, high] as composite (false)
            for (long long j = base; j <= high; j += p)
            {
                isPrime[j - low] = false;
            }
        }

        // Output primes found in the current segment
        std::cout << "Primes in segment [" << low << ", " << high << "]:\n";
        for (long long i = low; i <= high; i++)
        {
            if (isPrime[i - low])
            {
                std::cout << i << " ";
            }
        }
        std::cout << "\n\n";

        // Slide the window to the next segment
        low = low + limit;
        high = high + limit;
    }
}

int main()
{
    long long n = 100;  // Find and print primes up to 100
    std::cout << "--- Segmented Sieve of Eratosthenes (Limit = " << n << ") ---\n\n";
    segmentedSieve(n);
    return 0;
}
