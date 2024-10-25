#pragma once
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include "frodoPIR/internals/utility/utils.hpp"
#include "shake128.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace frodoPIR_matrix {

// All arithmetic operations are performed modulo 2^32, for which we have native reduction.
using zq_t = uint32_t;
constexpr auto Q = static_cast<uint64_t>(std::numeric_limits<zq_t>::max()) + 1;

// Size of interval, used for sampling from uniform ternary distribution χ.
inline constexpr size_t TERNARY_INTERVAL_SIZE = (std::numeric_limits<zq_t>::max() - 2) / 3;
// Uniform sampled value to be rejected, if greater than sampling max, which is < uint32_t_MAX.
inline constexpr size_t TERNARY_REJECTION_SAMPLING_MAX = TERNARY_INTERVAL_SIZE * 3;

// Lambda for computing row width (i.e. number of columns) of parsed database matrix.
constexpr auto get_required_num_columns = [](const size_t db_entry_byte_len, const size_t mat_element_bitlen) {
  const size_t db_entry_bit_len = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;
  const size_t required_num_cols = (db_entry_bit_len + (mat_element_bitlen - 1)) / mat_element_bitlen;

  return required_num_cols;
};

// Matrix of dimension `rows x cols`.
template<size_t rows, size_t cols>
  requires((rows > 0) && (cols > 0))
struct matrix_t
{
public:
  // Constructor(s)
  forceinline constexpr matrix_t() { this->elements = std::vector<zq_t>(rows * cols, zq_t{}); };
  explicit matrix_t(std::vector<zq_t> elements)
    : elements(std::move(elements))
  {
  }
  matrix_t(const matrix_t&) = default;
  matrix_t(matrix_t&&) = default;
  matrix_t& operator=(const matrix_t&) = default;
  matrix_t& operator=(matrix_t&&) = default;

  // Given a `λ` -bit seed, this routine uniform random samples a matrix of dimension `rows x cols`.
  template<size_t λ>
    requires(std::endian::native == std::endian::little)
  static forceinline matrix_t generate(std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> μ)
  {
    constexpr size_t row_byte_len = cols * sizeof(zq_t);

    prng::prng_t prng(μ);
    matrix_t mat{};

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      const size_t row_begins_at = r_idx * row_byte_len;

      auto row_begin_ptr = reinterpret_cast<uint8_t*>(mat.elements.data()) + row_begins_at;
      auto row_span = std::span<uint8_t, row_byte_len>(row_begin_ptr, row_byte_len);

      prng.read(row_span);
    }

