#include <fstream>
#include <iomanip>
#include <iostream>

#include "benchmark_config.hpp"
#include "benchmark_runner.hpp"
#include "constant_mappings.hpp"
#include "expression/abstract_predicate_expression.hpp"
#include "expression/expression_utils.hpp"
#include "expression/lqp_column_expression.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "operators/operator_scan_predicate.hpp"
#include "sql/sql_plan_cache.hpp"
#include "statistics/table_statistics.hpp"
#include "storage/create_iterable_from_segment.hpp"
#include "storage/storage_manager.hpp"
#include "tpch/tpch_query_generator.hpp"
#include "tpch/tpch_table_generator.hpp"
#include "types.hpp"

#include <boost/bimap.hpp>
#include "benchmark_config.hpp"
#include "benchmark_runner.hpp"
#include "boost/functional/hash.hpp"
#include "expression/abstract_predicate_expression.hpp"
#include "expression/lqp_column_expression.hpp"
#include "logical_query_plan/join_node.hpp"
#include "storage/index/group_key/group_key_index.hpp"
#include "logical_query_plan/lqp_utils.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "operators/operator_join_predicate.hpp"
#include "operators/operator_scan_predicate.hpp"
#include "sql/sql_pipeline_builder.hpp"
#include "sql/sql_plan_cache.hpp"
#include "storage/index/base_index.hpp"
#include "storage/storage_manager.hpp"
#include "tpch/tpch_query_generator.hpp"
#include "tpch/tpch_table_generator.hpp"
#include "types.hpp"
#include "utils/load_table.hpp"
#include "utils/plugin_manager.hpp"

using namespace opossum;  // NOLINT

constexpr auto SCALE_FACTOR = 1.0f;

typedef boost::bimap<std::string, uint16_t> table_name_id_bimap;
typedef table_name_id_bimap::value_type table_name_id;

table_name_id_bimap table_name_id_map;

using TableColumnIdentifier = std::pair<uint16_t, ColumnID>;

using TableIdentifierMap = std::map<std::string, uint16_t>;

// maps an attribute to an identifier in the form of `table_idenfier_column_id`
using AttributeIdentifierMap = std::map<std::pair<std::string, std::string>, std::string>;

namespace std {
template <>
struct hash<TableColumnIdentifier> {
  size_t operator()(TableColumnIdentifier const& v) const {
    const std::string combined = std::to_string(v.first) + "_" + std::to_string(v.second);
    return std::hash<std::string>{}(combined);
  }
};
}  // namespace std

// std::unordered_map<TableColumnIdentifier, std::string> attribute_id_name_map;

// struct TableColumnInformation {
//   size_t accesses;
//   std::vector<std::chrono::nanoseconds> execution_times;
//   std::vector<size_t> input_rows;
//   std::vector<size_t> output_rows;
//   std::vector<std::time_t> timestamps;
// };

// class AbstractCandidate {
// public:
//   TableColumnIdentifier tcid;

//   AbstractCandidate(TableColumnIdentifier tcid) : tcid(tcid) {}
// };

// class IndexCandidate : public AbstractCandidate {
// public:
//   IndexCandidate(TableColumnIdentifier tcid_sub) : AbstractCandidate(tcid_sub) {}
// };

// class AbstractCandidateAssessment {
// public:
//   std::shared_ptr<AbstractCandidate> candidate;
//   float desirability = 0.0f;
//   float cost = 0.0f;
// };

// class IndexCandidateAssessment : public AbstractCandidateAssessment {
// public:
//   IndexCandidateAssessment(const std::shared_ptr<IndexCandidate> candidate_2, const float desirability_2, const float cost_2) : AbstractCandidateAssessment(candidate_2, desirability_2, cost_2) {}
// };

// Todo: what is wrong with customer.c_nationkey
// Todo: dont clean up performance data on clean up
// Todo: executing the same query more than once, is it cached?
// std::unordered_map<TableColumnIdentifier, TableColumnInformation> scan_map;
// std::unordered_map<TableColumnIdentifier, TableColumnInformation> join_map;

// using TableIdentifierMap = std::map<std::string, uint16_t>;

// // maps an attribute to an identifier in the form of `table_idenfier_column_id`
// using AttributeIdentifierMap = std::map<std::pair<std::string, std::string>, std::string>;

// template <typename Iter>
// std::string join(Iter begin, Iter end, std::string const& separator) {
//   std::ostringstream result;
//   if (begin != end) result << *begin++;
//   while (begin != end) result << separator << *begin++;
//   return result.str();
// }

