#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include <cstdint>
#include <limits>
#include <thread>

namespace frodoPIR_serialization {

// Given a byte serialized database s.t. it has `db_entry_count` -number of rows and each row contains `db_entry_byte_len` -bytes
// entry, this routines parses database into a matrix s.t. each element of matrix has at max `mat_element_bitlen` significant bits.
//
// Note, 0 < `mat_element_bitlen` < 32.
// Collects inspiration from https://github.com/brave-experiments/frodo-pir/blob/15573960/src/db.rs#L229-L254, while also making it multi-threaded.
template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
  requires(((0 < mat_element_bitlen) && (mat_element_bitlen < std::numeric_limits<frodoPIR_matrix::zq_t>::digits)))
frodoPIR_matrix::matrix_t<db_entry_count, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)>
parse_db_bytes(std::span<const uint8_t, db_entry_count * db_entry_byte_len> bytes)
{
  constexpr auto mat_element_mask = (1ul << mat_element_bitlen) - 1ul;
  constexpr size_t rows = db_entry_count;
  constexpr size_t cols = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);

  frodoPIR_matrix::matrix_t<rows, cols> mat{};

  auto parse_db_row = [=, &bytes, &mat](const size_t r_idx) {
    uint64_t buffer = 0;
    size_t buf_num_bits = 0;
    size_t c_idx = 0;

    size_t byte_off = r_idx * db_entry_byte_len;
    const size_t till_byte_off = byte_off + db_entry_byte_len;

    while (byte_off < till_byte_off) {
      const size_t remaining_num_bytes = till_byte_off - byte_off;

      const size_t fillable_num_bits = std::numeric_limits<decltype(buffer)>::digits - buf_num_bits;
      const size_t readable_num_bits = fillable_num_bits & (-std::numeric_limits<uint8_t>::digits);
      const size_t readable_num_bytes = std::min(readable_num_bits / std::numeric_limits<uint8_t>::digits, remaining_num_bytes);
      const size_t read_num_bits = readable_num_bytes * std::numeric_limits<uint8_t>::digits;

      const auto read_word = frodoPIR_utils::from_le_bytes<uint64_t>(bytes.subspan(byte_off, readable_num_bytes));
      byte_off += readable_num_bytes;

      buffer |= (read_word << buf_num_bits);
      buf_num_bits += read_num_bits;

      const size_t fillable_mat_elem_count = buf_num_bits / mat_element_bitlen;

      for (size_t elem_idx = 0; elem_idx < fillable_mat_elem_count; elem_idx++) {
        const auto mat_element = static_cast<frodoPIR_matrix::zq_t>(buffer & mat_element_mask);
        mat[{ r_idx, c_idx + elem_idx }] = mat_element;

        buffer >>= mat_element_bitlen;
        buf_num_bits -= mat_element_bitlen;
      }

      c_idx += fillable_mat_elem_count;
    }

    if ((buf_num_bits > 0) && (c_idx < cols)) {
      mat[{ r_idx, c_idx }] = buffer & mat_element_mask;
    }
  };

  constexpr size_t min_num_threads = 1;
  const size_t hw_hinted_max_num_threads = std::thread::hardware_concurrency();
  const size_t spawnable_num_threads = std::max(min_num_threads, hw_hinted_max_num_threads);

  const size_t num_rows_per_thread = rows / spawnable_num_threads;
  const size_t num_rows_distributed = num_rows_per_thread * spawnable_num_threads;
  const size_t remaining_num_rows = rows - num_rows_distributed;

  std::vector<std::thread> threads;
  threads.reserve(spawnable_num_threads);

  // Let's first spawn N -number of threads s.t. each of them will have equal many database rows to parse.
  for (size_t t_idx = 0; t_idx < spawnable_num_threads; t_idx++) {
    const size_t r_idx_begin = t_idx * num_rows_per_thread;
    const size_t r_idx_end = r_idx_begin + num_rows_per_thread;

    auto thread = std::thread([=]() {
      for (size_t r_idx = r_idx_begin; r_idx < r_idx_end; r_idx++) {
        parse_db_row(r_idx);
      }
    });

    threads.push_back(std::move(thread));
  }

  // Finally, remaining rows, if any, are parsed by "this" parent thread.
  if (remaining_num_rows > 0) {
    const size_t final_thread_r_idx_begin = num_rows_distributed;
    const size_t final_thread_r_idx_end = final_thread_r_idx_begin + remaining_num_rows;

    for (size_t r_idx = final_thread_r_idx_begin; r_idx < final_thread_r_idx_end; r_idx++) {
      parse_db_row(r_idx);
    }
  }

  // Now we wait until all of spawned threads finish their job.
  std::ranges::for_each(threads, [](auto& handle) { handle.join(); });

  return mat;
}

