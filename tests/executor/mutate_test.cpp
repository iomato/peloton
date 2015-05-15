/**
 * @brief Test cases for insert node.
 *
 * Copyright(c) 2015, CMU
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "catalog/schema.h"
#include "common/exception.h"
#include "common/value.h"
#include "common/value_factory.h"

#include "executor/delete_executor.h"
#include "executor/insert_executor.h"
#include "executor/seq_scan_executor.h"

#include "executor/logical_tile.h"
#include "executor/logical_tile_factory.h"

#include "planner/delete_node.h"
#include "planner/insert_node.h"
#include "planner/seq_scan_node.h"

#include "storage/backend_vm.h"
#include "storage/tile.h"
#include "storage/tile_group.h"

#include "executor/executor_tests_util.h"
#include "expression/expression_util.h"

#include "executor/executor_tests_util.h"
#include "executor/mock_executor.h"
#include "harness.h"

#include <atomic>

using ::testing::NotNull;
using ::testing::Return;

namespace nstore {
namespace test {

//===--------------------------------------------------------------------===//
// Mutator Tests
//===--------------------------------------------------------------------===//

std::atomic<int> tuple_id;
std::atomic<int> delete_tuple_id;

void InsertTuple(storage::Table *table){

  planner::InsertNode node(table);
  std::vector<storage::Tuple *> tuples;
  const txn_id_t txn_id = 1000;
  Context context(txn_id);

  for(int tuple_itr = 0 ; tuple_itr < 10 ; tuple_itr++) {
    auto tuple = ExecutorTestsUtil::GetTuple(table, ++tuple_id);
    tuples.push_back(tuple);
  }

  // Bulk insert
  executor::InsertExecutor executor(&node, &context, tuples);
  executor.Execute();

  for(auto tuple : tuples) {
    tuple->FreeUninlinedData();
    delete tuple;
  }

}

void DeleteTuple(storage::Table *table){

  const txn_id_t txn_id = 2000;
  Context context(txn_id);
  std::vector<storage::Tuple *> tuples;

  // Delete
  planner::DeleteNode delete_node(table, false);
  executor::DeleteExecutor delete_executor(&delete_node, &context);

  // Seq scan
  std::vector<id_t> column_ids = { 0 };
  planner::SeqScanNode seq_scan_node(
      table,
      expression::ConstantValueFactory(ValueFactory::GetTrue()),
      column_ids);
  executor::SeqScanExecutor seq_scan_executor(&seq_scan_node);

  // Parent-Child relationship
  delete_node.AddChild(&seq_scan_node);
  delete_executor.AddChild(&seq_scan_executor);

  delete_executor.Execute();

}

// Insert a tuple into a table
TEST(InsertTests, BasicTests) {

  // Create insert node for this test.
  storage::Table *table = ExecutorTestsUtil::CreateTable();
  planner::InsertNode node(table);

  // Pass through insert executor.
  const txn_id_t txn_id = 1000;
  Context context(txn_id);
  storage::Tuple *tuple;
  std::vector<storage::Tuple *> tuples;

  tuple = ExecutorTestsUtil::GetNullTuple(table);
  tuples.push_back(tuple);
  executor::InsertExecutor executor(&node, &context, tuples);

  try{
    executor.Execute();
  }
  catch(ConstraintException& ce){
    std::cout << ce.what();
  }

  tuple->FreeUninlinedData();
  delete tuple;
  tuples.clear();

  tuple = ExecutorTestsUtil::GetTuple(table, ++tuple_id);
  tuples.push_back(tuple);
  executor::InsertExecutor executor2(&node, &context, tuples);
  executor2.Execute();

  try{
    executor2.Execute();
  }
  catch(ConstraintException& ce){
    std::cout << ce.what();
  }

  tuple->FreeUninlinedData();
  delete tuple;
  tuples.clear();

  LaunchParallelTest(4, InsertTuple, table);
  //std::cout << (*table);

  //LaunchParallelTest(1, DeleteTuple, table);
  //std::cout << (*table);

  // PRIMARY KEY
  auto pkey_index = table->GetIndex(0);
  std::vector<catalog::ColumnInfo> columns;

  columns.push_back(ExecutorTestsUtil::GetColumnInfo(0));
  catalog::Schema *key_schema = new catalog::Schema(columns);
  storage::Tuple *key1 = new storage::Tuple(key_schema, true);
  storage::Tuple *key2 = new storage::Tuple(key_schema, true);

  key1->SetValue(0, ValueFactory::GetIntegerValue(10));
  key2->SetValue(0, ValueFactory::GetIntegerValue(100));

  auto pkey_list = pkey_index->GetLocationsForKeyBetween(key1, key2);
  std::cout << "PKEY INDEX :: Entries : " << pkey_list.size() << "\n";

  delete key1;
  delete key2;
  delete key_schema;

  // SEC KEY
  auto sec_index = table->GetIndex(1);

  columns.clear();
  columns.push_back(ExecutorTestsUtil::GetColumnInfo(0));
  columns.push_back(ExecutorTestsUtil::GetColumnInfo(1));
  key_schema = new catalog::Schema(columns);

  storage::Tuple *key3 = new storage::Tuple(key_schema, true);
  storage::Tuple *key4 = new storage::Tuple(key_schema, true);

  key3->SetValue(0, ValueFactory::GetIntegerValue(10));
  key3->SetValue(1, ValueFactory::GetIntegerValue(11));
  key4->SetValue(0, ValueFactory::GetIntegerValue(100));
  key4->SetValue(1, ValueFactory::GetIntegerValue(101));

  auto sec_list = sec_index->GetLocationsForKeyBetween(key3, key4);
  std::cout << "SEC INDEX :: Entries : " << sec_list.size() << "\n";

  delete key3;
  delete key4;
  delete key_schema;

  delete table;
}

} // namespace test
} // namespace nstore
