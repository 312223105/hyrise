#pragma once

#include <memory>
#include <string>

#include "base_non_query_node.hpp"

#include "enable_make_for_lqp_node.hpp"
#include "operators/abstract_operator.hpp"
#include "statistics/table_statistics.hpp"
#include "types.hpp"

namespace opossum {

/**
 * Node type to represent deleting a view from the StorageManager
 */
class DropViewNode : public EnableMakeForLQPNode<DropViewNode>, public BaseNonQueryNode {
 public:
  DropViewNode(const std::string& view_name, bool if_exists);

  std::string description() const override;

  const std::string view_name;
  const bool if_exists;

  OperatorType operator_type() const override { return OperatorType::DropView; }

 protected:
  size_t _shallow_hash() const override;
  std::shared_ptr<AbstractLQPNode> _on_shallow_copy(LQPNodeMapping& node_mapping) const override;
  bool _on_shallow_equals(const AbstractLQPNode& rhs, const LQPNodeMapping& node_mapping) const override;
};

}  // namespace opossum
