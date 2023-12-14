//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.h
//
// Identification: src/include/buffer/lru_k_replacer.h
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <limits>
#include <list>
#include <queue>  // 新加
#include <mutex>  // NOLINT
#include <unordered_map>
#include <vector>

#include "common/config.h"
#include "common/macros.h"

namespace bustub {

enum class AccessType { Unknown = 0, Get, Scan };

class LRUKNode {
 private:
  /** History of last seen K timestamps of this page. Least recent timestamp stored in front. */
  // Remove maybe_unused if you start using them. Feel free to change the member variables as you want.

  std::list<size_t> history_;
  size_t k_;
  frame_id_t fid_;
  bool is_evictable_{false};
  bool kicked_off_{true};  // 目前是没有登记上的状态
  bool queue_tag_{0}; // 表示在not_full_还是full_中，（为了解决满了被从not_full_中踢掉（不可能改变kicked_off_的状态），但是没有登记到full_中的情形）
  size_t register_timestamp_;  // 记录下最近一次登记的时候last_k_timestamp是多少
  
 public:
  LRUKNode(size_t replacer_k, frame_id_t frame_id);
  // 关于Evictable的状态
  void SetEvictable(bool set_evictable);
  auto isEvictable()->bool;
  void Clear();  // 用于evict之后，对于History等的重置
  // 关于History的状态
  auto GetHistorySize()->size_t;
  void NewVisit(size_t timestamp);
  // 关于full_更新priority_queue
  auto GetRegisterTimestamp()->size_t;
  auto GetLastKTimestamp()->size_t;
  void FlushKTimestamp(); // 这个是更新register_timestamp字段的。
  // 关于not_full_和full_的登记状态
  void KickOff(bool on_off); // 通知已经被full_踢下去了，需要找机会自己再register一下。
  auto isRegistered()->bool;  // 查看被踢下去了没有
  auto GetQueueTag()->bool; //查看在not_full_还是full_中（配合GetHistorySize以及isRegistered食用）超过k_并且isRegister==true并且not_full_就改
  void SetQueueTag(bool queue_tag); // 0:not_full_ 1:full_
};

/**
 * LRUKReplacer implements the LRU-k replacement policy.
 *
 * The LRU-k algorithm evicts a frame whose backward k-distance is maximum
 * of all frames. Backward k-distance is computed as the difference in time between
 * current timestamp and the timestamp of kth previous access.
 *
 * A frame with less than k historical references is given
 * +inf as its backward k-distance. When multiple frames have +inf backward k-distance,
 * classical LRU algorithm is used to choose victim.
 */
class LRUKReplacer {
 public:
  /**
   *
   * TODO(P1): Add implementation
   *
   * @brief a new LRUKReplacer.
   * @param num_frames the maximum number of frames the LRUReplacer will be required to store
   */
  explicit LRUKReplacer(size_t num_frames, size_t k);

  DISALLOW_COPY_AND_MOVE(LRUKReplacer);

  /**
   * TODO(P1): Add implementation
   *
   * @brief Destroys the LRUReplacer.
   */
  ~LRUKReplacer();  // =default的意思是让编译器自己生成

  /**
   * TODO(P1): Add implementation
   *
   * @brief Find the frame with largest backward k-distance and evict that frame. Only frames
   * that are marked as 'evictable' are candidates for eviction.
   *
   * A frame with less than k historical references is given +inf as its backward k-distance.
   * If multiple frames have inf backward k-distance, then evict frame with earliest timestamp
   * based on LRU.
   *
   * Successful eviction of a frame should decrement the size of replacer and remove the frame's
   * access history.
   *
   * @param[out] frame_id id of frame that is evicted.
   * @return true if a frame is evicted successfully, false if no frames can be evicted.
   */
  auto Evict(frame_id_t *frame_id) -> bool;

  /**
   * TODO(P1): Add implementation
   *
   * @brief Record the event that the given frame id is accessed at current timestamp.
   * Create a new entry for access history if frame id has not been seen before.
   *
   * If frame id is invalid (ie. larger than replacer_size_), throw an exception. You can
   * also use BUSTUB_ASSERT to abort the process if frame id is invalid.
   *
   * @param frame_id id of frame that received a new access.
   * @param access_type type of access that was received. This parameter is only needed for
   * leaderboard tests.
   */
  void RecordAccess(frame_id_t frame_id, AccessType access_type = AccessType::Unknown);

  /**
   * TODO(P1): Add implementation
   *
   * @brief Toggle whether a frame is evictable or non-evictable. This function also
   * controls replacer's size. Note that size is equal to number of evictable entries.
   *
   * If a frame was previously evictable and is to be set to non-evictable, then size should
   * decrement. If a frame was previously non-evictable and is to be set to evictable,
   * then size should increment.
   *
   * If frame id is invalid, throw an exception or abort the process.
   *
   * For other scenarios, this function should terminate without modifying anything.
   *
   * @param frame_id id of frame whose 'evictable' status will be modified
   * @param set_evictable whether the given frame is evictable or not
   */
  void SetEvictable(frame_id_t frame_id, bool set_evictable);

  /**
   * TODO(P1): Add implementation
   *
   * @brief Remove an evictable frame from replacer, along with its access history.
   * This function should also decrement replacer's size if removal is successful.
   *
   * Note that this is different from evicting a frame, which always remove the frame
   * with largest backward k-distance. This function removes specified frame id,
   * no matter what its backward k-distance is.
   *
   * If Remove is called on a non-evictable frame, throw an exception or abort the
   * process.
   *
   * If specified frame is not found, directly return from this function.
   *
   * @param frame_id id of frame to be removed
   */
  void Remove(frame_id_t frame_id);

  /**
   * TODO(P1): Add implementation
   *
   * @brief Return replacer's size, which tracks the number of evictable frames.
   *
   * @return size_t
   */
  auto Size() -> size_t;

 private:
  // TODO(student): implement me! You can replace these member variables as you like.
  // Remove maybe_unused if you start using them.
  std::unordered_map<frame_id_t, LRUKNode> node_store_;
  size_t current_timestamp_{0};
  size_t curr_size_{0};  // 当前的lru池子里的元素数(能够evict的)
  size_t replacer_size_; // 这个是replacer（能够evict以及pin上的）最大值
  size_t k_; // 这是 LRU_k中的K
  [[maybe_unused]] std::mutex latch_;

//  public:
//   static bool cmp(frame_id_t& a, frame_id_t& b){  // 小根堆,说找不到这个函数，看看挪上来可以吗
//     return node_store_[a].GetRegisterTimestamp()< node_store_[b].GetRegisterTimestamp(); // 好像语法出问题了
//   }
struct cmp{
  bool operator()(std::pair<frame_id_t,size_t>& a, std::pair<frame_id_t,size_t>& b){
    // return a.second < b.second;
    return a.second > b.second; //这个比较和正常的逻辑是反的。（如果想让a排在b前，需要a>b）
  }
};
  
 private:
  // 辅助的一些数据结构
  size_t curr_tot_size_{0};
  std::list<frame_id_t> not_full_;  //引用次数未达到k次的
  std::priority_queue<std::pair<frame_id_t,size_t>, std::vector<std::pair<frame_id_t,size_t>>, cmp> full_;  // 之前写力扣的时候也遇到这个问题了，忘了怎么解决的了
};

}  // namespace bustub
