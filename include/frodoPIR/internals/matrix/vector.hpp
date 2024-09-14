#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"

namespace frodoPIR_vector {

template<size_t element_count>
using vector_t = frodoPIR_matrix::matrix_t<element_count, 1>;

}
