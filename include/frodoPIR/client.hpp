#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/serialization.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include "frodoPIR/internals/rng/prng.hpp"
#include "frodoPIR/internals/utility/force_inline.hpp"
#include "frodoPIR/internals/utility/params.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace frodoPIR_client {

// FrodoPIR query status
enum class query_status_t : uint32_t
{
  prepared,
  sent,
};

// FrodoPIR client query data type
template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
struct client_query_t
{
  query_status_t status;
  size_t db_index;
  frodoPIR_vector::row_vector_t<db_entry_count> b;
  frodoPIR_vector::row_vector_t<frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> c;
};

// Frodo *P*rivate *I*nformation *R*etrieval Client
template<size_t λ, size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen, size_t lwe_dimension>
  requires(frodoPIR_params::check_frodoPIR_params(λ, db_entry_count, mat_element_bitlen, lwe_dimension))
struct client_t
{
public:
  // Constructor(s)
  explicit constexpr client_t(
    frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count> pub_matA,
    frodoPIR_matrix::matrix_t<lwe_dimension, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> pub_matM)
    : A(std::move(pub_matA))
    , M(std::move(pub_matM))
  {
  }

  client_t(const client_t&) = default;
  client_t(client_t&&) = default;
  client_t& operator=(const client_t&) = default;
  client_t& operator=(client_t&&) = default;

  // Given a `λ` -bit seed and a byte serialized public matrix M, computed by frodoPIR server, this routine can be used
  // for setting up FrodoPIR client, ready to generate queries and process server response.
  static forceinline constexpr client_t setup(
    std::span<const uint8_t, λ / std::numeric_limits<uint8_t>::digits> seed_μ,
    std::span<const uint8_t, lwe_dimension * frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen) * sizeof(frodoPIR_matrix::zq_t)>
      pub_matM_bytes)
  {
    constexpr auto cols = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);

    return client_t(frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count>::template generate<λ>(seed_μ),
                    frodoPIR_matrix::matrix_t<lwe_dimension, cols>::from_le_bytes(pub_matM_bytes));
  }

  // Given `n` -many database row indices, this routine prepares `n` -many queries, for enquiring their values,
  // using FrodoPIR scheme. This function returns a boolean vector of length `n` s.t. each boolean value denotes
  // status of query preparation, for corresponding database row index, as appearing in `db_row_indices`, in order.
  [[nodiscard("Must use status of query preparation for DB row indices")]] constexpr std::vector<bool> prepare_query(std::span<const size_t> db_row_indices,
                                                                                                                     prng::prng_t& prng)
  {
    std::vector<bool> query_prep_status;
    query_prep_status.reserve(db_row_indices.size());

    for (const auto db_row_index : db_row_indices) {
      query_prep_status.push_back(this->prepare_query(db_row_index, prng));
    }

    return query_prep_status;
  }

  // Given a database row index, this routine prepares a query, so that value at that index can be enquired, using FrodoPIR scheme.
  // This routine returns boolean truth value if query for requested database row index is prepared - ready to be used, while also
  // placing an entry of query for corresponding database row index in the internal cache. But in case, query for corresponding database
  // row index has already been prepared, it returns false, denoting that no change has been done to the internal cache.
  [[nodiscard("Must use status of query preparation")]] constexpr bool prepare_query(const size_t db_row_index, prng::prng_t& prng)
  {
    if (this->queries.contains(db_row_index)) {
      return false;
    }

    const auto s = frodoPIR_vector::row_vector_t<lwe_dimension>::sample_from_uniform_ternary_distribution(prng);  // secret vector
    const auto e = frodoPIR_vector::row_vector_t<db_entry_count>::sample_from_uniform_ternary_distribution(prng); // error vector

    const auto b = s * this->A + e;
    const auto c = s * this->M;

    this->queries[db_row_index] = client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>{
      .status = query_status_t::prepared,
      .db_index = db_row_index,
      .b = b,
      .c = c,
    };

    return true;
  }

  // Given a database row index, for which query has already been prepared, this routine finalizes the query, making it ready
  // for processing at the server's end. This routine returns boolean truth value if byte serialized query is ready to be sent to server.
  // Or else it returns false, denoting either of
  //
  // (a) Query is not yet prepared for requested database row index.
  // (b) Query is already sent to server for requested database row index.
  [[nodiscard("Must use status of query finalization")]] constexpr bool query(const size_t db_row_index,
                                                                              std::span<uint8_t, db_entry_count * sizeof(frodoPIR_matrix::zq_t)> query_bytes)
  {
    if (!this->queries.contains(db_row_index)) {
      return false;
    }
    if (this->queries[db_row_index].status != query_status_t::prepared) {
      return false;
    }

    constexpr auto rho = 1ul << mat_element_bitlen;
    constexpr auto query_indicator_value = static_cast<frodoPIR_matrix::zq_t>(frodoPIR_matrix::Q / rho);

    this->queries[db_row_index].b[db_row_index] += query_indicator_value;
    this->queries[db_row_index].b.to_le_bytes(query_bytes);
    this->queries[db_row_index].status = query_status_t::sent;

    return true;
  }

  // Given a database row index, for which query has already been sent to server and we are awaiting response,
  // this routine can be used for processing server response, returning byte serialized content of queried row.
  // This function returns boolean truth value, if response is successfully decoded, while also removing entry
  // of corresponding client query from internal cache. Or it may return false, denoting either of
  //
  // (a) Query is not yet prepared for requested database row index.
  // (b) Query has not yet been sent to server, so can't process response for it.
  [[nodiscard("Must use status of response decoding")]] constexpr bool process_response(
    const size_t db_row_index,
    std::span<const uint8_t, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen) * sizeof(frodoPIR_matrix::zq_t)> response_bytes,
    std::span<uint8_t, db_entry_byte_len> db_row_bytes)
  {
    if (!this->queries.contains(db_row_index)) {
      return false;
    }
    if (this->queries[db_row_index].status != query_status_t::sent) {
      return false;
    }

    constexpr auto rho = 1ul << mat_element_bitlen;
    constexpr auto rounding_factor = static_cast<frodoPIR_matrix::zq_t>(frodoPIR_matrix::Q / rho);
    constexpr auto rounding_floor = rounding_factor / 2;

    constexpr size_t db_matrix_row_width = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);

    frodoPIR_vector::row_vector_t<db_matrix_row_width> db_matrix_row{};
    auto c_tilda = frodoPIR_vector::row_vector_t<db_matrix_row_width>::from_le_bytes(response_bytes);

    for (size_t idx = 0; idx < db_matrix_row_width; idx++) {
      const auto unscaled_res = c_tilda[idx] - this->queries[db_row_index].c[idx];

      const auto scaled_res = unscaled_res / rounding_factor;
      const auto scaled_rem = unscaled_res % rounding_factor;

      auto rounded_res = scaled_res;

      rounded_res += ((scaled_rem > rounding_floor) ? 1 : 0);
      rounded_res %= static_cast<frodoPIR_matrix::zq_t>(rho);

      db_matrix_row[idx] = rounded_res;
    }

    frodoPIR_serialization::serialize_db_row<db_entry_byte_len, mat_element_bitlen>(db_matrix_row, db_row_bytes);
    this->queries.erase(db_row_index);

    return true;
  }

private:
  frodoPIR_matrix::matrix_t<lwe_dimension, db_entry_count> A{};
  frodoPIR_matrix::matrix_t<lwe_dimension, frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen)> M{};
  std::unordered_map<size_t, client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>> queries{};
};

}
