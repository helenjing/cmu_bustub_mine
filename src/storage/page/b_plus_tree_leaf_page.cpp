//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/page/b_plus_tree_leaf_page.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <sstream>

#include "common/exception.h"
#include "common/rid.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set next page id and set max size
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
  SetPageType(IndexPageType::LEAF_PAGE);
  SetSize(0);
  SetMaxSize(max_size);
  next_page_id_ = INVALID_PAGE_ID;  // setNextPageId怎么办？只能这个Init先调用
}

/**
 * Helper methods to set/get next page id
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t { 
  return next_page_id_; 
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) {
  next_page_id_ = next_page_id;
}

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  if(index >= GetSize()) throw Exception("BPlusTreeLeafPage::KeyAt:array_ index out of range...");
  return array_[index].first; // 好像不对
}

    /**
   *
   * @param index the index
   * @return the value at the index
   */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const -> ValueType{
  if(index >= GetSize()){
    // std::cout << index << ":" << GetSize() << std::endl;
    throw Exception("BPlusTreeLeafPage::ValueAt:array_ index out of range...");
  } 
  return array_[index].second; // 好像不对
}

  /**
   * 没有comparator这种东西，所以更新value的操作，只能再函数外做了
   * 这里只是在不满的情况下进行插入
   * @param index The index of the key to set. Index must be non-zero.
   * @param key The new value for key
   * @param value The new value for value
   */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::InsertKeyValueAt(int index, const KeyType &key, const ValueType &value){
    // 如果原来就存在这个key，就更新value
    // 如果不存在这个可以，就插入到这里（应该是保证了不满的情况下），如果满了还是throw exception
    if(index > GetSize() || index < 0){
      std::cout << index << ":" << GetSize() << std::endl;
      throw Exception("BPlusTreeLeafPage::InsertKeyValueAt:array_ index out of range...");
    } 
    if(GetSize() == GetMaxSize()) throw Exception("BPlusTreeLeafPage::InsertKeyValueAt:page is full, cannot insert into this page...");
    int ptr = GetSize();  // 这个是最后一个元素往后挪完的位置
    while(ptr!=index){
      array_[ptr] = array_[ptr-1];
      ptr--;
    }
    array_[ptr] = {key, value};
    IncreaseSize(1);
  }

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::FindValueForKey(const KeyType &key, ValueType *value, KeyComparator comparator) const -> bool{
  for(int i = 0; i < GetSize(); i++){
    if(comparator(KeyAt(i), key)==0){
      *value = ValueAt(i); // 为什么result要声明为vector，不是说uniquekey嘛
      return true;
    }
  }
  return false;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::InsertKeyValueNotFull(const KeyType &key, const ValueType &value, KeyComparator comparator){
  // std::cout << "InsertKeyValueNotFull(Leaf!!)" << std::endl;
  for(int i = 0; i < GetSize(); i++){
    if(comparator(KeyAt(i),key)==1){ // 查看是否存在key，如果存在key则返回false
      // std::cout << KeyAt(i) << ">" << key << std::endl;
      InsertKeyValueAt(i, key, value); // 这里应该不用手动清空吧，手动drop
      return;
    } 
  } 
  InsertKeyValueAt(GetSize(), key, value);
}


  /**
   * split，在leaf节点下插入一个新的key value对
  */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::SplitInsert(const KeyType &key, const ValueType &value, KeyComparator comparator, BPlusTreeLeafPage* newLeaf, page_id_t new_page_id)
 -> std::pair<KeyType, KeyType>{
  // std::cout <<"leafPage:splitInsert:GetOldSize" << this->GetSize() << std::endl;
  int insert_ind = 0;
  for(  ; insert_ind < this->GetSize(); insert_ind++){  // 找到插入的定位点
    
    if(comparator(KeyAt(insert_ind),key)==1) break;   // -1 1还是记不清（改了）
  } 

  // 写入数据，后半 leaf:0~MaxSize/2, newLeaf: MaxSize/2+1~MaxSize
  int movePtr = newLeaf->GetMaxSize()/2+1;
  if(movePtr > insert_ind){ // insert_ind在leaf中
    // std::cout << "insertion in old_leaf..." << std::endl;
    for(int i = movePtr; i <= newLeaf->GetMaxSize(); i++){
      // std::cout << "insert key at:"<< i-1 << "#" << this->KeyAt(i-1)  << " newplace:"<< i-movePtr<< std::endl;
      newLeaf->InsertKeyValueAt(i-movePtr, this->KeyAt(i-1), this->ValueAt(i-1));
    }
    // this->IncreaseSize(-newLeaf->GetMaxSize()+newLeaf->GetMaxSize()/2);  // 这相当于就把leaf中的内容删掉了 
    this->SetSize(this->GetMaxSize()/2);
    this->InsertKeyValueAt(insert_ind, key, value); // 这里应该不用手动清空吧，手动drop
  }else{  // insert_ind在newLeaf中, 貌似还是有问题
    // std::cout << "insertion in new_leaf..." << std::endl;
    for(int i = movePtr; i <= newLeaf->GetMaxSize(); i++){
      if(i < insert_ind)  newLeaf->InsertKeyValueAt(i-movePtr, this->KeyAt(i), this->ValueAt(i));
      else if(i == insert_ind) newLeaf->InsertKeyValueAt(i-movePtr, key, value); // 这里应该不用手动清空吧，手动drop
      else newLeaf->InsertKeyValueAt(i-movePtr, this->KeyAt(i-1), this->ValueAt(i-1));
    }
    // this->IncreaseSize(-newLeaf->GetMaxSize()+newLeaf->GetMaxSize()/2+1);  // 这相当于就把leaf中的内容删掉了
    this->SetSize(this->GetMaxSize()/2+1);
  }

    // siblings
  newLeaf->next_page_id_ = this->next_page_id_;
  this->next_page_id_ = new_page_id;
  return {this->KeyAt(0),newLeaf->KeyAt(0)};
}

/**
 * **************************************************
 *                    DELETION
 * **************************************************
*/
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::DeleteWithoutMerge(const KeyType &key, KeyComparator comparator)->bool{
  std::cout << "BPlusLeafPage::DeleteWithoutMerge..." << std::endl;
  for(int i = 0; i < GetSize(); i++){
    if(comparator(KeyAt(i),key)==0){ // 查看是否存在key，如果存在key则返回false
      DeleteKeyValueAt(i); // 这里应该不用手动清空吧，手动drop
      // std::cout << i << " " << GetSize()<< std::endl;
      return i==0;
    } 
  } 
  throw Exception("B+TreeLeafPage::ChangeKey:cannot find old_key...");
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::DeleteKeyValueAt(int index){
  for(int i = index; i < GetSize()-1; i++){
    array_[i] = array_[i+1];
  }
  IncreaseSize(-1);
  
}


template class BPlusTreeLeafPage<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTreeLeafPage<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTreeLeafPage<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTreeLeafPage<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
