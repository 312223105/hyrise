#pragma once

#include <memory>

#include "base_segment_statistics2.hpp"
#include "selectivity.hpp"

namespace opossum {

template <typename T>
class AbstractHistogram;
class AbstractStatisticsObject;
template <typename T>
class EqualDistinctCountHistogram;
template <typename T>
class EqualWidthHistogram;
template <typename T>
class GenericHistogram;

template <typename T>
class SegmentStatistics2 : public BaseSegmentStatistics2 {
 public:
  void set_statistics_object(const std::shared_ptr<AbstractStatisticsObject>& statistics_object);
  std::shared_ptr<BaseSegmentStatistics2> scale_with_selectivity(const Selectivity selectivity) const override;
  std::shared_ptr<BaseSegmentStatistics2> slice_with_predicate(
      const PredicateCondition predicate_type, const AllTypeVariant& variant_value,
      const std::optional<AllTypeVariant>& variant_value2 = std::nullopt) const override;

  std::shared_ptr<EqualDistinctCountHistogram<T>> equal_distinct_count_histogram;
  std::shared_ptr<EqualWidthHistogram<T>> equal_width_histogram;
  std::shared_ptr<GenericHistogram<T>> generic_histogram;
};

}  // namespace opossum
