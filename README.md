> [!CAUTION]
> Following implementation of FrodoPIR private information retrieval scheme is designed from scratch, having zero third-party dependencies. As it's not yet audited, you are advised to exercise caution, in case you plan to use it in production!

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

To paint a more practical picture, imagine, we have a database with $2^{20}$ (~1 million) entries s.t. each entry is 1024 -bytes (1kB), meaning database is of size 1 GB. We are setting up both server and client(s), on each of

Machine Type | Machine | Kernel | Compiler | Memory Read Speed
--- | --- | --- | --- | ---
aarch64 server | AWS EC2 `m8g.8xlarge` | `Linux 6.8.0-1021-aws aarch64` | `GCC 13.2.0` | 28.25 GB/s
x86_64 server | AWS EC2 `m7i.8xlarge` | `Linux 6.8.0-1021-aws x86_64` | `GCC 13.2.0` | 10.33 GB/s

and this implementation of FrodoPIR is compiled with specified compiler, while also passing `-O3 -march=native -flto` compiler optimization flags.

> [!NOTE]
> Memory read speed is measured using `$ sysbench memory --memory-block-size=1G --memory-total-size=20G --memory-oper=read run` command.

Step | `(a)` Time Taken on `aarch64` server | `(b)` Time Taken on `x86_64` server | Ratio `a / b`
:-- | --: | --: | --:
`server_setup` | 46.7 seconds | 67.7 seconds | 0.68
`client_setup` | 21.7 seconds | 20.5 seconds | 1.05
`client_preprocess_query` | 39.4 milliseconds | 65.6 milliseconds | 0.6
`client_query` | 146 microseconds | 454 microseconds | 0.32
`server_respond` | 18.1 milliseconds | 30.8 milliseconds | 0.58
`client_process_response` | 782 microseconds | 1257 microseconds | 0.62

So, bandwidth of the `server_respond` algorithm, which needs to traverse through the whole processed database, is
- (a) For `aarch64` server: 55.24 GB/s
- (b) For `x86_64` server: 32.46 GB/s

Here I'm maintaining a zero-dependency, header-only C++20 library implementation of FrodoPIR scheme, supporting all parameter sets, as suggested in table 5 of https://ia.cr/2022/981. Using this library is very easy, follow [here](#usage).

## Prerequisites
- A C++ compiler with support for compiling C++20 code.

```bash
$ g++ --version
g++ (Ubuntu 14.2.0-4ubuntu2) 14.2.0s
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
make test -j                    # Run tests without any sort of sanitizers, with default C++ compiler.
CXX=clang++ make test -j        # Switch to non-default compiler, by setting variable `CXX`.

make debug_asan_test -j    # Run tests with AddressSanitizer enabled, with `-O1`.
make release_asan_test -j  # Run tests with AddressSanitizer enabled, with `-O3 -march=native`.
make debug_ubsan_test -j   # Run tests with UndefinedBehaviourSanitizer enabled, with `-O1`.
make release_ubsan_test -j # Run tests with UndefinedBehaviourSanitizer enabled, with `-O3 -march=native`.
```

```bash
PASSED TESTS (7/7):
       3 ms: build/test/test.out FrodoPIR.RowVectorMatrixMultiplicationWorks
      15 ms: build/test/test.out FrodoPIR.MatrixSerializationWorks
      20 ms: build/test/test.out FrodoPIR.MatrixTranspositionWorks
     123 ms: build/test/test.out FrodoPIR.MatrixMultiplicationWorks
    2447 ms: build/test/test.out FrodoPIR.ClientQueryCacheStateTransition
    5112 ms: build/test/test.out FrodoPIR.ParsingDatabaseAndSerializingDatabaseMatrix
   44540 ms: build/test/test.out FrodoPIR.PrivateInformationRetrieval
```

> [!NOTE]
> There is a help menu, which introduces you to all available commands; just run `make` from the root directory of this project.

## Benchmarking
Benchmarking of all 6 algorithms of FrodoPIR scheme can be done, by issuing

```bash
# For switching to non-default compiler, set `CXX` variable, right before invoking following command.

make benchmark -j  # If you haven't built google-benchmark library with libPFM support.
make perf -j       # If you have built google-benchmark library with libPFM support.
```

