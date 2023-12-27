//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"

  

namespace bustub {
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, and set max page size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(int max_size) {   // 对于柔性数组的创建在这儿可以吗
    // 可是如果没有申请成功， BPlusTreeInternalPage就会变成null啊，里面其他的数据也找不到了
    // 继承关系，直接访问父类字段就好了(访问不了，私有的)
  SetPageType(IndexPageType::INTERNAL_PAGE);  // 原来不是因为没有include进来，而是必须要namespace
  SetSize(0);
  SetMaxSize(max_size);
}
/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  // replace with your own code
  if(index >= GetSize()) throw Exception("array_ index out of range...");
  return array_[index].first; // 好像不对
  // return (array_+index)->first; // 好像不对
  // KeyType key{};
  // return key;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
  if(index > GetSize()) throw Exception("array_ index out of range...");
  array_[index].first = key;
}

/*
 * Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType { 
  if(index >= GetSize())  throw Exception("array_ index out of range...");
  return array_[index].second; // 好像不对
 }

  /**
   * ************************************
   *        下面均为自己添加的函数
   * ************************************
  */

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertKeyValueAt(int index, const KeyType &key, const ValueType &value){
  if(index > GetSize() || index < 0)  throw Exception("array_ index out of range...");
  if(GetSize()==GetMaxSize()) throw Exception("array_ is full, cannot insert into this page...");
  int ptr = GetSize();  // 这个是最后一个元素往后挪完的位置
  while(ptr!=index){
      array_[ptr] = array_[ptr-1];
      ptr--;
    }
  array_[ptr] = {key, value};
  IncreaseSize(1);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::FindNextNode(const KeyType &key, const KeyComparator & comparator_) const -> ValueType{
  auto rtvalue = ValueAt(GetSize()-1); // 不用等遍历结束再赋值为最后一个value
  for(int i = 1; i < GetSize(); i++){
    // 
    if(comparator_(KeyAt(i), key)==1){
      rtvalue = ValueAt(i-1);
      break;
    }
  }
  return rtvalue;
}
  

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertKeyValueNotFull(const KeyType &key, const ValueType &value, KeyComparator comparator){
  for(int i = 1; i < GetSize(); i++){
    if(comparator(KeyAt(i),key)==1){ // 查看是否存在key，如果存在key则返回false
      InsertKeyValueAt(i, key, value); // 这里应该不用手动清空吧，手动drop
      return;
    } 
  } 
  InsertKeyValueAt(GetSize(), key, value);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::SplitInsert(const KeyType &key, const ValueType &value, KeyComparator comparator, BPlusTreeInternalPage* newInternal)
 -> std::pair<KeyType, KeyType>{  // 有问题，再调吧
  int insert_ind = 1;
  for(  ; insert_ind < this->GetSize(); insert_ind++){  // 找到插入的定位点
    if(comparator(KeyAt(insert_ind),key)==1) break; 
  } 
  // 写入数据，后半 leaf:0~MaxSize/2, newLeaf: MaxSize/2+1~MaxSize
  int movePtr = newInternal->GetMaxSize()/2+1;
  if(movePtr > insert_ind){ // insert_ind在internal中,internal[0]的哨兵怎么办，最好这些也扔到一个函数里面去吧
    // std::cout<<"insertion in old_internal..." << std::endl;
    for(int i = movePtr; i <= newInternal->GetMaxSize(); i++){  // 逻辑应该是对的
      newInternal->InsertKeyValueAt(i-movePtr, this->KeyAt(i-1), this->ValueAt(i-1));
    }
    this->IncreaseSize(-newInternal->GetMaxSize()+newInternal->GetMaxSize()/2);  // 这相当于就把leaf中的内容删掉了
    this->InsertKeyValueAt(insert_ind, key, value); // 这里有问题，key也不是参数key而是下一级的最头头的那个key
  }else{  // insert_ind在newLeaf中, 貌似还是有问题
  // std::cout << "insertion in new_internal..." << std::endl;
    for(int i = movePtr; i <= newInternal->GetMaxSize(); i++){
      if(i < insert_ind)  newInternal->InsertKeyValueAt(i-movePtr, this->KeyAt(i), this->ValueAt(i));
      else if(i == insert_ind) newInternal->InsertKeyValueAt(i-movePtr, key, value); // 这里应该不用手动清空吧，手动drop
      else newInternal->InsertKeyValueAt(i-movePtr+1, this->KeyAt(i-1), this->ValueAt(i-1));
    }
    this->IncreaseSize(-newInternal->GetMaxSize()+newInternal->GetMaxSize()/2+1);  // 这相当于就把leaf中的内容删掉了
  }
  return {this->KeyAt(1),newInternal->KeyAt(0)};  // 因为最开始的0是废掉的。
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;
}  // namespace bustub