// void update_map(std::unordered_map<TableColumnIdentifier, TableColumnInformation>& map, const TableColumnIdentifier& identifier, const OperatorPerformanceData& perf_data, const bool input_rows_left = true) {
//   const auto search = map.find(identifier);
//   if (search != map.cend()) {
//     map[identifier].accesses++;
//     map[identifier].execution_times.emplace_back(perf_data.walltime);
//     if (input_rows_left)
//       map[identifier].input_rows.emplace_back(perf_data.input_rows_left);
//     else
//       map[identifier].input_rows.emplace_back(perf_data.input_rows_right);
//     map[identifier].output_rows.emplace_back(perf_data.output_rows);
//     map[identifier].timestamps.emplace_back(perf_data.timestamp);
//   } else {
//     TableColumnInformation tci{1,
//                                {{perf_data.walltime}},
//                                {perf_data.input_rows_left},
//                                {perf_data.output_rows},
//                                {perf_data.timestamp}};
//     map.emplace(identifier, tci);
//   } 
// }

// void process_pqp(std::shared_ptr<const opossum::AbstractOperator> op) {
//   // TODO: handle diamonds
//   // Todo: handle index scans/joins
//   if (op->type() == OperatorType::TableScan) {
//     const auto node = op->lqp_node();
//     const auto predicate_node = std::dynamic_pointer_cast<const PredicateNode>(node);
//     const auto operator_predicates = OperatorScanPredicate::from_expression(*predicate_node->predicate(), *node);
    
//     if (operator_predicates->size() < 2) {
//       for (const auto& el : node->node_expressions) {
//         visit_expression(el, [&](const auto& expression) {
//           if (expression->type == ExpressionType::LQPColumn) {
//             const auto column_expression = std::dynamic_pointer_cast<LQPColumnExpression>(expression);
//             const auto column_reference = column_expression->column_reference;
//             const auto original_node = column_reference.original_node();

//             if (original_node->type == LQPNodeType::StoredTable) {
//               const auto stored_table_node = std::dynamic_pointer_cast<const StoredTableNode>(original_node);
//               const auto& table_name = stored_table_node->table_name;
//               const auto table_id = table_name_id_map.left.at(table_name);

//               const auto original_column_id = column_reference.original_column_id();
//               const auto identifier = std::make_pair(table_id, original_column_id);
//               const auto& perf_data = op->performance_data();

//               update_map(scan_map, identifier, perf_data);
//             }
//           }
//           return ExpressionVisitation::VisitArguments;
//         });
//       }
//     }
//   } else if (op->type() == OperatorType::JoinHash || op->type() == OperatorType::JoinMPSM ||
//              op->type() == OperatorType::JoinNestedLoop || op->type() == OperatorType::JoinSortMerge) {
//     const auto node = op->lqp_node();
//     const auto join_node = std::dynamic_pointer_cast<const JoinNode>(node);

//     if (join_node->join_mode == JoinMode::Inner) {
//       const auto operator_predicate = OperatorJoinPredicate::from_expression(*(join_node->node_expressions[0]),
//                                                                              *node->left_input(), *node->right_input());
//       if (operator_predicate.has_value()) {
//         if ((*operator_predicate).predicate_condition == PredicateCondition::Equals) {
//           const auto column_expressions = join_node->column_expressions();
//           const auto predicate_expression = std::dynamic_pointer_cast<const AbstractPredicateExpression>(join_node->node_expressions[0]);
//           std::string table_name_0, table_name_1;
//           ColumnID original_column_id_0, original_column_id_1;

//           for (const auto& column_expression : column_expressions) {
//             if (*column_expression == *(predicate_expression->arguments[0])) {
//               if (column_expression->type == ExpressionType::LQPColumn) {
//                 const auto column_reference = std::dynamic_pointer_cast<LQPColumnExpression>(column_expression)->column_reference;
//                 original_column_id_0 = column_reference.original_column_id();

//                 const auto original_node_0 = column_reference.original_node();
//                 if (original_node_0->type == LQPNodeType::StoredTable) {
//                   const auto stored_table_node_0 = std::dynamic_pointer_cast<const StoredTableNode>(original_node_0);
//                   table_name_0 = stored_table_node_0->table_name;
//                 }
//               }
//             }

