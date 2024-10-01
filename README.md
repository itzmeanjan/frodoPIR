> [!CAUTION]
> Following implementation of FrodoPIR private information retrieval scheme is designed from scratch, having zero third-party dependencies. It's not yet audited, avoid using it in production!

# frodoPIR
FrodoPIR: Simple, Scalable, Single-Server Private Information Retrieval

## Introduction

FrodoPIR is a very simple, stateful, single-server index-based *P*rivate *I*nformation *R*etrieval (PIR) scheme, built on top of *L*earning *W*ith *E*rror (LWE) problem, proposed in https://ia.cr/2022/981.

FrodoPIR protocol can be split into offline and online phases s.t. offline phase can solely be performed by the server, doesn't require any input from clients. As soon as public parameters become available from server, client can begin preprocessing queries, making them ready for quick future use. A simplified description of the protocol is given below. See figure 1 of https://ia.cr/2022/981 for more details.

- **Offline Phase**
  1) `server_setup`: Server samples pseudo-random matrix $A$, from seed $\mu$ and sets up database as matrix $D$, which has blowup factor of <3.5x, over the original database size. Server prepares public parameter $(\mu, M)$.
  2) `client_setup`: Client downloads public parameter $(\mu, M)$, setups up internal state.
  3) `client_prepare_query`: Client preprocesses a query by storing $(b, c)$ s.t. $b$ is a randomly distributed LWE sample vector.
- **Online Phase**
  1) `client_query`: When client is ready to make a query, it pops a pair of $(b, c)$ from it's internal cache and sends slightly mutated vector $b$, as query to the server.
  2) `server_respond`: Server responds to client's query, returning back response vector $\tilde{c}$.
  3) `client_process_response`: Client decodes server response, obtaining content of queried database row.

To present a more practical picture, imagine, we have a database with $2^{20}$ entries s.t. each entry is 256 -bytes, meaning database is of size 256 MB. We are setting up both server and client(s), on a `12th Gen Intel(R) Core(TM) i7-1260P` machine, running GNU/Linux kernel `Linux 6.8.0-45-generic x86_64` and this implementation of FrodoPIR is compiled with `GCC 14.0.1`, while also passing `-O3 -march=native -flto` compiler optimization flags.

| Step | Time Taken
:-- | --:
`server_setup` | 29.43 seconds
`client_setup` | 14.77 seconds
`client_preprocess_query` | 136.54 milliseconds
`client_query` | 449.73 microseconds
`server_respond` | 99.49 milliseconds
`client_process_response` | 628.83 microseconds

Here I'm maintaining a zero-dependency, header-only C++20 library implementation of FrodoPIR scheme, supporting all parameter sets, as suggested in table 5 of https://ia.cr/2022/981. Using this library is very easy, follow [here](#usage).

## Prerequisites

- A C++ compiler with support for compiling C++20 code.

```bash
$ g++ --version
g++ (Ubuntu 14-20240412-0ubuntu1) 14.0.1 20240412 (experimental) [master r14-9935-g67e1433a94f]
```

- System development utility programs such as `make` and `cmake`.
- For testing functional correctness of this PIR scheme, you need to globally install `google-test` library and its headers. Follow guide @ https://github.com/google/googletest/tree/main/googletest#standalone-cmake-project, if you don't have it installed.
- For benchmarking this PIR scheme, you must have `google-benchmark` header and library globally installed. I found guide @ https://github.com/google/benchmark#installation helpful.

> [!NOTE]
> If you are on a machine running GNU/Linux kernel and you want to obtain CPU cycle count for digital signature scheme routines, you should consider building `google-benchmark` library with libPFM support, following https://gist.github.com/itzmeanjan/05dc3e946f635d00c5e0b21aae6203a7, a step-by-step guide. Find more about libPFM @ https://perfmon2.sourceforge.net.

> [!TIP]
> Git submodule based dependencies will generally be imported automatically, but in case that doesn't work, you can manually initialize and update them by issuing `$ git submodule update --init` from inside the root of this repository.

## Testing

For ensuring functional correctness of this implementation of FrodoPIR scheme, issue

