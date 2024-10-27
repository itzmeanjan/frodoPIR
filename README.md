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

To paint a more practical picture, imagine, we have a database with $2^{20}$ entries s.t. each entry is 256 -bytes, meaning database is of size 256 MB. We are setting up both server and client(s), on each of

Machine Type | Machine | Kernel | Compiler
--- | --- | --- | ---
DESKTOP | `12th Gen Intel(R) Core(TM) i7-1260P` | `Linux 6.8.0-45-generic x86_64` | `GCC 14.0.1`
SERVER | `ARM Neoverse-V2` i.e. AWS EC2 `c8g.2xlarge` | `Linux 6.8.0-1016-aws aarch64` | `GCC 13.2.0`

and this implementation of FrodoPIR is compiled with specified compiler, while also passing `-O3 -march=native -flto` compiler optimization flags.

Step | `(a)` Time Taken on *DESKTOP* setup | `(b)` Time Taken on *SERVER* setup | Ratio `a / b`
:-- | --: | --: | --:
`server_setup` | 29.43 seconds | 29.3 seconds | ~1.00
`client_setup` | 14.77 seconds | 16.27 seconds | ~0.91
`client_preprocess_query` | 136.54 milliseconds | 74.5 milliseconds | ~1.83
`client_query` | 449.73 microseconds | 169 microseconds | ~2.66
`server_respond` | 99.49 milliseconds | 25.2 milliseconds | ~3.95
`client_process_response` | 628.83 microseconds | 229 microseconds | ~2.75

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

### On 12th Gen Intel(R) Core(TM) i7-1260P

Compiled with **gcc version 14.0.1 20240412** on

```bash
$ uname -srm
Linux 6.8.0-45-generic x86_64
```

Benchmark result in JSON format @ [bench_result_on_Linux_6.8.0-45-generic_x86_64_with_g++_14.json](./bench_result_on_Linux_6.8.0-45-generic_x86_64_with_g++_14.json).

### On ARM Neoverse-V2 (AWS EC2 Instance `c8g.2xlarge`)

Compiled with **gcc version 13.2.0** on

```bash
$ uname -srm
Linux 6.8.0-1016-aws aarch64
```

Benchmark result in JSON format @ [bench_result_on_Linux_6.8.0-1016-aws_aarch64_with_g++_13.json](./bench_result_on_Linux_6.8.0-1016-aws_aarch64_with_g++_13.json).

> [!NOTE]
> More about AWS EC2 instances @ https://aws.amazon.com/ec2/instance-types.

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