> [!CAUTION]
> You must put all the CPU cores on **performance** mode before running benchmark program, follow guide @ https://github.com/google/benchmark/blob/main/docs/reducing_variance.md.

- **On AWS EC2 Instance `m8g.8xlarge`**: Benchmark result in JSON format @ [bench_result_on_Linux_6.8.0-1021-aws_aarch64_with_g++_13](./bench_result_on_Linux_6.8.0-1021-aws_aarch64_with_g++_13.json).
- **On AWS EC2 Instance `m7i.8xlarge`**: Benchmark result in JSON format @ [bench_result_on_Linux_6.8.0-1021-aws_x86_64_with_g++_13](./bench_result_on_Linux_6.8.0-1021-aws_x86_64_with_g++_13.json).

> [!NOTE]
> More about AWS EC2 instances @ https://aws.amazon.com/ec2/instance-types.

## Usage
FrodoPIR is a header-only C++20 library implementing all recommended variants (see table 5) in https://ia.cr/2022/98. FrodoPIR header files live `./include` directory, while additional dependency `sha3` and `RandomShake` header files live under `sha3/include` and `RandomShake/include`, respectively.

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
make test -j # Also runs tests
popd
```

- Now that we've all the dependencies to use FrodoPIR header-only library, let's run our [example](./examples/frodoPIR.cpp) program, by issuing `make example`.

```bash
FrodoPIR:
Number of entries in Index Database  : 65536
Size of each database entry          : 1.0KB
DB size                              : 64.0MB
Encoded DB matrix element bit length : 10
Encoded DB matrix dimension          : 65536 x 820
Seed size                            : 16.0B
Hint download size                   : 5.5MB
Query vector size                    : 256.0KB
Response vector size                 : 3.2KB