```bash
make -j                    # Run tests without any sort of sanitizers, with default C++ compiler.
CXX=clang++ make -j        # Switch to non-default compiler, by setting variable `CXX`.

make debug_asan_test -j    # Run tests with AddressSanitizer enabled, with `-O1`.
make release_asan_test -j  # Run tests with AddressSanitizer enabled, with `-O3 -march=native`.
make debug_ubsan_test -j   # Run tests with UndefinedBehaviourSanitizer enabled, with `-O1`.
make release_ubsan_test -j # Run tests with UndefinedBehaviourSanitizer enabled, with `-O3 -march=native`.
```

```bash
PASSED TESTS (4/4):
     162 ms: build/test/test.out FrodoPIR.MatrixOperations
    3256 ms: build/test/test.out FrodoPIR.ClientQueryCacheStateTransition
    7399 ms: build/test/test.out FrodoPIR.ParsingDatabaseAndSerializingDatabaseMatrix
   66962 ms: build/test/test.out FrodoPIR.PrivateInformationRetrieval
```

## Benchmarking

Benchmarking of all 6 algorithms of FrodoPIR scheme can be done, by issuing

```bash
# For switching to non-default compiler, set `CXX` variable, right before invoking following command.

make benchmark -j  # If you haven't built google-benchmark library with libPFM support.
make perf -j       # If you have built google-benchmark library with libPFM support.
```

> [!CAUTION]
> You must put all the CPU cores on **performance** mode before running benchmark program, follow guide @ https://github.com/google/benchmark/blob/main/docs/reducing_variance.md.

## On 12th Gen Intel(R) Core(TM) i7-1260P

Compiled with **gcc version 14.0.1 20240412**.

```bash
$ uname -srm
Linux 6.8.0-45-generic x86_64
```

> [!NOTE]
> I maintain following benchmark result table as JSON [here](./bench_result_on_Linux_6.8.0-45-generic_x86_64_with_g++_14.json) for ease of benchmark comparison with future iterations.

