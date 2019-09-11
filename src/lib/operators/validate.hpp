#pragma once

#include <memory>
#include <string>
#include <vector>

#include "abstract_read_only_operator.hpp"
#include "types.hpp"
#include "utils/assert.hpp"

namespace opossum {

/**
 * Validates visibility of records of a table
 * within the context of a given transaction
 *
 * Assumption: Validate happens before joins.
 */
class Validate : public AbstractReadOnlyOperator {
 public:
  explicit Validate(const std::shared_ptr<AbstractOperator>& in);

  const std::string name() const override;

  // MVCC evaluation logic is exposed so that JitValidate can also use it
  static bool is_row_visible(TransactionID our_tid, CommitID snapshot_commit_id, const TransactionID row_tid,
                             const CommitID begin_cid, const CommitID end_cid);

 private:
  void _validate_chunks(const std::shared_ptr<const Table>& in_table, const ChunkID chunk_id_start,
                        const ChunkID chunk_id_end, const TransactionID our_tid, const TransactionID snapshot_commit_id,
                        std::vector<std::shared_ptr<Chunk>>& output_chunks, std::mutex& output_mutex);

 protected:
  std::shared_ptr<const Table> _on_execute(std::shared_ptr<TransactionContext> transaction_context) override;
  std::shared_ptr<const Table> _on_execute() override;
  std::shared_ptr<AbstractOperator> _on_deep_copy(
      const std::shared_ptr<AbstractOperator>& copied_input_left,
      const std::shared_ptr<AbstractOperator>& copied_input_right) const override;
  void _on_set_parameters(const std::unordered_map<ParameterID, AllTypeVariant>& parameters) override;

  bool _is_chunk_visible(const std::shared_ptr<const Chunk>& chunk, CommitID snapshot_commit_id);
};

}  // namespace opossum
