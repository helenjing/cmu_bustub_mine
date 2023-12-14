//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// clock_replacer.h
//
// Identification: src/include/buffer/clock_replacer.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <list>
#include <mutex>  // NOLINT
#include <vector>

#include "buffer/replacer.h"
#include "common/config.h"

namespace bustub {

/**
 * ClockReplacer implements the clock replacement policy, which approximates the Least Recently Used policy.
 */
class ClockReplacer : public Replacer {
 public:
  /**
   * Create a new ClockReplacer.
   * @param num_pages the maximum number of pages the ClockReplacer will be required to store
   */
  explicit ClockReplacer(size_t num_pages);

  /**
   * Destroys the ClockReplacer.
   */
  ~ClockReplacer() override;

  auto Victim(frame_id_t *frame_id) -> bool override;

  void Pin(frame_id_t frame_id) override;

  void Unpin(frame_id_t frame_id) override;

  auto Size() -> size_t override;

 private:
  // TODO(student): implement me!
  class Node{ //弄一个双向链表是不是好一点
    public:
      frame_id_t id = 0;
      bool isPinned = false;
      bool ref = true;  //表示近期有使用
      bool isDirty = false;
      Node* next = nullptr;
      Node* pre = nullptr;
  };


  int max_tot_cnt = 0;

  int pin_cnt = 0;

  int unpin_cnt = 0;  

  Node* unpin_ptr = nullptr;  

  Node* pin_ptr = nullptr; //给一个头指针吧
  
  Node* FindUnpin(frame_id_t frame_id);

  Node* FindPin(frame_id_t frame_id);

  void InsertUnpin(Node* ptr);

  void InsertPin(Node* ptr);

  Node* DeleteUnpin(Node* ptr);

  Node* DeletePin(Node* ptr);
};

}  // namespace bustub