```bash
2024-09-29T23:13:04+04:00
Running ./build/benchmark/perf.out
Run on (16 X 503.267 MHz CPU s)
CPU Caches:
  L1 Data 48 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1280 KiB (x8)
  L3 Unified 18432 KiB (x1)
Load Average: 1.29, 0.63, 0.58
-----------------------------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                         Time             CPU   Iterations     CYCLES items_per_second
-----------------------------------------------------------------------------------------------------------------------------------------------
frodoPIR/client_setup/2^17/256B/process_time/real_time_mean                    1930 ms         1930 ms           10   6.17317G        0.51913/s
frodoPIR/client_setup/2^17/256B/process_time/real_time_median                  1958 ms         1958 ms           10   6.17297G       0.510684/s
frodoPIR/client_setup/2^17/256B/process_time/real_time_stddev                  92.0 ms         92.0 ms           10   5.22393M      0.0255016/s
frodoPIR/client_setup/2^17/256B/process_time/real_time_cv                      4.76 %          4.76 %            10      0.08%            4.91%
frodoPIR/client_setup/2^17/256B/process_time/real_time_min                     1770 ms         1770 ms           10    6.1659G       0.490406/s
frodoPIR/client_setup/2^17/256B/process_time/real_time_max                     2039 ms         2039 ms           10   6.18239G       0.564877/s
frodoPIR/client_process_response/2^16/256B/process_time/real_time_mean          553 us          601 us           10   76.5266k       1.81993k/s
frodoPIR/client_process_response/2^16/256B/process_time/real_time_median        571 us          616 us           10   76.6165k       1.75134k/s
frodoPIR/client_process_response/2^16/256B/process_time/real_time_stddev       42.8 us         47.0 us           10   7.20791k        149.431/s
frodoPIR/client_process_response/2^16/256B/process_time/real_time_cv           7.75 %          7.81 %            10      9.42%            8.21%
frodoPIR/client_process_response/2^16/256B/process_time/real_time_min           478 us          521 us           10   64.8534k       1.67098k/s
frodoPIR/client_process_response/2^16/256B/process_time/real_time_max           598 us          653 us           10   91.6069k       2.09249k/s
frodoPIR/server_respond/2^16/256B/process_time/real_time_mean                  5.00 ms         43.4 ms           10   75.2895M        200.219/s
frodoPIR/server_respond/2^16/256B/process_time/real_time_median                4.99 ms         43.2 ms           10   75.6288M        200.295/s
frodoPIR/server_respond/2^16/256B/process_time/real_time_stddev               0.103 ms        0.637 ms           10   1.35724M        4.03961/s
frodoPIR/server_respond/2^16/256B/process_time/real_time_cv                    2.05 %          1.47 %            10      1.80%            2.02%
frodoPIR/server_respond/2^16/256B/process_time/real_time_min                   4.88 ms         42.5 ms           10    73.225M        191.547/s
frodoPIR/server_respond/2^16/256B/process_time/real_time_max                   5.22 ms         44.7 ms           10   76.9216M        204.814/s
frodoPIR/client_query/2^18/256B/process_time/real_time_mean                     110 us          109 us           10   252.645k       9.05308k/s
frodoPIR/client_query/2^18/256B/process_time/real_time_median                   111 us          109 us           10   253.039k       9.03606k/s
frodoPIR/client_query/2^18/256B/process_time/real_time_stddev                 0.623 us        0.620 us           10   2.96678k        51.5048/s
frodoPIR/client_query/2^18/256B/process_time/real_time_cv                      0.56 %          0.57 %            10      1.17%            0.57%
frodoPIR/client_query/2^18/256B/process_time/real_time_min                      109 us          107 us           10   247.682k       9.00946k/s
frodoPIR/client_query/2^18/256B/process_time/real_time_max                      111 us          109 us           10    255.93k       9.18164k/s
frodoPIR/client_query/2^19/256B/process_time/real_time_mean                     216 us          214 us           10    507.34k       4.62894k/s
frodoPIR/client_query/2^19/256B/process_time/real_time_median                   216 us          214 us           10   510.256k       4.63521k/s
frodoPIR/client_query/2^19/256B/process_time/real_time_stddev                 0.889 us        0.842 us           10   8.17701k        18.9706/s
frodoPIR/client_query/2^19/256B/process_time/real_time_cv                      0.41 %          0.39 %            10      1.61%            0.41%
frodoPIR/client_query/2^19/256B/process_time/real_time_min                      215 us          213 us           10   492.017k       4.58799k/s
frodoPIR/client_query/2^19/256B/process_time/real_time_max                      218 us          216 us           10   517.432k       4.64842k/s
frodoPIR/client_process_response/2^19/256B/process_time/real_time_mean          571 us          855 us           10   133.069k       1.75321k/s
frodoPIR/client_process_response/2^19/256B/process_time/real_time_median        569 us          858 us           10   133.281k       1.75662k/s
frodoPIR/client_process_response/2^19/256B/process_time/real_time_stddev       10.6 us         20.7 us           10   2.51599k        32.3923/s
frodoPIR/client_process_response/2^19/256B/process_time/real_time_cv           1.87 %          2.42 %            10      1.89%            1.85%
frodoPIR/client_process_response/2^19/256B/process_time/real_time_min           557 us          819 us           10   129.047k       1.69035k/s
frodoPIR/client_process_response/2^19/256B/process_time/real_time_max           592 us          888 us           10   136.817k       1.79489k/s
frodoPIR/client_process_response/2^17/256B/process_time/real_time_mean          578 us          649 us           10   73.8788k       1.73523k/s
frodoPIR/client_process_response/2^17/256B/process_time/real_time_median        574 us          651 us           10    73.304k        1.7437k/s
frodoPIR/client_process_response/2^17/256B/process_time/real_time_stddev       28.1 us         33.1 us           10   3.50183k        84.8486/s
frodoPIR/client_process_response/2^17/256B/process_time/real_time_cv           4.87 %          5.11 %            10      4.74%            4.89%
frodoPIR/client_process_response/2^17/256B/process_time/real_time_min           530 us          592 us           10   68.7128k        1.6113k/s
frodoPIR/client_process_response/2^17/256B/process_time/real_time_max           621 us          696 us           10   78.9619k       1.88607k/s
frodoPIR/client_setup/2^16/256B/process_time/real_time_mean                     979 ms          979 ms           10   3.04952G        1.02328/s
frodoPIR/client_setup/2^16/256B/process_time/real_time_median                   989 ms          989 ms           10   3.04972G        1.01129/s
frodoPIR/client_setup/2^16/256B/process_time/real_time_stddev                  43.4 ms         43.4 ms           10   1.60967M      0.0470617/s
frodoPIR/client_setup/2^16/256B/process_time/real_time_cv                      4.43 %          4.43 %            10      0.05%            4.60%
frodoPIR/client_setup/2^16/256B/process_time/real_time_min                      903 ms          903 ms           10   3.04748G       0.974966/s
frodoPIR/client_setup/2^16/256B/process_time/real_time_max                     1026 ms         1026 ms           10   3.05232G        1.10723/s
frodoPIR/client_process_response/2^20/256B/process_time/real_time_mean          591 us          963 us           10   140.443k       1.69144k/s
frodoPIR/client_process_response/2^20/256B/process_time/real_time_median        589 us          949 us           10   140.594k        1.6975k/s
frodoPIR/client_process_response/2^20/256B/process_time/real_time_stddev       8.49 us         30.6 us           10   2.47158k        24.0694/s
frodoPIR/client_process_response/2^20/256B/process_time/real_time_cv           1.44 %          3.18 %            10      1.76%            1.42%
frodoPIR/client_process_response/2^20/256B/process_time/real_time_min           581 us          941 us           10   135.074k       1.64646k/s
frodoPIR/client_process_response/2^20/256B/process_time/real_time_max           607 us         1042 us           10   144.286k       1.72084k/s
frodoPIR/client_prepare_query/2^16/256B/process_time/real_time_mean            9.97 ms          118 ms           10   215.816M        100.293/s
frodoPIR/client_prepare_query/2^16/256B/process_time/real_time_median          10.0 ms          119 ms           10   216.415M        99.9284/s
frodoPIR/client_prepare_query/2^16/256B/process_time/real_time_stddev         0.139 ms         1.82 ms           10   3.52778M          1.422/s
frodoPIR/client_prepare_query/2^16/256B/process_time/real_time_cv              1.39 %          1.54 %            10      1.63%            1.42%
frodoPIR/client_prepare_query/2^16/256B/process_time/real_time_min             9.65 ms          115 ms           10   210.314M        99.0563/s
frodoPIR/client_prepare_query/2^16/256B/process_time/real_time_max             10.1 ms          120 ms           10    220.09M          103.6/s
frodoPIR/server_setup/2^18/256B/process_time/real_time_mean                    6.70 s          45.0 s            10   73.0524G       0.149915/s
frodoPIR/server_setup/2^18/256B/process_time/real_time_median                  6.68 s          44.8 s            10   72.3798G       0.149604/s
frodoPIR/server_setup/2^18/256B/process_time/real_time_stddev                 0.442 s          1.23 s            10   3.70898G       9.43851m/s
frodoPIR/server_setup/2^18/256B/process_time/real_time_cv                      6.60 %          2.72 %            10      5.08%            6.30%
frodoPIR/server_setup/2^18/256B/process_time/real_time_min                     6.06 s          43.5 s            10    70.097G       0.129646/s
frodoPIR/server_setup/2^18/256B/process_time/real_time_max                     7.71 s          48.0 s            10   83.0646G       0.165101/s
frodoPIR/client_setup/2^19/256B/process_time/real_time_mean                    7574 ms         7574 ms           10    24.381G        0.13204/s
frodoPIR/client_setup/2^19/256B/process_time/real_time_median                  7562 ms         7561 ms           10   24.4142G       0.132248/s
frodoPIR/client_setup/2^19/256B/process_time/real_time_stddev                  62.6 ms         62.8 ms           10   60.9751M       1.08313m/s
frodoPIR/client_setup/2^19/256B/process_time/real_time_cv                      0.83 %          0.83 %            10      0.25%            0.82%
frodoPIR/client_setup/2^19/256B/process_time/real_time_min                     7511 ms         7511 ms           10   24.2367G       0.129832/s
frodoPIR/client_setup/2^19/256B/process_time/real_time_max                     7702 ms         7702 ms           10    24.418G       0.133132/s
frodoPIR/server_setup/2^19/256B/process_time/real_time_mean                    14.2 s           104 s            10    160.32G      0.0705744/s
frodoPIR/server_setup/2^19/256B/process_time/real_time_median                  14.1 s           103 s            10   161.022G      0.0708027/s
frodoPIR/server_setup/2^19/256B/process_time/real_time_stddev                 0.364 s          2.64 s            10    7.7105G       1.79889m/s
frodoPIR/server_setup/2^19/256B/process_time/real_time_cv                      2.57 %          2.55 %            10      4.81%            2.55%
frodoPIR/server_setup/2^19/256B/process_time/real_time_min                     13.7 s           100 s            10    145.98G      0.0675568/s
frodoPIR/server_setup/2^19/256B/process_time/real_time_max                     14.8 s           107 s            10   173.936G      0.0727934/s
frodoPIR/server_setup/2^17/256B/process_time/real_time_mean                    3.39 s          22.2 s            10   35.6742G       0.294928/s
frodoPIR/server_setup/2^17/256B/process_time/real_time_median                  3.37 s          22.6 s            10   36.1179G       0.296538/s
frodoPIR/server_setup/2^17/256B/process_time/real_time_stddev                 0.105 s         0.882 s            10   1.86431G       9.16599m/s
frodoPIR/server_setup/2^17/256B/process_time/real_time_cv                      3.10 %          3.97 %            10      5.23%            3.11%
frodoPIR/server_setup/2^17/256B/process_time/real_time_min                     3.23 s          20.7 s            10   32.7358G       0.283906/s
frodoPIR/server_setup/2^17/256B/process_time/real_time_max                     3.52 s          23.4 s            10   38.2234G       0.310073/s
frodoPIR/client_query/2^20/256B/process_time/real_time_mean                     446 us          445 us           10   1.03874M       2.24172k/s
frodoPIR/client_query/2^20/256B/process_time/real_time_median                   448 us          446 us           10   1.03737M       2.23385k/s
frodoPIR/client_query/2^20/256B/process_time/real_time_stddev                  5.49 us         5.41 us           10   31.5635k        28.1559/s
frodoPIR/client_query/2^20/256B/process_time/real_time_cv                      1.23 %          1.22 %            10      3.04%            1.26%
frodoPIR/client_query/2^20/256B/process_time/real_time_min                      432 us          431 us           10   992.445k       2.21211k/s
frodoPIR/client_query/2^20/256B/process_time/real_time_max                      452 us          451 us           10   1.09696M       2.31463k/s
frodoPIR/client_process_response/2^18/256B/process_time/real_time_mean          598 us          779 us           10    94.559k       1.67424k/s
frodoPIR/client_process_response/2^18/256B/process_time/real_time_median        597 us          782 us           10   93.8358k       1.67499k/s
frodoPIR/client_process_response/2^18/256B/process_time/real_time_stddev       16.7 us         22.4 us           10   4.32568k        46.7285/s
frodoPIR/client_process_response/2^18/256B/process_time/real_time_cv           2.80 %          2.87 %            10      4.57%            2.79%
frodoPIR/client_process_response/2^18/256B/process_time/real_time_min           574 us          752 us           10   88.6416k       1.60747k/s
frodoPIR/client_process_response/2^18/256B/process_time/real_time_max           622 us          817 us           10   100.963k       1.74248k/s
frodoPIR/client_prepare_query/2^17/256B/process_time/real_time_mean            18.5 ms          240 ms           10   431.552M        54.1698/s
frodoPIR/client_prepare_query/2^17/256B/process_time/real_time_median          18.5 ms          241 ms           10   434.493M        54.2004/s
frodoPIR/client_prepare_query/2^17/256B/process_time/real_time_stddev         0.244 ms         5.26 ms           10   10.5934M       0.715381/s
frodoPIR/client_prepare_query/2^17/256B/process_time/real_time_cv              1.32 %          2.19 %            10      2.45%            1.32%
frodoPIR/client_prepare_query/2^17/256B/process_time/real_time_min             18.1 ms          229 ms           10   406.941M        53.2494/s
frodoPIR/client_prepare_query/2^17/256B/process_time/real_time_max             18.8 ms          246 ms           10   445.886M        55.2952/s
frodoPIR/server_setup/2^20/256B/process_time/real_time_mean                    28.5 s           213 s            10   305.091G      0.0351209/s
frodoPIR/server_setup/2^20/256B/process_time/real_time_median                  28.7 s           213 s            10   302.762G      0.0349043/s
frodoPIR/server_setup/2^20/256B/process_time/real_time_stddev                 0.609 s          5.93 s            10   11.0259G        769.93u/s
frodoPIR/server_setup/2^20/256B/process_time/real_time_cv                      2.14 %          2.78 %            10      3.61%            2.19%
frodoPIR/server_setup/2^20/256B/process_time/real_time_min                     27.1 s           206 s            10   290.981G      0.0342801/s
frodoPIR/server_setup/2^20/256B/process_time/real_time_max                     29.2 s           222 s            10   329.112G      0.0369177/s
frodoPIR/client_prepare_query/2^18/256B/process_time/real_time_mean            35.4 ms          486 ms           10   865.901M        28.2257/s
frodoPIR/client_prepare_query/2^18/256B/process_time/real_time_median          35.5 ms          488 ms           10   872.547M        28.1949/s
frodoPIR/client_prepare_query/2^18/256B/process_time/real_time_stddev         0.619 ms         7.17 ms           10   18.1316M       0.493974/s
frodoPIR/client_prepare_query/2^18/256B/process_time/real_time_cv              1.75 %          1.48 %            10      2.09%            1.75%
frodoPIR/client_prepare_query/2^18/256B/process_time/real_time_min             34.5 ms          475 ms           10   824.539M        27.4146/s
frodoPIR/client_prepare_query/2^18/256B/process_time/real_time_max             36.5 ms          493 ms           10   881.946M        28.9776/s
frodoPIR/client_setup/2^18/256B/process_time/real_time_mean                    3771 ms         3771 ms           10   12.3669G       0.265895/s
frodoPIR/client_setup/2^18/256B/process_time/real_time_median                  3833 ms         3833 ms           10   12.3697G       0.260869/s
frodoPIR/client_setup/2^18/256B/process_time/real_time_stddev                   208 ms          208 ms           10   13.9428M      0.0147502/s
frodoPIR/client_setup/2^18/256B/process_time/real_time_cv                      5.52 %          5.52 %            10      0.11%            5.55%
frodoPIR/client_setup/2^18/256B/process_time/real_time_min                     3521 ms         3521 ms           10   12.3481G       0.246642/s
frodoPIR/client_setup/2^18/256B/process_time/real_time_max                     4054 ms         4054 ms           10   12.3853G       0.284029/s
frodoPIR/client_prepare_query/2^20/256B/process_time/real_time_mean             138 ms         1965 ms           10   3.46642G        7.22879/s
frodoPIR/client_prepare_query/2^20/256B/process_time/real_time_median           139 ms         1976 ms           10    3.4696G        7.19446/s
frodoPIR/client_prepare_query/2^20/256B/process_time/real_time_stddev          1.42 ms         21.1 ms           10   35.2454M      0.0743133/s
frodoPIR/client_prepare_query/2^20/256B/process_time/real_time_cv              1.02 %          1.07 %            10      1.02%            1.03%
frodoPIR/client_prepare_query/2^20/256B/process_time/real_time_min              136 ms         1929 ms           10   3.39759G        7.14961/s
frodoPIR/client_prepare_query/2^20/256B/process_time/real_time_max              140 ms         1982 ms           10   3.51033G        7.34416/s
frodoPIR/server_respond/2^20/256B/process_time/real_time_mean                  99.5 ms         1173 ms           10   1.97878G        10.0565/s
frodoPIR/server_respond/2^20/256B/process_time/real_time_median                99.6 ms         1171 ms           10   1.97791G        10.0422/s
frodoPIR/server_respond/2^20/256B/process_time/real_time_stddev                3.45 ms         31.2 ms           10   79.3244M       0.349206/s
frodoPIR/server_respond/2^20/256B/process_time/real_time_cv                    3.47 %          2.66 %            10      4.01%            3.47%
frodoPIR/server_respond/2^20/256B/process_time/real_time_min                   94.8 ms         1111 ms           10   1.83851G        9.59048/s
frodoPIR/server_respond/2^20/256B/process_time/real_time_max                    104 ms         1216 ms           10   2.07275G        10.5484/s
frodoPIR/server_respond/2^17/256B/process_time/real_time_mean                  10.2 ms          103 ms           10   169.632M        97.9035/s
frodoPIR/server_respond/2^17/256B/process_time/real_time_median                10.2 ms          104 ms           10   171.053M        98.4863/s
frodoPIR/server_respond/2^17/256B/process_time/real_time_stddev               0.374 ms         3.22 ms           10   5.70744M        3.51239/s
frodoPIR/server_respond/2^17/256B/process_time/real_time_cv                    3.66 %          3.13 %            10      3.36%            3.59%
frodoPIR/server_respond/2^17/256B/process_time/real_time_min                   9.73 ms         96.5 ms           10   157.118M        91.4747/s
frodoPIR/server_respond/2^17/256B/process_time/real_time_max                   10.9 ms          106 ms           10   176.333M        102.752/s
frodoPIR/server_respond/2^18/256B/process_time/real_time_mean                  20.1 ms          227 ms           10   372.789M        49.8509/s
frodoPIR/server_respond/2^18/256B/process_time/real_time_median                20.1 ms          226 ms           10   374.042M        49.7974/s
frodoPIR/server_respond/2^18/256B/process_time/real_time_stddev               0.319 ms         5.85 ms           10   9.95863M       0.785662/s
frodoPIR/server_respond/2^18/256B/process_time/real_time_cv                    1.59 %          2.58 %            10      2.67%            1.58%
frodoPIR/server_respond/2^18/256B/process_time/real_time_min                   19.6 ms          218 ms           10   357.387M        48.2735/s
frodoPIR/server_respond/2^18/256B/process_time/real_time_max                   20.7 ms          235 ms           10   386.252M        50.9897/s
frodoPIR/client_prepare_query/2^19/256B/process_time/real_time_mean            69.3 ms          957 ms           10   1.68532G        14.4326/s
frodoPIR/client_prepare_query/2^19/256B/process_time/real_time_median          68.8 ms          953 ms           10   1.69159G        14.5309/s
frodoPIR/client_prepare_query/2^19/256B/process_time/real_time_stddev          1.58 ms         24.4 ms           10   65.4275M       0.323254/s
frodoPIR/client_prepare_query/2^19/256B/process_time/real_time_cv              2.28 %          2.55 %            10      3.88%            2.24%
frodoPIR/client_prepare_query/2^19/256B/process_time/real_time_min             67.3 ms          905 ms           10   1.51648G        13.7956/s
frodoPIR/client_prepare_query/2^19/256B/process_time/real_time_max             72.5 ms          992 ms           10   1.74798G        14.8494/s
frodoPIR/server_setup/2^16/256B/process_time/real_time_mean                    1.72 s          11.3 s            10   18.6947G       0.580374/s
frodoPIR/server_setup/2^16/256B/process_time/real_time_median                  1.73 s          11.4 s            10   18.8673G       0.576841/s
frodoPIR/server_setup/2^16/256B/process_time/real_time_stddev                 0.043 s         0.306 s            10   484.092M      0.0147395/s
frodoPIR/server_setup/2^16/256B/process_time/real_time_cv                      2.48 %          2.71 %            10      2.59%            2.54%
frodoPIR/server_setup/2^16/256B/process_time/real_time_min                     1.64 s          10.5 s            10   17.4679G       0.562965/s
frodoPIR/server_setup/2^16/256B/process_time/real_time_max                     1.78 s          11.5 s            10   19.0726G       0.609984/s
frodoPIR/server_respond/2^19/256B/process_time/real_time_mean                  48.3 ms          554 ms           10   920.533M        20.7528/s
frodoPIR/server_respond/2^19/256B/process_time/real_time_median                48.1 ms          552 ms           10   916.522M        20.8106/s
frodoPIR/server_respond/2^19/256B/process_time/real_time_stddev                1.91 ms         15.0 ms           10    21.216M       0.816012/s
frodoPIR/server_respond/2^19/256B/process_time/real_time_cv                    3.95 %          2.70 %            10      2.30%            3.93%
frodoPIR/server_respond/2^19/256B/process_time/real_time_min                   45.7 ms          535 ms           10   898.526M        19.4052/s
frodoPIR/server_respond/2^19/256B/process_time/real_time_max                   51.5 ms          577 ms           10   965.828M        21.8705/s
frodoPIR/client_query/2^16/256B/process_time/real_time_mean                    31.5 us         29.8 us           10   44.5464k       31.7019k/s
frodoPIR/client_query/2^16/256B/process_time/real_time_median                  31.3 us         29.6 us           10   44.1522k       31.9171k/s
frodoPIR/client_query/2^16/256B/process_time/real_time_stddev                 0.456 us        0.463 us           10   1.25677k        455.483/s
frodoPIR/client_query/2^16/256B/process_time/real_time_cv                      1.45 %          1.55 %            10      2.82%            1.44%
frodoPIR/client_query/2^16/256B/process_time/real_time_min                     31.0 us         29.3 us           10    42.985k       31.0537k/s
frodoPIR/client_query/2^16/256B/process_time/real_time_max                     32.2 us         30.5 us           10   46.4317k       32.2068k/s
frodoPIR/client_setup/2^20/256B/process_time/real_time_mean                   14627 ms        14626 ms           10   48.7324G      0.0684256/s
frodoPIR/client_setup/2^20/256B/process_time/real_time_median                 14771 ms        14770 ms           10   48.7354G      0.0677011/s
frodoPIR/client_setup/2^20/256B/process_time/real_time_stddev                   439 ms          439 ms           10   48.7721M       2.10304m/s
frodoPIR/client_setup/2^20/256B/process_time/real_time_cv                      3.00 %          3.00 %            10      0.10%            3.07%
frodoPIR/client_setup/2^20/256B/process_time/real_time_min                    13862 ms        13862 ms           10   48.6322G      0.0662292/s
frodoPIR/client_setup/2^20/256B/process_time/real_time_max                    15099 ms        15098 ms           10   48.7943G      0.0721371/s
frodoPIR/client_query/2^17/256B/process_time/real_time_mean                    57.4 us         55.8 us           10   120.746k       17.4153k/s
frodoPIR/client_query/2^17/256B/process_time/real_time_median                  57.4 us         55.8 us           10   120.894k        17.419k/s
frodoPIR/client_query/2^17/256B/process_time/real_time_stddev                 0.205 us        0.179 us           10   1.02941k         62.382/s
frodoPIR/client_query/2^17/256B/process_time/real_time_cv                      0.36 %          0.32 %            10      0.85%            0.36%
frodoPIR/client_query/2^17/256B/process_time/real_time_min                     57.0 us         55.4 us           10   119.049k       17.3367k/s
frodoPIR/client_query/2^17/256B/process_time/real_time_max                     57.7 us         56.0 us           10   122.084k       17.5481k/s
```