Original database row bytes    : 86ec860911c1ba1b573ffb956f2e75c65a61de75328cf5e7a3ca061b2114be2d42ae1515eb013b34dc1be335790800ebf3f44025c08121cd36ef6e57b504f82e1a0495665e5bad163b93a07665c0a8675916701150b40c7f46dfdd540c26ac2ada284bdd3c360960f4a24cc37235f7473ddbe5be50d0be3c9f15a933071a974d1aa5cad19b476d482444bcf5bbf5391546b7e95448a7e4c9351de85b4771878812b060c6f62bce6727cd5821e371a71ccb62a2e527d2e8b1b800dbe1e424bcea7cfc641e44c9b17768efef1c0b3cf166dd466e02b68c2f4ba61e0f20904a67654f61d773b955b163d769b75c9cc2760ad8d1aa40e473d526cd86a21d46c5d646877949c571fbaf06171eac7bbe0f7a27b371947fbd73682e5adcc7df8acd9c780e1378d7834d8e5479427f01793c3d130cffa6d5a6ecc39cbcb3303a960013caf51d0de7cd2146d282a0187ef131251cde7043ab071b89804e9abfe4f5636ed812c0199e0e60834025bb9df35cd46b7bd0019ff93fe565841faf45e4e4250164454b8115c3f66bdda1e430c696821e098a4b48451e0122f34c961101432f7ef1f7de6b46d0f19e79fdd2a434c0f2c63b820231d3fcaf6e364ef0b94e36e4abeda1c2fe2ad6a7fc89b7e881343018929e05cb402594a6570791931debf1b167adbc58f128765166e815d4a9a5c8f3bd2fdf4741cd257336481caa686d6765f87afa1ea58a17f3e43d10679aa88a521586a8d93a7cadfca9df4418c2c1653792d09fe632f61d833994122ce7ca0b73ade3dbed7fdd599ef29d9c9c33cd5be455e8358d0d6e6da309b4ec25be5e82340644ccca23cbc1ef2489507deaa14e5c5cca45aa02a3833f6638eeed8dcf25718369513c68b1386b857790c6c9f69c274c65fc69362d5d0fc93749f6312cb38a04399c31a34d0799ba2c4d24ce07ead41bbcaeee0dbc525321fb81dea05c42c281c14c26559f6dda984c54fbbf364771172a49e5c9140ff95408956d0c415b335cef278d264822224d2d56bec651fafe99921cbc1aab39f1fd5b3f159054afad396cae5804894520e96e2c31dd3163e6e90415390f7c0b8485f670ba5fb98ce8f8e0387a73365254721a13070b6c8680be50f06ff06f0493a903da0eda0ef92ca8d503fe8364eca1401103fa0b3e9929809bb19dd0af6ea3a7133a0c6aff7aa401872b969a18d5081ffa69f0065921c7a35c91f7be6e639705d72f8baac41e44b842bef2d59105925bcc637b5bf80984b2b5b841ebece3f99c51fcc8b328f8f0c879643b3f28628f60c929421c895e6b13a152fe836e213c0e2c9382a4fe504cb7b41947c48eed3eb540d804999deba1d056171f333396265abe42f8e8f9a8ac60f48ed3017920d757aa1aa22c0b001f575eb16cb69812108b65e62530979431493737af23ce6e28d7bcbc585a451b9b780a
PIR decoded database row bytes : 86ec860911c1ba1b573ffb956f2e75c65a61de75328cf5e7a3ca061b2114be2d42ae1515eb013b34dc1be335790800ebf3f44025c08121cd36ef6e57b504f82e1a0495665e5bad163b93a07665c0a8675916701150b40c7f46dfdd540c26ac2ada284bdd3c360960f4a24cc37235f7473ddbe5be50d0be3c9f15a933071a974d1aa5cad19b476d482444bcf5bbf5391546b7e95448a7e4c9351de85b4771878812b060c6f62bce6727cd5821e371a71ccb62a2e527d2e8b1b800dbe1e424bcea7cfc641e44c9b17768efef1c0b3cf166dd466e02b68c2f4ba61e0f20904a67654f61d773b955b163d769b75c9cc2760ad8d1aa40e473d526cd86a21d46c5d646877949c571fbaf06171eac7bbe0f7a27b371947fbd73682e5adcc7df8acd9c780e1378d7834d8e5479427f01793c3d130cffa6d5a6ecc39cbcb3303a960013caf51d0de7cd2146d282a0187ef131251cde7043ab071b89804e9abfe4f5636ed812c0199e0e60834025bb9df35cd46b7bd0019ff93fe565841faf45e4e4250164454b8115c3f66bdda1e430c696821e098a4b48451e0122f34c961101432f7ef1f7de6b46d0f19e79fdd2a434c0f2c63b820231d3fcaf6e364ef0b94e36e4abeda1c2fe2ad6a7fc89b7e881343018929e05cb402594a6570791931debf1b167adbc58f128765166e815d4a9a5c8f3bd2fdf4741cd257336481caa686d6765f87afa1ea58a17f3e43d10679aa88a521586a8d93a7cadfca9df4418c2c1653792d09fe632f61d833994122ce7ca0b73ade3dbed7fdd599ef29d9c9c33cd5be455e8358d0d6e6da309b4ec25be5e82340644ccca23cbc1ef2489507deaa14e5c5cca45aa02a3833f6638eeed8dcf25718369513c68b1386b857790c6c9f69c274c65fc69362d5d0fc93749f6312cb38a04399c31a34d0799ba2c4d24ce07ead41bbcaeee0dbc525321fb81dea05c42c281c14c26559f6dda984c54fbbf364771172a49e5c9140ff95408956d0c415b335cef278d264822224d2d56bec651fafe99921cbc1aab39f1fd5b3f159054afad396cae5804894520e96e2c31dd3163e6e90415390f7c0b8485f670ba5fb98ce8f8e0387a73365254721a13070b6c8680be50f06ff06f0493a903da0eda0ef92ca8d503fe8364eca1401103fa0b3e9929809bb19dd0af6ea3a7133a0c6aff7aa401872b969a18d5081ffa69f0065921c7a35c91f7be6e639705d72f8baac41e44b842bef2d59105925bcc637b5bf80984b2b5b841ebece3f99c51fcc8b328f8f0c879643b3f28628f60c929421c895e6b13a152fe836e213c0e2c9382a4fe504cb7b41947c48eed3eb540d804999deba1d056171f333396265abe42f8e8f9a8ac60f48ed3017920d757aa1aa22c0b001f575eb16cb69812108b65e62530979431493737af23ce6e28d7bcbc585a451b9b780a
```
