#include <sstream>
#include <string>
#include <tuple>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
// #include "b_plus_tree.h"

namespace bustub {

Context::~Context(){
  // 释放资源，read_set_, write_set_
  // std::cout<< "~Context" << std::endl;
  while(!read_set_.empty()){
    read_set_.front().Drop();
    read_set_.pop_front();
  }
  while(!write_set_.empty()){
    bpm_->FlushPage(write_set_.front().PageId());
    write_set_.front().Drop(); //相当于unpin了，latch也解掉了？
    write_set_.pop_front();
  }
  if(header_page_w_ != std::nullopt){
    // auto header_page_r = reinterpret_cast<BPlusTreeHeaderPage *>(bpm_->FetchPage(header_page_id_)->GetData());
    bool flag = bpm_->FlushPage(header_page_id_);
    bpm_->UnpinPage(header_page_id_, false);
    // bpm_->DeletePage(header_page_id_);
  }
}
void Context::PopReadGuardToWriteGuard(WritePageGuard *writeGuard){
  if(read_set_.empty()) throw Exception("read_set_ is empty.");
  *writeGuard = std::move(bpm_->FetchPageWrite(read_set_.back().PageId()));
  read_set_.pop_back();
  write_set_.push_back(std::move(*writeGuard));  // 改了移动构造，但是不懂移动构造是什么
}

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->FetchPageWrite(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { 
  // 如果这个指的是一整棵树，而非（所在）子树为空的话
  BasicPageGuard guard = bpm_->FetchPageBasic(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();  // 这个函数是不是按照一定的格式来解析页内的数据？
  return root_page->root_page_id_==INVALID_PAGE_ID;
  
}
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
// 完全没考虑多线程，加锁的问题，template好像也写错了导致cur_page无法识别类型
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *txn) -> bool {
  // Declaration of context instance.存储一些关于这棵树地metadata这样不用反复地IO了
  // 1. 先读取dummy_page找到root在哪儿，应该是上锁了，但是怎么解锁啊
  // 1. 获得节点：拿到page_id，fetchPage，将这一页的数据解析为InternalPage或者是LeafPage的模式。（如何确定是叶子节点还是其他）
  // 2. （迭代地搜索）遍历该node的所有<key,value>对，如果遇到k(i)>key，则选择k(i),v(i).???写错了吧
  // 3. 直到遍历到leafnode（如何判断？）遍历这个page（node）中所有的key，value对
  Context ctx(bpm_, header_page_id_); 
  // std::cout << "GetValue:header_page_id" << header_page_id_ << std::endl;
  ReadPageGuard guard = bpm_->FetchPageRead(header_page_id_);
  auto header_page = guard.As<BPlusTreeHeaderPage>();
  ctx.header_page_r_ = std::move(guard);
  ctx.root_page_id_ = header_page->root_page_id_; 
  // std::cout << "GetValue: root_page_id" << ctx.root_page_id_ << std::endl;
  guard = bpm_->FetchPageRead(ctx.root_page_id_);
  auto cur_page = guard.As<BPlusTreePage>();
  ctx.header_page_r_= std::move(guard);
  while(!cur_page->IsLeafPage()){  // 一定要写非递归的吗，还是可以写递归的？
    // 遍历这个node，比较
    auto *internal = reinterpret_cast<const InternalPage *>(cur_page);
    auto next_page_id = internal->FindNextNode(key, comparator_);
    guard = bpm_->FetchPageRead(next_page_id);
    cur_page = guard.As<BPlusTreePage>();
    ctx.read_set_.push_back(std::move(guard));
  }
  //已经到了叶子节点这一层
  // std::cout <<  "GetValue: leaf_page_id" << guard.PageId() << std::endl;
  auto *leaf = reinterpret_cast<const LeafPage *>(cur_page);
  ValueType rtvalue;
  bool flag = leaf->FindValueForKey(key, &rtvalue, comparator_);
  // std::cout << "GetValue: canfindkey" << flag << std::endl;
  if(flag == true) result->push_back(rtvalue);
  return flag;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *txn) -> bool {
  // std::cout << "Inserting..." << std::endl;
  // Declaration of context instance.
  Context ctx(bpm_, header_page_id_);

  ReadPageGuard guard = bpm_->FetchPageRead(header_page_id_); //不可以是xLock，否则根本不会有并发可言
  auto header_page = guard.As<BPlusTreeHeaderPage>();
  
  
  ctx.header_page_r_ = std::move(guard);  
  ctx.root_page_id_ = header_page->root_page_id_;
  
  // 树为空
  if(ctx.root_page_id_ == INVALID_PAGE_ID){ 
    // std::cout << "Inserting in an empty tree..." << std::endl;
    page_id_t cur_page_id;
    // leaf page
    if(bpm_->NewPage(&cur_page_id) == nullptr) return false;  // 返回的是一个page啊，我能用这个page指针做什么
    WritePageGuard writeGuard = bpm_->FetchPageWrite(cur_page_id);
    auto Leaf = writeGuard.AsMut<LeafPage>();
    ctx.write_set_.push_back(std::move(writeGuard));
    // initialize
    Leaf->Init(leaf_max_size_);
    Leaf->InsertKeyValueNotFull(key, value, comparator_);
    // 更新dummynode的ptr
    WritePageGuard wG = bpm_->FetchPageWrite(header_page_id_);
    ctx.header_page_r_ = std::nullopt;
    ctx.header_page_w_ = std::move(wG);
    auto header_page_1 = wG.AsMut<BPlusTreeHeaderPage>();
    header_page_1->root_page_id_ = cur_page_id;
    bpm_->FlushPage(header_page_id_);
    return true;
  }
  // std::cout << "inserting into an un-empty tree..." << std::endl;
  // 树不为空, ctx.root_page_id_ != INVALID_PAGE_ID
  auto readGuard = bpm_->FetchPageRead(ctx.root_page_id_);
  auto cur_page = readGuard.As<BPlusTreePage>();
  while(!cur_page->IsLeafPage()){  // 一定要写非递归的吗，还是可以写递归的？
    // 遍历这个node，比较
    auto *rinternal = reinterpret_cast<const InternalPage *>(cur_page);
    page_id_t next_page_id = rinternal->FindNextNode(key, comparator_);
    ctx.read_set_.push_back(std::move(readGuard));
    readGuard = bpm_->FetchPageRead(next_page_id);
    cur_page = readGuard.As<BPlusTreePage>();
  }
  ctx.read_set_.push_back(std::move(readGuard)); // 这次push_back的是叶子节点的guard
  //已经到了叶子节点这一层
  auto *rleaf = reinterpret_cast<const LeafPage *>(cur_page);
  ValueType tmpValue;
  if(rleaf->FindValueForKey(key, &tmpValue, comparator_))  return false; // key已经存在

  page_id_t new_page_id = INVALID_PAGE_ID;  // split后产生的page_id放在这里
  KeyType new_key;  // 存split后产生的page的第一个key
  KeyType old_key;  // 存split后老page的第一个key
  while(!ctx.read_set_.empty()){ 

    // 首先把read_set_转化成write_set
    // 先把read转换成write
    WritePageGuard writeGuard;
    ctx.PopReadGuardToWriteGuard(&writeGuard);
    // std::cout << "readGuard=====>writeGuard pageId: " << writeGuard.PageId() << std::endl;
    auto cur_w_page = writeGuard.AsMut<BPlusTreePage>();
    // 不会再进行split了
    if(cur_w_page->GetSize()<cur_w_page->GetMaxSize()){  //不需要转换成internal 或者 leaf 就能直接getSize了？
      // std::cout << "no more splitings!!!" << std::endl;
      if(cur_w_page->IsLeafPage()){ // IsLeafPage donot split
        // 1. 需要改，因为cur_page目前是const，不能转成非const，或许只能通过找pageid重新再fetch，这样才好。
        // 2. 这个cur_page目前指的是什么，是不是变量没有赋值，或者用了先前的变量。
        auto *wleaf = reinterpret_cast<LeafPage *>(cur_w_page);  // 这里不能再const LeafPage*了，因为const不能改变对象的内容。
        wleaf->InsertKeyValueNotFull(key, value, comparator_);
      }else{  // IsInternalPage donot split
        auto *winternal = reinterpret_cast<InternalPage *>(cur_w_page);
        winternal->InsertKeyValueNotFull(new_key, new_page_id, comparator_);
      }
      return true;
    }
    
    /*
     **************************************************************
                  这条分界线以上的代码出错的概率极小
     **************************************************************
    */
    // 需要split
    if(cur_w_page->IsLeafPage()){ // IsLeafPage split
      // std::cout << "needs to split a Leaf Page...." << std::endl;
      LeafPage *wleaf = reinterpret_cast<LeafPage *>(cur_w_page);
      
      // 先New一个Page
      if(bpm_->NewPage(&new_page_id) == nullptr) return false;  // 返回的是一个page啊，我能用这个page指针做什么
      writeGuard = bpm_->FetchPageWrite(new_page_id);
      LeafPage* newLeaf = writeGuard.AsMut<LeafPage>();
      // initialize
      newLeaf->Init(leaf_max_size_);
      ctx.write_set_.push_back(std::move(writeGuard));

      std::tie(old_key, new_key) = wleaf->SplitInsert(key, value, comparator_, newLeaf);
      // std::cout << "old_key:" << old_key << "new_key_:" << new_key << std::endl;

    }else{  // IsInternalPage
      InternalPage *winternal = reinterpret_cast<InternalPage *>(cur_w_page);
      KeyType insert_key = new_key;
      page_id_t insert_page_id = new_page_id; // 这个一定是有值的，为了防止newpage的时候把它冲掉

      // 先New一个Page
      if(bpm_->NewPage(&new_page_id) == nullptr) return false;  // 返回的是一个page啊，我能用这个page指针做什么
      writeGuard = bpm_->FetchPageWrite(new_page_id);
      auto newInternal = writeGuard.AsMut<InternalPage>();
      newInternal->Init(internal_max_size_);

      std::tie(old_key, new_key) = winternal->SplitInsert(insert_key, insert_page_id, comparator_, newInternal);
      // std::cout << "old_key: "<< old_key << " new_key: " << new_key << std::endl;
      ctx.write_set_.push_back(std::move(writeGuard));
    }
    
    
  }
  // std::cout << "allocating a new root page..." << std::endl;
  // 如果能运行到这儿，说明根节点已经split过了。这时需要new一个page作为新的root，并且将dummynode指向它
  // 先New一个Page
  page_id_t new_root_page_id = INVALID_PAGE_ID;
  if(bpm_->NewPage(&new_root_page_id) == nullptr) return false;  // 返回的是一个page啊，我能用这个page指针做什么
  WritePageGuard writeGuard = bpm_->FetchPageWrite(new_root_page_id);
  InternalPage* newRoot = writeGuard.AsMut<InternalPage>();
  ctx.write_set_.push_back(std::move(writeGuard));
  newRoot->Init(internal_max_size_);
  newRoot->InsertKeyValueNotFull(old_key, ctx.root_page_id_, comparator_); //第一个废节点的value指向old_key
  newRoot->InsertKeyValueNotFull(new_key, new_page_id, comparator_); //插入分裂出的节点, 改了从insert_xxx改成new_xxx了不知道对不对
  // 把dummynode指向newRoot
  writeGuard = bpm_->FetchPageWrite(header_page_id_);
  ctx.header_page_r_ = std::nullopt;
  ctx.header_page_w_ = std::move(writeGuard);
  auto h_page = writeGuard.AsMut<BPlusTreeHeaderPage>();
  h_page->root_page_id_ = new_root_page_id;
  // std::cout << "new_root_page_id: " << h_page->root_page_id_ << std::endl;
  return true;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *txn) {
  // Declaration of context instance.
  Context ctx(bpm_, header_page_id_);
  (void)ctx;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

/**
 * @return Page id of the root of this tree
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { 
  ReadPageGuard guard = bpm_->FetchPageRead(header_page_id_); //不可以是xLock，否则根本不会有并发可言
  auto header_page = guard.As<BPlusTreeHeaderPage>();
  return header_page->root_page_id_; 
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *txn) {
  int64_t key;
  std::ifstream input(file_name);
  while (input >> key) {
    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, txn);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *txn) {
  int64_t key;
  std::ifstream input(file_name);
  while (input >> key) {
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, txn);
  }
}

/*
 * This method is used for test only
 * Read data from file and insert/remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::BatchOpsFromFile(const std::string &file_name, Transaction *txn) {
  int64_t key;
  char instruction;
  std::ifstream input(file_name);
  while (input) {
    input >> instruction >> key;
    RID rid(key);
    KeyType index_key;
    index_key.SetFromInteger(key);
    switch (instruction) {
      case 'i':
        Insert(index_key, rid, txn);
        break;
      case 'd':
        Remove(index_key, txn);
        break;
      default:
        break;
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  auto root_page_id = GetRootPageId();
  auto guard = bpm->FetchPageBasic(root_page_id);
  PrintTree(guard.PageId(), guard.template As<BPlusTreePage>());
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::PrintTree(page_id_t page_id, const BPlusTreePage *page) {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<const LeafPage *>(page);
    std::cout << "Leaf Page: " << page_id << "\tNext: " << leaf->GetNextPageId() << std::endl;

    // Print the contents of the leaf page.
    std::cout << "Contents: ";
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i);
      if ((i + 1) < leaf->GetSize()) {
        std::cout << ", ";
      }
    }
    std::cout << std::endl;
    std::cout << std::endl;

  } else {
    auto *internal = reinterpret_cast<const InternalPage *>(page);
    std::cout << "Internal Page: " << page_id << std::endl;

    // Print the contents of the internal page.
    std::cout << "Contents: ";
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i);
      if ((i + 1) < internal->GetSize()) {
        std::cout << ", ";
      }
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      auto guard = bpm_->FetchPageBasic(internal->ValueAt(i));
      PrintTree(guard.PageId(), guard.template As<BPlusTreePage>());
    }
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Drawing an empty tree");
    return;
  }

  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  auto root_page_id = GetRootPageId();
  auto guard = bpm->FetchPageBasic(root_page_id);
  ToGraph(guard.PageId(), guard.template As<BPlusTreePage>(), out);
  out << "}" << std::endl;
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(page_id_t page_id, const BPlusTreePage *page, std::ofstream &out) {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<const LeafPage *>(page);
    // Print node name
    out << leaf_prefix << page_id;
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << page_id << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << page_id << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << page_id << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }
  } else {
    auto *inner = reinterpret_cast<const InternalPage *>(page);
    // Print node name
    out << internal_prefix << page_id;
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << page_id << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_guard = bpm_->FetchPageBasic(inner->ValueAt(i));
      auto child_page = child_guard.template As<BPlusTreePage>();
      ToGraph(child_guard.PageId(), child_page, out);
      if (i > 0) {
        auto sibling_guard = bpm_->FetchPageBasic(inner->ValueAt(i - 1));
        auto sibling_page = sibling_guard.template As<BPlusTreePage>();
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_guard.PageId() << " " << internal_prefix
              << child_guard.PageId() << "};\n";
        }
      }
      out << internal_prefix << page_id << ":p" << child_guard.PageId() << " -> ";
      if (child_page->IsLeafPage()) {
        out << leaf_prefix << child_guard.PageId() << ";\n";
      } else {
        out << internal_prefix << child_guard.PageId() << ";\n";
      }
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::DrawBPlusTree() -> std::string {
  if (IsEmpty()) {
    return "()";
  }

  PrintableBPlusTree p_root = ToPrintableBPlusTree(GetRootPageId());
  std::ostringstream out_buf;
  p_root.Print(out_buf);

  return out_buf.str();
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::ToPrintableBPlusTree(page_id_t root_id) -> PrintableBPlusTree {
  auto root_page_guard = bpm_->FetchPageBasic(root_id);
  auto root_page = root_page_guard.template As<BPlusTreePage>();
  PrintableBPlusTree proot;

  if (root_page->IsLeafPage()) {
    auto leaf_page = root_page_guard.template As<LeafPage>();
    proot.keys_ = leaf_page->ToString();
    proot.size_ = proot.keys_.size() + 4;  // 4 more spaces for indent

    return proot;
  }

  // draw internal page
  auto internal_page = root_page_guard.template As<InternalPage>();
  proot.keys_ = internal_page->ToString();
  proot.size_ = 0;
  for (int i = 0; i < internal_page->GetSize(); i++) {
    page_id_t child_id = internal_page->ValueAt(i);
    PrintableBPlusTree child_node = ToPrintableBPlusTree(child_id);
    proot.size_ += child_node.size_;
    proot.children_.push_back(child_node);
  }

  return proot;
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

/**
   * 自己写的函数，parse这一页的内容
   * 输入page_id，输出parse的结果，bplustreexxxpage*
   * 用模板来写，极有可能写错 不写了，干脆直接调guard的函数吧，受不鸟呢
*/

}  // namespace bustub