## Usage

FrodoPIR is a header-only C++20 library implementing all recommended variants (see table 5) in https://ia.cr/2022/98. FrodoPIR header files live `./include` directory, while only additional dependency `sha3` header files live under `sha3/include`.

- Let's begin by cloning the repository.

```bash
# Just clones FrodoPIR source tree, but not its submodule -based dependencies.
git clone https://github.com/itzmeanjan/frodoPIR.git
# Clones FrodoPIR source tree and also imports submodule -based dependencies.
git clone https://github.com/itzmeanjan/frodoPIR.git --recurse-submodules
```

- Move inside the directory holding the cloned repository and import all git submodule -based dependencies.

```bash
pushd frodoPIR
make -j # Also runs tests
popd
```

- Now that we've all the dependencies to use FrodoPIR header-only library, let's run our [example program](./examples/frodoPIR.cpp).

```bash
FrodoPIR_HEADERS=include
SHA3_HEADERS=sha3/include

g++ -std=c++20 -Wall -Wextra -pedantic -O3 -march=native -I $FrodoPIR_HEADERS -I $SHA3_HEADERS examples/frodoPIR.cpp
./a.out && echo $? # = 0 means success!

Original database row bytes    : bf71e04189fff486c062cf1e814bedfc2205422807da319d4ac6f5a956d63e48
PIR decoded database row bytes : bf71e04189fff486c062cf1e814bedfc2205422807da319d4ac6f5a956d63e48
```
