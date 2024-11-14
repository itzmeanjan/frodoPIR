#pragma once
#include "randomshake/randomshake.hpp"

namespace csprng {

// Cryptographically Secure PRNG, offering 128 -bit security.
using csprng_t = randomshake::randomshake_t<128>;

}