//             if (*column_expression == *(predicate_expression->arguments[1])) {
//               if (column_expression->type == ExpressionType::LQPColumn) {
//                 const auto column_reference = std::dynamic_pointer_cast<LQPColumnExpression>(column_expression)->column_reference;
//                 original_column_id_1 = column_reference.original_column_id();
                
//                 const auto original_node_1 = column_reference.original_node();
//                 if (original_node_1->type == LQPNodeType::StoredTable) {
//                   const auto stored_table_node_1 = std::dynamic_pointer_cast<const StoredTableNode>(original_node_1);
//                   table_name_1 = stored_table_node_1->table_name;
//                 }
//               }
//             }
//           }
//           const auto& perf_data = op->performance_data();

//           auto table_id = table_name_id_map.left.at(table_name_0);
//           auto identifier = std::make_pair(table_id, original_column_id_0);
//           update_map(join_map, identifier, perf_data);

//           // How do we know whether the left_input_rows are actually added to the left table?
//           table_id = table_name_id_map.left.at(table_name_1);
//           identifier = std::make_pair(table_id, original_column_id_1);
//           update_map(join_map, identifier, perf_data, false);
//         }
//       }
//     }
//   } else {
//     std::cout << "Skipping this node. Not a Scan or Join." << std::endl;
//   }
//   if (op->input_left()) process_pqp(op->input_left());

//   if (op->input_right()) process_pqp(op->input_right());
// }

// std::string table_name_from_TCID(const TableColumnIdentifier& tcid) {
//   const auto table_id = tcid.first;
//   const auto table_name = table_name_id_map.right.at(table_id);

//   return table_name;
// }

// std::string attribute_name_from_TCID(const TableColumnIdentifier& tcid) {
//   const auto attribute_name = attribute_id_name_map[tcid];

//   return attribute_name;
// }

// std::string TCID_to_string(const TableColumnIdentifier& tcid) {
//   const std::string result = table_name_from_TCID(tcid) + "." + attribute_name_from_TCID(tcid);

//   return result;
// }

// void print_operator_map(std::unordered_map<TableColumnIdentifier, TableColumnInformation>& map) {
//   for (const auto& elem : map) {
//     const auto table_id = elem.first.first;
//     const auto attribute_id = elem.first.second;
//     const auto identifier_pair = std::make_pair(table_id, attribute_id);
//     const auto attribute_name = attribute_id_name_map[identifier_pair];
//     const auto table_name = table_name_id_map.right.at(table_id);

//     const auto& table_column_information = elem.second;

//     const auto total_processed_rows = std::accumulate(table_column_information.input_rows.rbegin(), table_column_information.input_rows.rend(), 0);
//     const auto total_output_rows = std::accumulate(table_column_information.output_rows.rbegin(), table_column_information.output_rows.rend(), 0);
//     const float avg_selectivity = static_cast<float>(total_output_rows) / total_processed_rows;
//     const auto avg_processed_rows = total_processed_rows / table_column_information.input_rows.size();

//     std::string output         = "";
//     const auto input_rows_str  = join(table_column_information.input_rows.cbegin(), table_column_information.input_rows.cend(), ", ");
//     const auto output_rows_str = join(table_column_information.output_rows.cbegin(), table_column_information.output_rows.cend(), ", ");

//     const auto table = StorageManager::get().get_table(table_name);
//     size_t predicted_index_size = 0;
//     for (const auto& chunk : table->chunks()) {
//       const auto segment = chunk->get_segment(attribute_id);
//       const auto dict_segment = std::dynamic_pointer_cast<const BaseDictionarySegment>(segment);
//       const auto unique_values = dict_segment->unique_values_count();
//       predicted_index_size +=
//           BaseIndex::estimate_memory_consumption(SegmentIndexType::GroupKey, chunk->size(), unique_values, /*Last Parameter is ignored for GroupKeyIndex*/1);
//     }

//     const auto index_size_processed_rows_ratio = static_cast<float>(predicted_index_size) / total_processed_rows;
//     // const auto index_size_avg_rows_ratio = static_cast<float>(predicted_index_size) / avg_processed_rows;

//     output += table_name + "." + attribute_name + ":\n";
//     output += "Accesses:              " + std::to_string(table_column_information.accesses) + "\n";
//     // output += "Input rows: "            + input_rows_str + "\n";
//     // output += "Output rows: "           + output_rows_str + "\n";
//     output += "AVG Selectivity:       " + std::to_string(avg_selectivity) + "\n";
//     output += "Total processed rows:  " + std::to_string(total_processed_rows) + "\n";
//     output += "AVG proccesed rows:    " + std::to_string(avg_processed_rows) + "\n";
//     output += "Predicted Index Size:  " + std::to_string(predicted_index_size) + "\n";
//     output += "Index bytes per row:   " + std::to_string(index_size_processed_rows_ratio) + "\n";
//     // output += "Index bytes per AVG:   " + std::to_string(index_size_avg_rows_ratio) + "\n";
//     output += "\n";

