#pragma once
#include "frodoPIR/internals/matrix/matrix.hpp"
#include "frodoPIR/internals/matrix/serialization.hpp"
#include "frodoPIR/internals/matrix/vector.hpp"
#include "frodoPIR/internals/utility/csprng.hpp"
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

static constexpr size_t λ = 128;
static constexpr size_t LWE_DIMENSION = 1774;
static constexpr size_t SEED_BYTE_LEN = λ / std::numeric_limits<uint8_t>::digits;

// Frodo *P*rivate *I*nformation *R*etrieval Client
template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
  requires(frodoPIR_params::check_frodoPIR_params(db_entry_count, mat_element_bitlen))
struct client_t
{
public:
  // Compile-time computable values.
  static constexpr auto NUM_COLUMNS_IN_PARSED_DB = frodoPIR_matrix::get_required_num_columns(db_entry_byte_len, mat_element_bitlen);
  static constexpr auto PUBLIC_MATRIX_M_BYTE_LEN = LWE_DIMENSION * NUM_COLUMNS_IN_PARSED_DB * sizeof(frodoPIR_matrix::zq_t);
  static constexpr auto QUERY_BYTE_LEN = db_entry_count * sizeof(frodoPIR_matrix::zq_t);
  static constexpr auto RESPONSE_BYTE_LEN = NUM_COLUMNS_IN_PARSED_DB * sizeof(frodoPIR_matrix::zq_t);

  // Type aliases.
  using pub_mat_A_t = frodoPIR_matrix::matrix_t<LWE_DIMENSION, db_entry_count>;
  using pub_mat_M_t = frodoPIR_matrix::matrix_t<LWE_DIMENSION, NUM_COLUMNS_IN_PARSED_DB>;
  using secret_vec_t = frodoPIR_vector::row_vector_t<LWE_DIMENSION>;
  using error_vec_t = frodoPIR_vector::row_vector_t<db_entry_count>;
  using query_t = client_query_t<db_entry_count, db_entry_byte_len, mat_element_bitlen>;
  using response_t = frodoPIR_vector::row_vector_t<NUM_COLUMNS_IN_PARSED_DB>;

  // Constructor(s)
  explicit constexpr client_t(auto pub_matA, auto pub_matM)
    : A(std::move(pub_matA))
    , M(std::move(pub_matM))
  {
  }

  client_t() = default;
  client_t(const client_t&) = default;
  client_t(client_t&&) = default;
  client_t& operator=(const client_t&) = default;
  client_t& operator=(client_t&&) = default;

  // Given a `λ` -bit seed and a byte serialized public matrix M, computed by frodoPIR server, this routine can be used
  // for setting up FrodoPIR client, ready to generate queries and process server response.
  static forceinline constexpr client_t setup(std::span<const uint8_t, SEED_BYTE_LEN> seed_μ, std::span<const uint8_t, PUBLIC_MATRIX_M_BYTE_LEN> pub_matM_bytes)
  {
    return client_t(pub_mat_A_t::template generate<λ>(seed_μ), pub_mat_M_t::from_le_bytes(pub_matM_bytes));
  }

  // Given `n` -many database row indices, this routine prepares `n` -many queries, for enquiring their values,
  // using FrodoPIR scheme. This function returns a boolean vector of length `n` s.t. each boolean value denotes
  // status of query preparation, for corresponding database row index, as appearing in `db_row_indices`, in order.
  [[nodiscard("Must use status of query preparation for DB row indices")]] constexpr std::vector<bool> prepare_query(std::span<const size_t> db_row_indices,
                                                                                                                     csprng::csprng_t& csprng)
  {
    std::vector<bool> query_prep_status;
    query_prep_status.reserve(db_row_indices.size());

    for (const auto db_row_index : db_row_indices) {
      query_prep_status.push_back(this->prepare_query(db_row_index, csprng));
    }

    return query_prep_status;
  }

  // Given a database row index, this routine prepares a query, so that value at that index can be enquired, using FrodoPIR scheme.
  // This routine returns boolean truth value if query for requested database row index is prepared - ready to be used, while also
  // placing an entry of query for corresponding database row index in the internal cache. But in case, query for corresponding database
  // row index has already been prepared, it returns false, denoting that no change has been done to the internal cache.
  [[nodiscard("Must use status of query preparation")]] constexpr bool prepare_query(const size_t db_row_index, csprng::csprng_t& csprng)
  {
    if (this->queries.contains(db_row_index)) {
      return false;
    }

    const auto s = secret_vec_t::sample_from_uniform_ternary_distribution(csprng); // secret vector
    const auto e = error_vec_t::sample_from_uniform_ternary_distribution(csprng);  // error vector

    const auto b = s * this->A + e;
    const auto c = s * this->M;

    this->queries[db_row_index] = query_t{
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
  [[nodiscard("Must use status of query finalization")]] constexpr bool query(const size_t db_row_index, std::span<uint8_t, QUERY_BYTE_LEN> query_bytes)
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
  [[nodiscard("Must use status of response decoding")]] constexpr bool process_response(const size_t db_row_index,
                                                                                        std::span<const uint8_t, RESPONSE_BYTE_LEN> response_bytes,
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

    response_t db_matrix_row{};
    auto c_tilda = response_t::from_le_bytes(response_bytes);

    for (size_t idx = 0; idx < NUM_COLUMNS_IN_PARSED_DB; idx++) {
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
  pub_mat_A_t A{};
  pub_mat_M_t M{};
  std::unordered_map<size_t, query_t> queries{};
};

}