    return mat;
  }

  // Given a seeded PRNG, this routine can be used for sampling a row/ column vector, s.t. each value is rejection sampled from
  // a uniform ternary distribution χ. Returns sampled value ∈ {-1, 0, +1}.
  //
  // Collects inspiration from https://github.com/brave-experiments/frodo-pir/blob/15573960/src/utils.rs#L102-L125.
  static forceinline constexpr matrix_t sample_from_uniform_ternary_distribution(prng::prng_t& prng)
    requires((rows == 1) || (cols == 1))
  {
    matrix_t mat{};

    constexpr size_t buffer_byte_len = (8 * shake128::RATE) / std::numeric_limits<uint8_t>::digits;
    constexpr size_t total_num_elements = rows * cols;

    std::array<uint8_t, buffer_byte_len> buffer{};
    auto buffer_span = std::span(buffer);

    size_t buffer_offset = 0;
    size_t e_idx = 0;

    while (e_idx < total_num_elements) {
      zq_t val = std::numeric_limits<zq_t>::max();

      while (val > TERNARY_REJECTION_SAMPLING_MAX) {
        if ((buffer_offset + sizeof(zq_t)) > buffer_span.size()) {
          const size_t remaining_num_random_bytes = buffer_span.size() - buffer_offset;

          std::copy_n(buffer_span.last(remaining_num_random_bytes).begin(), remaining_num_random_bytes, buffer_span.begin());
          prng.read(buffer_span.subspan(remaining_num_random_bytes));
          buffer_offset = 0;
        }

        val = frodoPIR_utils::from_le_bytes<zq_t>(buffer_span.subspan(buffer_offset, sizeof(zq_t)));
        buffer_offset += sizeof(zq_t);
      }

      zq_t ternary = 0;
      if ((val > TERNARY_INTERVAL_SIZE) && (val <= (2 * TERNARY_INTERVAL_SIZE))) {
        ternary = 1;
      } else if (val > (2 * TERNARY_INTERVAL_SIZE)) {
        ternary = std::numeric_limits<zq_t>::max();
      }

      mat[e_idx] = ternary;
      e_idx++;
    }

    return mat;
  }

  // Returns an identity matrix of dimension `rows x rows`.
  static forceinline constexpr matrix_t identity()
    requires(rows == cols)
  {
    matrix_t mat{};

    for (size_t idx = 0; idx < rows; idx++) {
      mat[{ idx, idx }] = zq_t(1);
    }

    return mat;
  }

  // Accessor, using {row_index, column_index} pair.
  forceinline constexpr zq_t& operator[](const std::pair<size_t, size_t> idx)
  {
    const auto [r_idx, c_idx] = idx;
    return this->elements[r_idx * cols + c_idx];
  }
  forceinline constexpr const zq_t& operator[](const std::pair<size_t, size_t> idx) const
  {
    const auto [r_idx, c_idx] = idx;
    return this->elements[r_idx * cols + c_idx];
  }

  // Accessor, using linearized index.
  forceinline constexpr zq_t& operator[](const size_t lin_idx) { return this->elements[lin_idx]; }
  forceinline constexpr const zq_t& operator[](const size_t lin_idx) const { return this->elements[lin_idx]; }

  // Get byte length of serialized matrix.
  static forceinline constexpr size_t get_byte_len() { return rows * cols * sizeof(zq_t); }

  // Check equality of two equal dimension matrices, returning boolean result.
  forceinline constexpr bool operator==(const matrix_t& rhs) const
  {
    zq_t result = 0;

    for (size_t r_idx = 0; r_idx < rows; r_idx++) {
      for (size_t c_idx = 0; c_idx < cols; c_idx++) {
        result ^= ((*this)[{ r_idx, c_idx }] ^ rhs[{ r_idx, c_idx }]);
      }
    }

    return (result == 0);
  }

  // Given two matrices A, B of equal dimension, this routine can be used for performing matrix addition over Zq,
  // returning a matrix of same dimension, using multiple threads.
  forceinline matrix_t operator+(const matrix_t& rhs) const
  {
    matrix_t res{};

    constexpr size_t min_num_threads = 1;
    const size_t hw_hinted_max_num_threads = std::thread::hardware_concurrency();
    const size_t spawnable_num_threads = std::max(min_num_threads, hw_hinted_max_num_threads);

    const size_t total_num_elements = rows * cols;
    const size_t num_elements_per_thread = total_num_elements / spawnable_num_threads;
    const size_t num_elements_distributed = num_elements_per_thread * spawnable_num_threads;
    const size_t remaining_num_elements = total_num_elements - num_elements_distributed;

    std::vector<std::thread> threads;
    threads.reserve(spawnable_num_threads);

    // Let's first spawn N -number of threads s.t. each of them will have equal many rows to work on.
    for (size_t t_idx = 0; t_idx < spawnable_num_threads; t_idx++) {
      const size_t e_idx_begin = t_idx * num_elements_per_thread;
      const size_t e_idx_end = e_idx_begin + num_elements_per_thread;

      auto thread = std::thread([=, this, &rhs, &res]() {
        for (size_t e_idx = e_idx_begin; e_idx < e_idx_end; e_idx++) {
          res[e_idx] = (*this)[e_idx] + rhs[e_idx];
        }
      });

      threads.push_back(std::move(thread));
    }

    // Finally, remaining rows, if any, are processed by "this" parent thread.
    if (remaining_num_elements > 0) {
      const size_t final_thread_e_idx_begin = num_elements_distributed;
      const size_t final_thread_e_idx_end = final_thread_e_idx_begin + remaining_num_elements;

      for (size_t e_idx = final_thread_e_idx_begin; e_idx < final_thread_e_idx_end; e_idx++) {
        res[e_idx] = (*this)[e_idx] + rhs[e_idx];
      }
    }

    // Now we wait until all of spawned threads finish their job.
    std::ranges::for_each(threads, [](auto& handle) { handle.join(); });

    return res;
  }

  // Given two matrices A ( of dimension rows x cols ) and B ( of dimension rhs_rows x rhs_cols ) s.t. cols == rhs_rows,
  // this routine can be used for multiplying them over Zq, resulting into another matrix (C) of dimension rows x rhs_cols.
  //
  // Following home-brewed matrix multiplication technique from
  // https://lemire.me/blog/2024/06/13/rolling-your-own-fast-matrix-multiplication-loop-order-and-vectorization, while also
  // parallelizing the outer loop using std::thread API.
  template<size_t rhs_rows, size_t rhs_cols>
    requires((cols == rhs_rows))
  forceinline matrix_t<rows, rhs_cols> operator*(const matrix_t<rhs_rows, rhs_cols>& rhs) const
  {
    matrix_t<rows, rhs_cols> res{};

    constexpr size_t min_num_threads = 1;
    const size_t hw_hinted_max_num_threads = std::thread::hardware_concurrency();
    const size_t spawnable_num_threads = std::max(min_num_threads, hw_hinted_max_num_threads);

    // Distribute work either row-wise or column-wise, based on which one has more work.
    constexpr size_t distributable_work_count = std::max(rows, rhs_cols);
    constexpr bool if_distribute_across_row = rows >= rhs_cols;

    const size_t num_work_per_thread = distributable_work_count / spawnable_num_threads;
    const size_t num_work_distributed = num_work_per_thread * spawnable_num_threads;
    const size_t remaining_num_work = distributable_work_count - num_work_distributed;

    std::vector<std::thread> threads;
    threads.reserve(spawnable_num_threads);

    // Let's first spawn N -number of threads s.t. each of them will have equal many rows to work on.
    for (size_t t_idx = 0; t_idx < spawnable_num_threads; t_idx++) {
      if constexpr (if_distribute_across_row) {
        // If there are more (or equal many) rows than columns, it's better to distribute computation of rows.
        const size_t r_idx_begin = t_idx * num_work_per_thread;
        const size_t r_idx_end = r_idx_begin + num_work_per_thread;

        auto thread = std::thread([=, this, &rhs, &res]() {
          for (size_t r_idx = r_idx_begin; r_idx < r_idx_end; r_idx++) {
            for (size_t k = 0; k < cols; k++) {
              for (size_t c_idx = 0; c_idx < rhs_cols; c_idx++) {
                res[{ r_idx, c_idx }] += (*this)[{ r_idx, k }] * rhs[{ k, c_idx }];
              }
            }
          }
        });

        threads.push_back(std::move(thread));
      } else {
        // If there are more columns, it's better to distribute computation of columns across threads.
        const size_t c_idx_begin = t_idx * num_work_per_thread;
        const size_t c_idx_end = c_idx_begin + num_work_per_thread;

        auto thread = std::thread([=, this, &rhs, &res]() {
          for (size_t r_idx = 0; r_idx < rows; r_idx++) {
            for (size_t k = 0; k < cols; k++) {
              for (size_t c_idx = c_idx_begin; c_idx < c_idx_end; c_idx++) {
                res[{ r_idx, c_idx }] += (*this)[{ r_idx, k }] * rhs[{ k, c_idx }];
              }
            }
          }
        });

        threads.push_back(std::move(thread));
      }
    }

    // Finally, remaining rows, if any, are processed by "this" parent thread.
    if (remaining_num_work > 0) {
      if constexpr (if_distribute_across_row) {
        const size_t final_thread_r_idx_begin = num_work_distributed;
        const size_t final_thread_r_idx_end = final_thread_r_idx_begin + remaining_num_work;

        for (size_t r_idx = final_thread_r_idx_begin; r_idx < final_thread_r_idx_end; r_idx++) {
          for (size_t k = 0; k < cols; k++) {
            for (size_t c_idx = 0; c_idx < rhs_cols; c_idx++) {
              res[{ r_idx, c_idx }] += (*this)[{ r_idx, k }] * rhs[{ k, c_idx }];
            }
          }
        }
      } else {
        const size_t final_thread_c_idx_begin = num_work_distributed;
        const size_t final_thread_c_idx_end = final_thread_c_idx_begin + remaining_num_work;

        for (size_t r_idx = 0; r_idx < rows; r_idx++) {
          for (size_t k = 0; k < cols; k++) {
            for (size_t c_idx = final_thread_c_idx_begin; c_idx < final_thread_c_idx_end; c_idx++) {
              res[{ r_idx, c_idx }] += (*this)[{ r_idx, k }] * rhs[{ k, c_idx }];
            }
          }
        }
      }
    }

    // Now we wait until all of spawned threads finish their job.
    std::ranges::for_each(threads, [](auto& handle) { handle.join(); });

    return res;
  }

  // Given a matrix M of dimension `rows x cols`, this routine can be used for serializing each of its elements as
  // four little-endian bytes and concatenating them in order to compute a byte array of length `rows * cols * 4`.
  forceinline void to_le_bytes(std::span<uint8_t, matrix_t::get_byte_len()> bytes) const
    requires(std::endian::native == std::endian::little)
  {
    auto elements_ptr = reinterpret_cast<const uint8_t*>(this->elements.data());
    auto bytes_ptr = reinterpret_cast<uint8_t*>(bytes.data());

    constexpr size_t min_num_threads = 1;
    const size_t hw_hinted_max_num_threads = std::thread::hardware_concurrency();
    const size_t spawnable_num_threads = std::max(min_num_threads, hw_hinted_max_num_threads);

    constexpr size_t total_num_bytes = bytes.size();
    const size_t num_bytes_per_thread = total_num_bytes / spawnable_num_threads;
    const size_t num_bytes_distributed = num_bytes_per_thread * spawnable_num_threads;
    const size_t remaining_num_bytes = total_num_bytes - num_bytes_distributed;

    std::vector<std::thread> threads;
    threads.reserve(spawnable_num_threads);

    for (size_t t_idx = 0; t_idx < spawnable_num_threads; t_idx++) {
      const size_t byte_idx_begin = t_idx * num_bytes_per_thread;

      auto thread = std::thread([=, &elements_ptr, &bytes_ptr]() { memcpy(bytes_ptr + byte_idx_begin, elements_ptr + byte_idx_begin, num_bytes_per_thread); });
      threads.push_back(std::move(thread));
    }

    if (remaining_num_bytes > 0) {
      const size_t final_thread_byte_idx_begin = num_bytes_distributed;
      memcpy(bytes_ptr + final_thread_byte_idx_begin, elements_ptr + final_thread_byte_idx_begin, remaining_num_bytes);
    }

    std::ranges::for_each(threads, [](auto& handle) { handle.join(); });
  }

  // Given a byte array of length `rows * cols * 4`, this routine can be used for deserializing it as a matrix of dimension
  // `rows x cols` s.t. each matrix element is computed by interpreting four consecutive bytes in little-endian order.
  forceinline static matrix_t from_le_bytes(std::span<const uint8_t, matrix_t::get_byte_len()> bytes)
    requires(std::endian::native == std::endian::little)
  {
    matrix_t res{};

    auto elements_ptr = reinterpret_cast<uint8_t*>(res.elements.data());
    auto bytes_ptr = reinterpret_cast<const uint8_t*>(bytes.data());

    constexpr size_t min_num_threads = 1;
    const size_t hw_hinted_max_num_threads = std::thread::hardware_concurrency();
    const size_t spawnable_num_threads = std::max(min_num_threads, hw_hinted_max_num_threads);

    constexpr size_t total_num_bytes = bytes.size();
    const size_t num_bytes_per_thread = total_num_bytes / spawnable_num_threads;
    const size_t num_bytes_distributed = num_bytes_per_thread * spawnable_num_threads;
    const size_t remaining_num_bytes = total_num_bytes - num_bytes_distributed;

    std::vector<std::thread> threads;
    threads.reserve(spawnable_num_threads);

    for (size_t t_idx = 0; t_idx < spawnable_num_threads; t_idx++) {
      const size_t byte_idx_begin = t_idx * num_bytes_per_thread;

      auto thread = std::thread([=, &elements_ptr, &bytes_ptr]() { memcpy(elements_ptr + byte_idx_begin, bytes_ptr + byte_idx_begin, num_bytes_per_thread); });
      threads.push_back(std::move(thread));
    }

    if (remaining_num_bytes > 0) {
      const size_t final_thread_byte_idx_begin = num_bytes_distributed;
      memcpy(elements_ptr + final_thread_byte_idx_begin, bytes_ptr + final_thread_byte_idx_begin, remaining_num_bytes);
    }

    std::ranges::for_each(threads, [](auto& handle) { handle.join(); });
    return res;
  }

private:
  std::vector<zq_t> elements;
};

}
