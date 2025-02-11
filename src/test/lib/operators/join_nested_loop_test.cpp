#include "base_test.hpp"

#include "operators/join_nested_loop.hpp"
#include "operators/projection.hpp"
#include "operators/table_wrapper.hpp"

namespace opossum {

class OperatorsJoinNestedLoopTest : public BaseTest {
 public:
  void SetUp() override {
    const auto dummy_table =
        std::make_shared<Table>(TableColumnDefinitions{{"a", DataType::Int, false}}, TableType::Data);
    dummy_input = std::make_shared<TableWrapper>(dummy_table);
    dummy_input->never_clear_output();
  }

  std::shared_ptr<AbstractOperator> dummy_input;
};

TEST_F(OperatorsJoinNestedLoopTest, DescriptionAndName) {
  const auto primary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::Equals};
  const auto secondary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::NotEquals};

  const auto join_operator =
      std::make_shared<JoinNestedLoop>(dummy_input, dummy_input, JoinMode::Inner, primary_predicate,
                                       std::vector<OperatorJoinPredicate>{secondary_predicate});

  EXPECT_EQ(join_operator->description(DescriptionMode::SingleLine),
            "JoinNestedLoop (Inner Join where Column #0 = Column #0 AND Column #0 != Column #0)");
  EXPECT_EQ(join_operator->description(DescriptionMode::MultiLine),
            "JoinNestedLoop\n(Inner Join where Column #0 = Column #0 AND Column #0 != Column #0)");

  dummy_input->execute();
  EXPECT_EQ(join_operator->description(DescriptionMode::SingleLine),
            "JoinNestedLoop (Inner Join where a = a AND a != a)");
  EXPECT_EQ(join_operator->description(DescriptionMode::MultiLine),
            "JoinNestedLoop\n(Inner Join where a = a AND a != a)");

  EXPECT_EQ(join_operator->name(), "JoinNestedLoop");
}

TEST_F(OperatorsJoinNestedLoopTest, DeepCopy) {
  const auto primary_predicate = OperatorJoinPredicate{{ColumnID{0}, ColumnID{0}}, PredicateCondition::Equals};
  const auto secondary_predicates =
      std::vector<OperatorJoinPredicate>{{{ColumnID{1}, ColumnID{1}}, PredicateCondition::NotEquals}};
  const auto join_operator = std::make_shared<JoinNestedLoop>(dummy_input, dummy_input, JoinMode::Left,
                                                              primary_predicate, secondary_predicates);
  const auto abstract_join_operator_copy = join_operator->deep_copy();
  const auto join_operator_copy = std::dynamic_pointer_cast<JoinNestedLoop>(abstract_join_operator_copy);

  ASSERT_TRUE(join_operator_copy);

  EXPECT_EQ(join_operator_copy->mode(), JoinMode::Left);
  EXPECT_EQ(join_operator_copy->primary_predicate(), primary_predicate);
  EXPECT_EQ(join_operator_copy->secondary_predicates(), secondary_predicates);
  EXPECT_NE(join_operator_copy->left_input(), nullptr);
  EXPECT_NE(join_operator_copy->right_input(), nullptr);
}

}  // namespace opossum