// Given a parsed database matrix as input s.t. each element of matrix has at max `mat_element_bitlen` significant bits, this routine serializes it
// into little-endian bytes of length `db_entry_count x db_entry_byte_len`, which can be interpretted as a database having `db_entry_count` -many
// entries s.t. each of those entries are `db_entry_byte_len` -bytes, using multiple threads.
//
// M = parse_db_bytes(orig_database_bytes)
// comp_database_bytes = serialize_parsed_db_matrix(M)
// assert(orig_database_bytes == comp_database_bytes)
template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
  requires(((0 < mat_element_bitlen) && (mat_element_bitlen < std::numeric_limits<frodoPIR_matrix::zq_t>::digits)))
void
serialize_parsed_db_matrix(
  frodoPIR_matrix::matrix_t<db_entry_count, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> const& db_matrix,
  std::span<uint8_t, db_entry_count * db_entry_byte_len> bytes)
{
  constexpr auto mat_element_mask = (1ul << mat_element_bitlen) - 1ul;
  constexpr size_t rows = db_entry_count;
  constexpr size_t cols = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
  constexpr size_t total_num_writable_bits_per_row = db_entry_byte_len * std::numeric_limits<uint8_t>::digits;

  auto serialize_parsed_db_row = [=, &db_matrix, &bytes](const size_t r_idx) {
    uint64_t buffer = 0;
    auto buffer_span = std::span<uint8_t, sizeof(buffer)>(reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer));

    size_t buf_num_bits = 0;
    size_t c_idx = 0;

    size_t byte_off = 0;
    const size_t row_begins_at_offset = r_idx * db_entry_byte_len;

    while (c_idx < cols) {
      const size_t remaining_num_bits = total_num_writable_bits_per_row - ((byte_off * std::numeric_limits<uint8_t>::digits) + buf_num_bits);
      const auto selected_bits = static_cast<uint64_t>(db_matrix[{ r_idx, c_idx }] & mat_element_mask);

      buffer |= (selected_bits << buf_num_bits);
      buf_num_bits += std::min(mat_element_bitlen, remaining_num_bits);

      const size_t writable_num_bits = buf_num_bits & (-std::numeric_limits<uint8_t>::digits);
      const size_t writable_num_bytes = writable_num_bits / std::numeric_limits<uint8_t>::digits;

      std::copy_n(buffer_span.begin(), writable_num_bytes, bytes.subspan(row_begins_at_offset + byte_off).begin());

      buffer >>= writable_num_bits;
      buf_num_bits -= writable_num_bits;

      c_idx++;
      byte_off += writable_num_bytes;
    }
  };

  constexpr size_t min_num_threads = 1;
  const size_t hw_hinted_max_num_threads = std::thread::hardware_concurrency();
  const size_t spawnable_num_threads = std::max(min_num_threads, hw_hinted_max_num_threads);

  const size_t num_rows_per_thread = rows / spawnable_num_threads;
  const size_t num_rows_distributed = num_rows_per_thread * spawnable_num_threads;
  const size_t remaining_num_rows = rows - num_rows_distributed;

  std::vector<std::thread> threads;
  threads.reserve(spawnable_num_threads);

  // Let's first spawn N -number of threads s.t. each of them will have equal many database rows to serialize.
  for (size_t t_idx = 0; t_idx < spawnable_num_threads; t_idx++) {
    const size_t r_idx_begin = t_idx * num_rows_per_thread;
    const size_t r_idx_end = r_idx_begin + num_rows_per_thread;

    auto thread = std::thread([=]() {
      for (size_t r_idx = r_idx_begin; r_idx < r_idx_end; r_idx++) {
        serialize_parsed_db_row(r_idx);
      }
    });

    threads.push_back(std::move(thread));
  }

  // Finally, remaining rows, if any, are serialized by "this" parent thread.
  if (remaining_num_rows > 0) {
    const size_t final_thread_r_idx_begin = num_rows_distributed;
    const size_t final_thread_r_idx_end = final_thread_r_idx_begin + remaining_num_rows;

    for (size_t r_idx = final_thread_r_idx_begin; r_idx < final_thread_r_idx_end; r_idx++) {
      serialize_parsed_db_row(r_idx);
    }
  }

  // Now we wait until all of spawned threads finish their job.
  std::ranges::for_each(threads, [](auto& handle) { handle.join(); });
}

// Given a row of parsed database s.t. each coefficient of input vector has at max `mat_element_bitlen` -many significant bits,
// this function can be used for serializing them as little-endian bytes, producing a byte array of length `db_entry_byte_len`.
template<size_t db_entry_byte_len, size_t mat_element_bitlen>
constexpr void
serialize_db_row(frodoPIR_vector::row_vector_t<frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> const& db_row,
                 std::span<uint8_t, db_entry_byte_len> bytes)
{
  serialize_parsed_db_matrix<1, db_entry_byte_len, mat_element_bitlen>(db_row, bytes);
}

}
