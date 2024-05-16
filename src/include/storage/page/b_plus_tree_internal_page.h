//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/page/b_plus_tree_internal_page.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#pragma once

#include <queue>
#include <string>

#include "storage/page/b_plus_tree_page.h"

namespace bustub {

#define B_PLUS_TREE_INTERNAL_PAGE_TYPE BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>
#define INTERNAL_PAGE_HEADER_SIZE 12
#define INTERNAL_PAGE_SIZE ((BUSTUB_PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (sizeof(MappingType)))
/**
 * Store n indexed keys and n+1 child pointers (page_id) within internal page.
 * Pointer PAGE_ID(i) points to a subtree in which all keys K satisfy:
 * K(i) <= K < K(i+1).
 * NOTE: since the number of keys does not equal to number of child pointers,
 * the first key always remains invalid. That is to say, any search/lookup
 * should ignore the first key.
 *
 * Internal page format (keys are stored in increasing order):
 *  --------------------------------------------------------------------------
 * | HEADER | KEY(1)+PAGE_ID(1) | KEY(2)+PAGE_ID(2) | ... | KEY(n)+PAGE_ID(n) |
 *  --------------------------------------------------------------------------
 */
INDEX_TEMPLATE_ARGUMENTS
class BPlusTreeInternalPage : public BPlusTreePage {
 public:
  // Deleted to disallow initialization
  BPlusTreeInternalPage() = delete;
  BPlusTreeInternalPage(const BPlusTreeInternalPage &other) = delete;

  /**
   * Writes the necessary header information to a newly created page, must be called after
   * the creation of a new page to make a valid BPlusTreeInternalPage
   * @param max_size Maximal size of the page
   */
  void Init(int max_size = INTERNAL_PAGE_SIZE);

  /**
   * @param index The index of the key to get. Index must be non-zero.
   * @return Key at index
   */
  auto KeyAt(int index) const -> KeyType;

  /**
   *
   * @param index The index of the key to set. Index must be non-zero.
   * @param key The new value for key
   */
  void SetKeyAt(int index, const KeyType &key);

  /**
   *
   * @param value the value to search for
   */
  auto ValueIndex(const ValueType &value) const -> int;

  /**
   *
   * @param index the index
   * @return the value at the index
   */
  auto ValueAt(int index) const -> ValueType;

  /**
   * ************************************
   *        下面均为自己添加的函数
   * ************************************
  */

  /**
   * 在检查过没有重复key的情况下，并且不满的情况下
   * 在array中添加key，value对
  */
  void InsertKeyValueAt(int index, /*const*/ KeyType /*&*/key, /*const*/ ValueType /*&*/value);

  /**
   * 内置了一个根据key，查找下一个该走那条edge的方法
   * @param key 需要查找的key
   * @return 返回要走的edge（ptr）
  */
  auto FindNextNode(const KeyType &key, const KeyComparator & comparator_) const -> ValueType ;

  /**
   * 节点中插入关键值我就直接封装成一个函数了，这样b_plus_tree中逻辑可能会清晰些
  */
  void InsertKeyValueNotFull(/*const*/ KeyType &key, /*const*/ ValueType &value, KeyComparator comparator);

  /**
   * split，在internal节点下插入一个新的key value对
  */
  auto SplitInsert(const KeyType &key, const ValueType &value, KeyComparator comparator,  BPlusTreeInternalPage* newInternal)->std::pair<KeyType, KeyType>;

  /**
   * *****************************************************
   *                      DELETION
   * *****************************************************
  */
  /**
   * 牵连父节点，更改父节点的key值（parentNode->ChangeKey()）
   */
  void ChangeKey(KeyType new_key, KeyType old_key, KeyComparator comparator);


  auto FindChildSiblingIndex(KeyType key, KeyComparator comparator) -> std::pair<int, bool>;


  /**
   * @brief For test only, return a string representing all keys in
   * this internal page, formatted as "(key1,key2,key3,...)"
   *
   * @return std::string
   */
  auto ToString() const -> std::string {
    std::string kstr = "(";
    bool first = true;

    // first key of internal page is always invalid
    for (int i = 1; i < GetSize(); i++) {
      KeyType key = KeyAt(i);
      if (first) {
        first = false;
      } else {
        kstr.append(",");
      }

      kstr.append(std::to_string(key.ToString()));
    }
    kstr.append(")");

    return kstr;
  }

 private:
  // Flexible array member for page data. 
  MappingType array_[0];  // 知识点！这个叫做柔性数组 https://zhuanlan.zhihu.com/p/247716877
};
}  // namespace bustub
