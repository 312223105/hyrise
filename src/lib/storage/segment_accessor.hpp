#pragma once

#include <memory>
#include <optional>
#include <type_traits>

#include "resolve_type.hpp"
#include "storage/base_segment_accessor.hpp"
#include "storage/reference_segment.hpp"
#include "storage/value_segment.hpp"
#include "types.hpp"
#include "utils/performance_warning.hpp"

namespace opossum {

/**
 * A SegmentAccessor is templated per SegmentType and DataType (T).
 * It requires that the underlying segment implements an implicit interface:
 *
 *   const std::optional<T> get_typed_value(const ChunkOffset chunk_offset) const;
 *
 */
template <typename T, typename SegmentType>
class SegmentAccessor : public AbstractSegmentAccessor<T> {
 public:
  explicit SegmentAccessor(const SegmentType& segment) : AbstractSegmentAccessor<T>{}, _segment{segment} {}

  const std::optional<T> access(ChunkOffset offset) const final { return _segment.get_typed_value(offset); }

 protected:
  const SegmentType& _segment;
};

template <typename T>
class ValueSegmentAccessor : public AbstractSegmentAccessor<T> {
 public:
  explicit ValueSegmentAccessor(const ValueSegment<T>& segment) : AbstractSegmentAccessor<T>{}, _segment{segment} {}

  const std::optional<T> access(ChunkOffset offset) const final { return _segment.get_typed_value(offset); }

  const void* get_void_ptr(ChunkOffset offset) const final {

    if (_segment.is_null(offset)) {
      return nullptr;
    }

    return &_segment.values()[offset];
  }

 protected:
  const ValueSegment<T>& _segment;
};

template <typename T>
std::unique_ptr<AbstractSegmentAccessor<T>> create_segment_accessor(const std::shared_ptr<const BaseSegment>& segment);
std::unique_ptr<BaseSegmentAccessor> create_base_segment_accessor(const std::shared_ptr<const BaseSegment>& segment);

/**
 * For ReferenceSegments, we don't use the SegmentAccessor but either the MultipleChunkReferenceSegmentAccessor or the.
 * SingleChunkReferenceSegmentAccessor. The first one is generally applicable. For each offset that is accessed, a new
 * accessor has to be created. This is because we cannot be sure that two consecutive offsets reference the same chunk.
 * In the SingleChunkReferenceSegmentAccessor, we know that the same chunk is referenced, so we create the accessor
 * only once.
 */
template <typename T>
class MultipleChunkReferenceSegmentAccessor : public AbstractSegmentAccessor<T> {
 public:
  explicit MultipleChunkReferenceSegmentAccessor(const ReferenceSegment& segment) : _segment{segment} {}

  const std::optional<T> access(ChunkOffset offset) const final {
    const auto& table = _segment.referenced_table();
    const auto& referenced_row_id = (*_segment.pos_list())[offset];
    const auto referenced_column_id = _segment.referenced_column_id();
    const auto referenced_chunk_id = referenced_row_id.chunk_id;
    const auto referenced_chunk_offset = referenced_row_id.chunk_offset;

    const auto accessor =
        create_segment_accessor<T>(table->get_chunk(referenced_chunk_id)->get_segment(referenced_column_id));
    return accessor->access(referenced_chunk_offset);
  }

 protected:
  const ReferenceSegment& _segment;
};

// Accessor for ReferenceSegments that reference single chunks - see comment above
template <typename T>
class SingleChunkReferenceSegmentAccessor : public AbstractSegmentAccessor<T> {
 public:
  explicit SingleChunkReferenceSegmentAccessor(const ReferenceSegment& segment)
      : _segment{segment},
        _chunk_id((*_segment.pos_list())[ChunkOffset{0}].chunk_id),
        _accessor{create_segment_accessor<T>(
            segment.referenced_table()->get_chunk(_chunk_id)->get_segment(_segment.referenced_column_id()))} {}

  const std::optional<T> access(ChunkOffset offset) const final {
    const auto referenced_chunk_offset = (*_segment.pos_list())[offset].chunk_offset;

    return _accessor->access(referenced_chunk_offset);
  }

  const void* get_void_ptr(ChunkOffset offset) const final {
    const auto referenced_chunk_offset = (*_segment.pos_list())[offset].chunk_offset;
    return _accessor->get_void_ptr(referenced_chunk_offset);
  }

 protected:
  const ReferenceSegment& _segment;
  const ChunkID _chunk_id;
  const std::unique_ptr<AbstractSegmentAccessor<T>> _accessor;
};

/**
 * Utility method to create a SegmentAccessor for a given BaseSegment.
 */
template <typename T>
std::unique_ptr<AbstractSegmentAccessor<T>> create_segment_accessor(const std::shared_ptr<const BaseSegment>& segment) {
  std::unique_ptr<AbstractSegmentAccessor<T>> accessor;
  resolve_segment_type<T>(*segment, [&](const auto& typed_segment) {
    using SegmentType = std::decay_t<decltype(typed_segment)>;
    if constexpr (std::is_same_v<SegmentType, ReferenceSegment>) {
      if (typed_segment.pos_list()->references_single_chunk() && typed_segment.pos_list()->size() > 0) {
        accessor = std::make_unique<SingleChunkReferenceSegmentAccessor<T>>(typed_segment);
      } else {
        accessor = std::make_unique<MultipleChunkReferenceSegmentAccessor<T>>(typed_segment);
      }
    } else if constexpr (std::is_same_v<SegmentType, ValueSegment<T>>) {
      accessor = std::make_unique<ValueSegmentAccessor<T>>(typed_segment);
    } else {
      accessor = std::make_unique<SegmentAccessor<T, SegmentType>>(typed_segment);
    }
  });
  return accessor;
}

template <typename T>
std::unique_ptr<BaseSegmentAccessor> create_base_segment_accessor(const std::shared_ptr<const BaseSegment>& segment) {
  const auto typed_segment_accessor = create_segment_accessor<T>(segment);
  //return std::unique_ptr<BaseSegmentAccessor> {static_cast<BaseSegmentAccessor*>(typed_segment_accessor.release())};
  return typed_segment_accessor;
}

}  // namespace opossum