//     std::cout << output;
//   }
// }

// size_t predict_index_size(const TableColumnIdentifier& tcid) {
//   const auto table_id = tcid.first;
//   const auto attribute_id = tcid.second;
//   const auto table_name = table_name_id_map.right.at(table_id);

//   const auto table = StorageManager::get().get_table(table_name);
//   size_t index_size = 0;

//   for (const auto& chunk : table->chunks()) {
//     const auto segment = chunk->get_segment(attribute_id);
//     const auto dict_segment = std::dynamic_pointer_cast<const BaseDictionarySegment>(segment);
//     const auto unique_values = dict_segment->unique_values_count();
//     index_size +=
//         BaseIndex::estimate_memory_consumption(SegmentIndexType::GroupKey, chunk->size(), unique_values, /*Last Parameter is ignored for GroupKeyIndex*/1);
//   }

//   return index_size;
// }

// // This enumerator considers all columns except these of tables that have less than 10'000 * SCALE_FACTOR rows
// std::vector<IndexCandidate> enumerate_index_candidates() {
//   std::vector<IndexCandidate> index_candidates;

//   uint16_t next_table_id = 0;
//   for (const auto& table_name : StorageManager::get().table_names()) {
//     const auto table = StorageManager::get().get_table(table_name);
//     if (table->row_count() >= (10'000 * SCALE_FACTOR)) {
//       ColumnID next_attribute_id{0};
//       for ([[maybe_unused]] const auto& column_def : table->column_definitions()) {
//         const TableColumnIdentifier identifier = std::make_pair(next_table_id, next_attribute_id);
//         next_attribute_id++;
//         index_candidates.emplace_back(identifier);
//       }
//     } else {
//       std::cout << "Not considering columns of: " << table_name << " as candidates." << std::endl;
//     }

//     ++next_table_id;
//   }

//   return index_candidates;
// }

// // This evaluator assigns a desirability according to the number of processed rows of this column
// std::vector<AbstractCandidateAssessment> assess_index_candidates(std::vector<IndexCandidate>& index_candidates) {
//   std::vector<AbstractCandidateAssessment> index_candidate_assessments;

//   for (auto& index_candidate : index_candidates) {
//     const auto table_column_information = scan_map[index_candidate.tcid];
//     const auto total_processed_rows = std::accumulate(table_column_information.input_rows.rbegin(), table_column_information.input_rows.rend(), 0);

//     index_candidate_assessments.emplace_back(std::make_shared<IndexCandidate>(index_candidate), static_cast<float>(total_processed_rows), static_cast<float>(predict_index_size(index_candidate.tcid)));
//   }

//   return index_candidate_assessments;
// }

// bool compareByDesirabilityPerCost(const AbstractCandidateAssessment& assessment_1, const AbstractCandidateAssessment& assessment_2) {
//   const auto desirability_per_cost_1 = assessment_1.desirability / assessment_1.cost;
//   const auto desirability_per_cost_2 = assessment_2.desirability / assessment_2.cost;

//   return desirability_per_cost_1 > desirability_per_cost_2;
// }

// // This selector greedily selects assessed items based on desirability per cost
// std::vector<AbstractCandidate> select_assessments_greedy(std::vector<AbstractCandidateAssessment>& assessments, size_t budget) {
//   std::vector<AbstractCandidate> selected_candidates;
//   size_t used_budget = 0;

//   auto assessments_copy = assessments;
//   std::sort(assessments_copy.begin(), assessments_copy.end(), compareByDesirabilityPerCost);

//   for (const auto& assessment : assessments_copy) {
//     if (assessment.cost + used_budget < budget) {
//       selected_candidates.emplace_back(*(assessment.candidate));
//       used_budget += static_cast<size_t>(assessment.cost);
//     }
//   }

//   return selected_candidates;
// }

