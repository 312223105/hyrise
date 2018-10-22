#include "cost_model_calibration.hpp"

#include <json.hpp>

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <iostream>

#include "cost_model_feature_extractor.hpp"
#include "feature/calibration_example.hpp"
#include "import_export/csv_writer.hpp"
#include "query/calibration_query_generator.hpp"
#include "scheduler/current_scheduler.hpp"
#include "scheduler/node_queue_scheduler.hpp"
#include "sql/sql_pipeline_builder.hpp"
#include "sql/sql_query_cache.hpp"
#include "storage/chunk_encoder.hpp"
#include "storage/storage_manager.hpp"
#include "tpch/tpch_db_generator.hpp"
#include "tpch/tpch_queries.hpp"
#include "utils/format_duration.hpp"
#include "utils/load_table.hpp"

namespace opossum {

CostModelCalibration::CostModelCalibration(const CalibrationConfiguration configuration)
    : _configuration(configuration) {
  const auto table_specifications = configuration.table_specifications;

  for (const auto& table_specification : table_specifications) {
    std::cout << "Loading table " << table_specification.table_name << std::endl;
    auto table = load_table(table_specification.table_path, 100000);
    std::cout << "Loaded table " << table_specification.table_name << " successfully." << std::endl;

    ChunkEncodingSpec chunk_spec;

    for (const auto& column_specification : table_specification.columns) {
      auto column = column_specification.second;
      chunk_spec.push_back(column.encoding);
    }

    ChunkEncoder::encode_all_chunks(table, chunk_spec);
    StorageManager::get().add_table(table_specification.table_name, table);

    std::cout << "Encoded table " << table_specification.table_name << " successfully." << std::endl;
  }

  const auto tables = opossum::TpchDbGenerator(0.01f, 100000).generate();

  for (auto& tpch_table : tables) {
    const auto& table_name = opossum::tpch_table_names.at(tpch_table.first);
    auto& table = tpch_table.second;

    ChunkEncodingSpec chunk_spec;

    for (size_t idx = 0; idx < table->column_count(); idx++) {
      chunk_spec.push_back(SegmentEncodingSpec{});
    }

    ChunkEncoder::encode_all_chunks(table, chunk_spec);
    StorageManager::get().add_table(table_name, table);

    std::cout << "Encoded table " << table_name << " successfully." << std::endl;
  }
}

void CostModelCalibration::run_tpch() const {
  const auto scheduler = std::make_shared<NodeQueueScheduler>();
  CurrentScheduler::set(scheduler);

  for (size_t i = 0; i < 1; i++) {
    for (const auto& query : opossum::tpch_queries) {
      std::vector<CalibrationExample> examples{};

      SQLQueryCache<SQLQueryPlan>::get().clear();

      auto pipeline_builder = SQLPipelineBuilder{query.second};
      pipeline_builder.disable_mvcc();
      pipeline_builder.dont_cleanup_temporaries();
      auto pipeline = pipeline_builder.create_pipeline();

      // Execute the query, we don't care about the results
      pipeline.get_result_table();

      auto query_plans = pipeline.get_query_plans();
      for (const auto& query_plan : query_plans) {
        for (const auto& root : query_plan->tree_roots()) {
          _traverse(root, examples);
        }
      }
      std::cout << "Finished TPCH " << query.first << std::endl;

      auto output_path = _configuration.tpch_output_path + "_" + std::to_string(query.first);
      _write_result_csv(output_path, _configuration, examples);
    }
  }
}

void CostModelCalibration::calibrate() const {
  std::vector<CalibrationExample> examples{};
  auto number_of_iterations = _configuration.calibration_runs;

  const auto scheduler = std::make_shared<NodeQueueScheduler>();
  CurrentScheduler::set(scheduler);

  for (size_t i = 0; i < number_of_iterations; i++) {
    // Regenerate Queries for each iteration...
    auto queries = CalibrationQueryGenerator::generate_queries(_configuration.table_specifications);

    for (const auto& query : queries) {
      std::cout << query << std::endl;
      SQLQueryCache<SQLQueryPlan>::get().clear();

      auto pipeline_builder = SQLPipelineBuilder{query};
      pipeline_builder.disable_mvcc();
      pipeline_builder.dont_cleanup_temporaries();
      auto pipeline = pipeline_builder.create_pipeline();

      // Execute the query, we don't care about the results
      pipeline.get_result_table();

      auto query_plans = pipeline.get_query_plans();
      for (const auto& query_plan : query_plans) {
        for (const auto& root : query_plan->tree_roots()) {
          _traverse(root, examples);
        }
      }
    }
    std::cout << "Finished iteration " << i << std::endl;
  }

  _write_result_csv(_configuration.output_path, _configuration, examples);
}

void CostModelCalibration::_write_result_csv(const std::string output_path, const nlohmann::json& configuration,
                                             const std::vector<CalibrationExample>& examples) const {
  std::vector<std::string> columns{};

  const auto calibration_features = CalibrationFeatures::columns;
  const auto hardware_features = CalibrationConstantHardwareFeatures::columns;
  const auto runtime_features = CalibrationRuntimeHardwareFeatures::columns;
  const auto join_features = CalibrationJoinFeatures::columns;
  const auto projection_features = CalibrationProjectionFeatures::columns;
  const auto table_scan_features = CalibrationTableScanFeatures::columns;

  columns.insert(std::end(columns), std::begin(calibration_features), std::end(calibration_features));
  columns.insert(std::end(columns), std::begin(hardware_features), std::end(hardware_features));
  columns.insert(std::end(columns), std::begin(runtime_features), std::end(runtime_features));
  columns.insert(std::end(columns), std::begin(join_features), std::end(join_features));
  columns.insert(std::end(columns), std::begin(projection_features), std::end(projection_features));
  columns.insert(std::end(columns), std::begin(table_scan_features), std::end(table_scan_features));

  std::ofstream stream;
  stream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  stream.open(output_path, std::ios::out);

  const auto header = boost::algorithm::join(columns, ", ");
  stream << header << '\n';
  stream.close();

  CsvWriter writer(output_path);

  for (const auto& example : examples) {
    const auto all_type_variants = serialize(example);

    for (const auto& value : all_type_variants) {
      writer.write(value);
    }
    writer.end_line();
  }

  // TODO(Sven): Append if file already exists.. incremental logging
  std::cout << "Writing result CSV" << std::endl;
}

void CostModelCalibration::_traverse(const std::shared_ptr<const AbstractOperator>& op,
                                     std::vector<CalibrationExample>& examples) const {
  if (op->input_left() != nullptr) {
    _traverse(op->input_left(), examples);
  }

  if (op->input_right() != nullptr) {
    _traverse(op->input_right(), examples);
  }

  auto operator_result = CostModelFeatureExtractor::extract_features(op);
  examples.push_back(operator_result);
}

}  // namespace opossum
