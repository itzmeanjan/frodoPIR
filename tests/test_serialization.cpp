#include "frodoPIR/internals/matrix/serialization.hpp"
#include "frodoPIR/internals/rng/prng.hpp"
#include <gtest/gtest.h>
#include <vector>

template<size_t db_entry_count, size_t db_entry_byte_len, size_t mat_element_bitlen>
static void
test_db_parsing_and_serialization()
{
  prng::prng_t prng;

  constexpr size_t db_byte_len = db_entry_count * db_entry_byte_len;

  std::vector<uint8_t> orig_db_bytes(db_byte_len, 0);
  std::vector<uint8_t> comp_db_bytes(db_byte_len, 0);

  auto orig_db_bytes_span = std::span<uint8_t, db_byte_len>(orig_db_bytes);
  auto comp_db_bytes_span = std::span<uint8_t, db_byte_len>(comp_db_bytes);

  prng.read(orig_db_bytes_span);

  auto D = frodoPIR_serialization::parse_db_bytes<db_entry_count, db_entry_byte_len, mat_element_bitlen>(orig_db_bytes_span);
  frodoPIR_serialization::serialize_parsed_db_matrix<db_entry_count, db_entry_byte_len, mat_element_bitlen>(D, comp_db_bytes_span);

  EXPECT_EQ(orig_db_bytes, comp_db_bytes);
}

TEST(FrodoPIR, ParsingDatabaseAndSerializingDatabaseMatrix)
{
  test_db_parsing_and_serialization<1ul << 16u, 1024, 10>();
  test_db_parsing_and_serialization<1ul << 20u, 1024, 9>();
}
