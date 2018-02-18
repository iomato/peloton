//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// sorter.h
//
// Identification: src/include/codegen/sorter.h
//
// Copyright (c) 2015-2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <unordered_map>

#include "codegen/updateable_storage.h"

namespace peloton {
namespace codegen {

//===----------------------------------------------------------------------===//
// This class simplifies interaction with a codegen::util::Sorter instance from
// generated code.
//===----------------------------------------------------------------------===//
class Sorter {
 public:
  class IterateCallback;
  class VectorizedIterateCallback;

  Sorter();
  Sorter(CodeGen &codegen, const std::vector<type::Type> &row_desc);

  /**
   * @brief Initialize the given sorter instance with the comparison function
   */
  void Init(CodeGen &codegen, llvm::Value *sorter_ptr,
            llvm::Value *comparison_func) const;

  /**
   * @brief Append the given tuple into the sorter instance
   */
  void Append(CodeGen &codegen, llvm::Value *sorter_ptr,
              const std::vector<codegen::Value> &tuple) const;

  /**
   * @brief Sort all the data that has been inserted into the sorter instance
   */
  void Sort(CodeGen &codegen, llvm::Value *sorter_ptr) const;

  /**
   * @brief Perform a parallel sort of all materialized runs stored in the
   * provided thread states
   */
  void SortParallel(CodeGen &codegen, llvm::Value *sorter_ptr,
                    llvm::Value *thread_states, uint32_t sorter_offset) const;

  /**
   * @brief Iterate over tuples stored in this sorter tuple-at-a-time
   */
  void Iterate(CodeGen &codegen, llvm::Value *sorter_ptr,
               IterateCallback &callback) const;

  /**
   * @brief Iterate over tuples in this sorter batch-at-a-time
   */
  void VectorizedIterate(CodeGen &codegen, llvm::Value *sorter_ptr,
                         uint32_t vector_size,
                         VectorizedIterateCallback &callback) const;

  /**
   * @brief Destroy all resources managed by this sorter
   */
  void Destroy(CodeGen &codegen, llvm::Value *sorter_ptr) const;

  //////////////////////////////////////////////////////////////////////////////
  ///
  /// Accessors
  ///
  //////////////////////////////////////////////////////////////////////////////

  const UpdateableStorage &GetStorageFormat() const { return storage_format_; }

  llvm::Value *NumTuples(CodeGen &codegen, llvm::Value *sorter_ptr) const;

  //////////////////////////////////////////////////////////////////////////////
  ///
  /// Helper classes
  ///
  //////////////////////////////////////////////////////////////////////////////

  //===--------------------------------------------------------------------===//
  // A class that provides a random access interface over a Sorter instance
  //===--------------------------------------------------------------------===//
  class SorterAccess {
   public:
    // A row in the sorter instance
    class Row {
     public:
      // Load a column at the given index in the row
      codegen::Value LoadColumn(CodeGen &codegen, uint32_t column_index);

     private:
      friend class SorterAccess;
      Row(SorterAccess &access, llvm::Value *row_idx);

     private:
      // The sorter accessor and the index of the row in the sorter instance
      SorterAccess &access_;
      llvm::Value *row_idx_;
      llvm::Value *row_pos_;
    };

    // Constructor
    SorterAccess(const Sorter &sorter, llvm::Value *start_pos);

    // Access a given row
    Row &GetRow(llvm::Value *row_idx);

   private:
    // Load the column with the provided index in the provided row
    codegen::Value LoadRowValue(CodeGen &codegen, Row &row,
                                uint32_t column_index) const;

   private:
    // The physical data format
    const Sorter &sorter_;
    // The pointer to the row in the sorter
    llvm::Value *start_pos_;
    std::unordered_map<llvm::Value *, Row> cached_rows_;
  };

  //===--------------------------------------------------------------------===//
  // Callback to process one entry when doing a tuple-at-a-time scan
  //===--------------------------------------------------------------------===//
  struct IterateCallback {
    // Destructor
    virtual ~IterateCallback() = default;

    // Process the range of rows between the given start and end indices
    virtual void ProcessEntry(
        CodeGen &codegen, const std::vector<codegen::Value> &vals) const = 0;
  };

  //===--------------------------------------------------------------------===//
  // Callback to process a vector of entries when doing a vectorized scan
  //===--------------------------------------------------------------------===//
  struct VectorizedIterateCallback {
    // Destructor
    virtual ~VectorizedIterateCallback() = default;

    // Process the range of rows between the given start and end indices
    virtual void ProcessEntries(CodeGen &codegen, llvm::Value *start_index,
                                llvm::Value *end_index,
                                SorterAccess &access) const = 0;
  };

 private:
  llvm::Value *GetStartPosition(CodeGen &codegen,
                                llvm::Value *sorter_ptr) const;
  llvm::Value *GetEndPosition(CodeGen &codegen, llvm::Value *sorter_ptr) const;

 private:
  // Compact storage to materialize things
  // TODO: Change to CompactStorage?
  UpdateableStorage storage_format_;
};

}  // namespace codegen
}  // namespace peloton