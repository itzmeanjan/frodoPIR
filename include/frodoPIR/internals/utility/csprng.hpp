#pragma once
#include "randomshake/randomshake.hpp"

namespace csprng {

// Cryptographically Secure PRNG, backed by TurboSHAKE256 XOF.
using csprng_t = randomshake::randomshake_t<>;

}
