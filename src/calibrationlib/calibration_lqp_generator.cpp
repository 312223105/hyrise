#include "calibration_lqp_generator.hpp"
#include <expression/expression_functional.hpp>
#include <logical_query_plan/join_node.hpp>
#include "logical_query_plan/predicate_node.hpp"

#include <synthetic_table_generator.hpp>
#include "storage/table.hpp"

using opossum::expression_functional::between_inclusive_;
using opossum::expression_functional::equals_;
using opossum::expression_functional::greater_than_;
using opossum::expression_functional::is_not_null_;
using opossum::expression_functional::is_null_;
using opossum::expression_functional::less_than_;
using opossum::expression_functional::like_;
using opossum::expression_functional::not_in_;

namespace opossum {
void CalibrationLQPGenerator::generate(OperatorType operator_type,
                                       const std::shared_ptr<const CalibrationTableWrapper>& table) {
  switch (operator_type) {
    case OperatorType::TableScan:
      _generate_table_scans(table);
      return;
    default:
      break;
  }

  Fail("Not implemented yet: Only TableScans are currently supported");
}

void CalibrationLQPGenerator::_generate_table_scans
(const std::shared_ptr<const CalibrationTableWrapper>& table_wrapper) {
  // selectivity resolution determines in how many LQPs with a different selectivity are generated
  // increase this value for providing more training data to the model
  // The resulting LQPs are equally distributed from 0% to 100% selectivity.
  constexpr uint32_t selectivity_resolution = 10;

  // for every table scan gets executed on the raw data as well as referenceSegments
  // reference_scan_selectivity_resolution determines how many of these scans on referenceSegments are generated
  // in addition to th raw scan.
  // The resulting referenceSegments reduce the selectivity of the original scan by 0% to 100% selectivity
  // in equally sized steps.
  constexpr uint32_t reference_scan_selectivity_resolution = 10;

  const auto stored_table_node = StoredTableNode::make(table_wrapper->get_name());

  const auto column_count = table_wrapper->get_table()->column_count();
  const std::vector<std::string> column_names = table_wrapper->get_table()->column_names();
  const auto column_data_types = table_wrapper->get_table()->column_data_types();

  for (ColumnID column_id = ColumnID{0}; column_id < column_count; ++column_id) {
    // Column specific values
    const auto column = stored_table_node->get_column(column_names[column_id]);
    const auto distribution = table_wrapper->get_column_data_distribution(column_id);
    const auto step_size = (distribution.max_value - distribution.min_value) / selectivity_resolution;

    if (_enable_column_vs_column_scans) {
      _generate_column_vs_column_scans(table_wrapper);
    }

    for (uint32_t selectivity_step = 0; selectivity_step < selectivity_resolution; selectivity_step++) {
      // in this for-loop we iterate up and go from 100% selectivity to 0% by increasing the lower_bound in steps
      // for any step there
      resolve_data_type(column_data_types[column_id], [&](const auto column_data_type) {
        using ColumnDataType = typename decltype(column_data_type)::type;

        // Get value for current iteration
        // the cursor is a int representation of where the column has to be cut in this iteration
        const auto step_cursor = static_cast<uint32_t>(selectivity_step * step_size);

        const ColumnDataType lower_bound = SyntheticTableGenerator::generate_value<ColumnDataType>(step_cursor);

        auto get_predicate_node_based_on = [column, lower_bound](const std::shared_ptr<AbstractLQPNode>& base) {
          return PredicateNode::make(greater_than_(column, lower_bound), base);
        };

        // Baseline
        _generated_lpqs.emplace_back(get_predicate_node_based_on(std::shared_ptr<AbstractLQPNode>(stored_table_node)));

        // Add reference scans
        if (_enable_reference_scans) {
          // generate reference scans to base original LQP on
          // that reduce the overall selectivity stepwise
          const double reference_scan_step_size =
                  (distribution.max_value - step_cursor) / reference_scan_selectivity_resolution;
          for (uint32_t step = 0; step < reference_scan_selectivity_resolution; step++) {
            const ColumnDataType upper_bound = SyntheticTableGenerator::generate_value<ColumnDataType>(
                static_cast<uint32_t>(step * reference_scan_step_size));
            _generated_lpqs.emplace_back(
                get_predicate_node_based_on(PredicateNode::make(less_than_(column, upper_bound), stored_table_node)));
            if (_enable_between_predicates) {
              _generated_lpqs.emplace_back(
                  PredicateNode::make(between_inclusive_(column, lower_bound, upper_bound), stored_table_node));
            }
          }
          // add reference scan with full pos list
          // TODO(ramboldio) ensure full and empty pos list also for NULL values
          _generated_lpqs.emplace_back(
              get_predicate_node_based_on(PredicateNode::make(is_not_null_(column), stored_table_node)));
          // add reference scan with empty pos list
          _generated_lpqs.emplace_back(
              get_predicate_node_based_on(PredicateNode::make(is_null_(column), stored_table_node)));
        }

        // LIKE and IN predicates for strings
        if (_enable_like_predicates && std::is_same<ColumnDataType, std::string>::value) {
          for (uint32_t step = 0; step < 10; step++) {
            auto const upper_bound = (SyntheticTableGenerator::generate_value<pmr_string>(step));
            _generated_lpqs.emplace_back(PredicateNode::make(like_(column, upper_bound + "%"), stored_table_node));
          }

          // IN
          _generated_lpqs.emplace_back(PredicateNode::make(not_in_(column, "not_there"), stored_table_node));

          // 100% selectivity
          _generated_lpqs.emplace_back(PredicateNode::make(like_(column, "%"), stored_table_node));
          // 0% selectivity
          _generated_lpqs.emplace_back(PredicateNode::make(like_(column, "%not_there%"), stored_table_node));
        }
      });
    }
  }
}

std::vector<CalibrationLQPGenerator::ColumnPair> CalibrationLQPGenerator::_get_column_pairs(
    const std::shared_ptr<const CalibrationTableWrapper>& table) const {
  /*
       * ColumnVsColumn Scans occur when the value of a predicate is a column.
       * In this case every value from one column has to be compared to every value of the other
       * making this operation somewhat costly and therefore requiring a dedicated test case.
       *
       * We implement this creating a ColumnVsColumn scan in between all the columns with same data type
       */
  const auto column_definitions = table->get_table()->column_definitions();
  auto column_comparison_pairs = std::vector<ColumnPair>();

  auto unmatched_columns = std::vector<TableColumnDefinition>();

  for (const auto& column : column_definitions) {
    // TODO(ramboldio) simplify loop / ensure that first and second element are not the same
    bool matched = false;
    auto single_iterator = unmatched_columns.begin();
    while (single_iterator < unmatched_columns.end()) {
      if (column.data_type == single_iterator->data_type) {
        matched = true;
        column_comparison_pairs.emplace_back(ColumnPair(single_iterator->name, column.name));
        unmatched_columns.erase(single_iterator);
        break;
      }
      single_iterator++;
    }
    if (!matched) {
      unmatched_columns.emplace_back(column);
    }
  }
  return column_comparison_pairs;
}

void CalibrationLQPGenerator::_generate_column_vs_column_scans(
    const std::shared_ptr<const CalibrationTableWrapper>& table) {
  // TODO(ramboldio) include statement of purpose like (ColumnVsColumn scans are an expensive special case)
  const auto stored_table_node = StoredTableNode::make(table->get_name());
  const auto column_vs_column_scan_pairs = _get_column_pairs(table);

  for (const ColumnPair& pair : column_vs_column_scan_pairs) {
    _generated_lpqs.emplace_back(PredicateNode::make(
        greater_than_(stored_table_node->get_column(pair.first), stored_table_node->get_column(pair.second)),
        stored_table_node));
  }
}

CalibrationLQPGenerator::CalibrationLQPGenerator() {
  _generated_lpqs = std::vector<std::shared_ptr<AbstractLQPNode>>();
}

const std::vector<std::shared_ptr<AbstractLQPNode>>& CalibrationLQPGenerator::get_lqps() { return _generated_lpqs; }
}  // namespace opossum