void extract_meta_data(TableIdentifierMap& table_name_table_id_to_map,
                       AttributeIdentifierMap& attribute_to_attribute_id_map) {
  std::ofstream table_meta_data_csv_file("table_meta_data.csv");
  table_meta_data_csv_file << "TABLE_ID,TABLE_NAME,ROW_COUNT,MAX_CHUNK_SIZE\n";

  std::ofstream attribute_meta_data_csv_file("attribute_meta_data.csv");
  attribute_meta_data_csv_file << "ATTRIBUTE_ID,TABLE_NAME,COLUMN_NAME,DATA_TYPE,IS_NULLABLE\n";

  std::ofstream segment_meta_data_csv_file("segment_meta_data.csv");
  segment_meta_data_csv_file << "ATTRIBUTE_ID,TABLE_NAME,COLUMN_NAME,CHUNK_ID,ENCODING,COMPRESSION,SIZE\n";

  uint16_t next_table_id = 0;
  uint16_t next_attribute_id = 0;

  for (const auto& table_name : StorageManager::get().table_names()) {
    const auto& table = StorageManager::get().get_table(table_name);

    // might be an overkill here?
    const auto table_id = next_table_id;
    const auto& [table_iter, table_inserted] = table_name_table_id_to_map.try_emplace(table_name, table_id);
    if (table_inserted) {
      ++next_table_id;
    }

    table_meta_data_csv_file << table_id << "," << table_name << "," << table->row_count() << ","
                             << table->max_chunk_size() << std::endl;

    next_attribute_id = 0;
    for (const auto& column_def : table->column_definitions()) {
      const auto& column_name = column_def.name;

      std::stringstream attr_table_id;
      attr_table_id << table_id << "_" << next_attribute_id;
      attribute_to_attribute_id_map.emplace(std::make_pair(table_name, column_name), attr_table_id.str());
      ++next_attribute_id;

      attribute_meta_data_csv_file << attr_table_id.str() << "," << table_name << "," << column_name << ","
                                   << data_type_to_string.left.at(column_def.data_type) << ","
                                   << (column_def.nullable ? "TRUE" : "FALSE") << "\n";

      for (auto chunk_id = ChunkID{0}, end = table->chunk_count(); chunk_id < end; ++chunk_id) {
        const auto& chunk = table->get_chunk(chunk_id);
        const auto column_id = table->column_id_by_name(column_name);
        const auto& segment = chunk->get_segment(column_id);

        const auto encoded_segment = std::dynamic_pointer_cast<const BaseEncodedSegment>(segment);
        const auto encoding_type = encoded_segment->encoding_type();

        segment_meta_data_csv_file << attr_table_id.str() << "," << table_name << "," << column_name << "," << chunk_id << "," << encoding_type_to_string.left.at(encoding_type) << ",";

        if (encoded_segment->compressed_vector_type()) {
          switch (*encoded_segment->compressed_vector_type()) {
            case CompressedVectorType::FixedSize4ByteAligned: {
              segment_meta_data_csv_file << "FixedSize4ByteAligned";
              break;
            }
            case CompressedVectorType::FixedSize2ByteAligned: {
              segment_meta_data_csv_file << "FixedSize2ByteAligned";
              break;
            }
            case CompressedVectorType::FixedSize1ByteAligned: {
              segment_meta_data_csv_file << "FixedSize1ByteAligned";
              break;
            }
            case CompressedVectorType::SimdBp128: {
              segment_meta_data_csv_file << "SimdBp128";
              break;
            }
            default:
              segment_meta_data_csv_file << "NONE";
          }
        }

        segment_meta_data_csv_file << "," << encoded_segment->estimate_memory_usage() << "\n";
      }
    }
  }

  table_meta_data_csv_file.close();
  attribute_meta_data_csv_file.close();
  segment_meta_data_csv_file.close();
}

void extract_physical_query_plan_cache_data() {
  std::ofstream plan_cache_csv_file("plan_cache.csv");
  plan_cache_csv_file << "QUERY_HASH,EXECUTION_COUNT,QUERY_STRING\n";

  for (const auto& [query_string, physical_query_plan] : SQLPhysicalPlanCache::get()) {
    size_t frequency = SQLPhysicalPlanCache::get().get_frequency(query_string);

    // std::cout << "=== Q (" << query_string.substr(0, 60) << " ...)" << std::endl;

    std::stringstream query_hex_hash;
    query_hex_hash << std::hex << std::hash<std::string>{}(query_string);

    auto query_string2 = query_string;
    query_string2.erase(std::remove(query_string2.begin(), query_string2.end(), '\n'), query_string2.end());

    plan_cache_csv_file << query_hex_hash.str() << "," << frequency << ",\"" << query_string2 << "\"\n";
  }

  plan_cache_csv_file.close();
}

