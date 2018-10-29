#pragma once

#include <memory>
#include <optional>

#include "all_type_variant.hpp"
#include "selectivity.hpp"

namespace opossum {

enum class PredicateCondition;

class BaseSegmentStatistics2 {
 public:
  virtual ~BaseSegmentStatistics2() = default;

  virtual std::shared_ptr<BaseSegmentStatistics2> scale_with_selectivity(const Selectivity selectivity) const = 0;
  virtual std::shared_ptr<BaseSegmentStatistics2> slice_with_predicate(
      const PredicateCondition predicate_type, const AllTypeVariant& variant_value,
      const std::optional<AllTypeVariant>& variant_value2 = std::nullopt) const = 0;

  virtual bool does_not_contain(const PredicateCondition predicate_type, const AllTypeVariant& variant_value,
                                const std::optional<AllTypeVariant>& variant_value2 = std::nullopt) const = 0;
};

}  // namespace opossum
