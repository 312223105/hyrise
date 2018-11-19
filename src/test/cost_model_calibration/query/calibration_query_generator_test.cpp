#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base_test.hpp"
#include "gtest/gtest.h"

#include "configuration/calibration_configuration.hpp"
#include "configuration/calibration_column_specification.hpp"
#include "query/calibration_query_generator.hpp"
#include "storage/encoding_type.hpp"
#include "storage/storage_manager.hpp"

namespace opossum {

class CalibrationQueryGeneratorTest : public BaseTest {
 protected:
  void SetUp() override {
    auto& manager = StorageManager::get();
    manager.add_table("SomeTable", load_table("src/test/tables/int_int_int_calibration.tbl", 1u));
  }
};

TEST_F(CalibrationQueryGeneratorTest, SimpleTest) {
  //             Query Generator expects one column with the name 'column_pk', which is handled as primary key
  std::vector<CalibrationColumnSpecification> columns = {
      CalibrationColumnSpecification{"column_pk", DataType::Int, "uniform", false, 100, EncodingType::Unencoded},
      CalibrationColumnSpecification{"a", DataType::Int, "uniform", false, 100, EncodingType::Unencoded},
      CalibrationColumnSpecification{"b", DataType::String, "uniform", false, 100, EncodingType::Unencoded}};

  const CalibrationConfiguration configuration {
          {},
          "",
          "",
          1,
          {EncodingType::Unencoded},
          {DataType::Int, DataType::String},
          {0.1, 0.8},
          {}
  };

  const CalibrationQueryGenerator generator({"SomeTable"}, columns, configuration);
  const auto query_templates = generator.generate_queries();

  for (const auto& query : query_templates) {
    query->print();
  }
}

}  // namespace opossum