int main(int argc, const char* argv[]) {
  auto config = BenchmarkConfig::get_default_config();
  config.max_num_query_runs = 5;
  config.enable_visualization = false;
  config.output_file_path = "perf.json";
  config.chunk_size = 1'000'000;
  config.cache_binary_tables = true;
  
  // const std::vector<QueryID> tpch_query_ids = {QueryID{0},  QueryID{1},  QueryID{2},  QueryID{3},  QueryID{4},
  //                                              QueryID{5},  QueryID{6},  QueryID{7},  QueryID{8},  QueryID{3},
  //                                              QueryID{10}, QueryID{11}, QueryID{12}, QueryID{13}, QueryID{14},
  //                                              QueryID{15}, QueryID{16}, QueryID{17}, QueryID{18}};
  // Indexes applicable


  // const std::vector<QueryID> tpch_query_ids = {QueryID{0},  QueryID{1},  QueryID{2},  QueryID{3},  QueryID{4},
  //                                              QueryID{5},  QueryID{6},  QueryID{7},  QueryID{9},
  //                                              QueryID{11}, QueryID{13}, QueryID{14},
  //                                              QueryID{15}, QueryID{16}, QueryID{17}, QueryID{18}};

  // auto br = std::make_shared<BenchmarkRunner>(config, std::make_unique<TPCHQueryGenerator>(false, SCALE_FACTOR, tpch_query_ids),
  //                 std::make_unique<TpchTableGenerator>(SCALE_FACTOR, std::make_shared<BenchmarkConfig>(config)),
  //                 100'000);

  // for (const auto& [table_name, table] : StorageManager::get().tables()) {
  //   for (ColumnID column_id = ColumnID{0}; column_id < table->column_count(); column_id++) {
  //     table->create_index<GroupKeyIndex>({column_id}, "idx" + std::to_string(column_id));
  //   }
  // }
  // br->run();
  // TODO andere lineitem spalte mit weniger distinkten elementen filtern (danach vielleicht groupen), damit weniger gepruned werden kann als bei equi auf comment
  // visualize current plan
// TODO avg wieder rein
  // TODO queries tauschen

  const std::filesystem::path plugin_path(argv[1]);
  const auto plugin_name = plugin_name_from_path(plugin_path);

  // const std::vector<QueryID> tpch_query_ids = {QueryID{6}};
  // auto br = std::make_shared<BenchmarkRunner>(config, std::make_unique<TPCHQueryGenerator>(false, SCALE_FACTOR, tpch_query_ids),
  //                 std::make_unique<TpchTableGenerator>(SCALE_FACTOR, std::make_shared<BenchmarkConfig>(config)), 100'000);

  const std::vector<QueryID> tpch_query_ids = {QueryID{5}, QueryID{8}};

  BenchmarkRunner(config, std::make_unique<TPCHQueryGenerator>(false, SCALE_FACTOR, tpch_query_ids),
                  std::make_unique<TpchTableGenerator>(SCALE_FACTOR, std::make_shared<BenchmarkConfig>(config)),
                  100'000)
      .run();

  // StorageManager::get().add_benchmark_runner(br);
  // auto& query_gen = br->query_generator();
  // br->run();

  TableIdentifierMap table_name_table_id_to_map;
  AttributeIdentifierMap attribute_to_attribute_id_map;

  extract_meta_data(table_name_table_id_to_map, attribute_to_attribute_id_map);

  extract_physical_query_plan_cache_data();

  PluginManager::get().load_plugin(plugin_path);

  // std::this_thread::sleep_for(std::chrono::milliseconds(1'500));
  // br->rerun();
  // SQLPhysicalPlanCache::get().clear();
  // SQLLogicalPlanCache::get().clear();

  // br->runs(75);
  // query_gen->selected_queries({QueryID{13}, QueryID{22}});
  // br->rerun();

  // std::this_thread::sleep_for(std::chrono::milliseconds(1'500));
  // br->rerun();
  // SQLPhysicalPlanCache::get().clear();
  // SQLLogicalPlanCache::get().clear();

	  // br->runs(50);
	  // query_gen->selected_queries({QueryID{13}, QueryID{22}, QueryID{5}});
	  // br->rerun();

  // std::this_thread::sleep_for(std::chrono::milliseconds(1'500));
  // br->rerun();

  return 0;
}
