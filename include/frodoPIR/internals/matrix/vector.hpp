#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"

namespace frodoPIR_vector {

template<size_t element_count>
using column_vector_t = frodoPIR_matrix::matrix_t<element_count, 1>;

template<size_t element_count>
using row_vector_t = frodoPIR_matrix::matrix_t<1, element_count>;

}
