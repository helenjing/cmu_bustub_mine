//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

#include "common/exception.h"
#include "common/macros.h"
#include "storage/page/page_guard.h"

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // TODO(students): remove this line after you have implemented the buffer pool manager
  // throw NotImplementedException(
  //     "BufferPoolManager is not implemented yet. If you have finished implementing BPM, please remove the throw "
  //     "exception line in `buffer_pool_manager.cpp`.");

  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

// TODO
// 没考虑多线程
// debug:指针和引用弄混了，每一行都是错误，因为指针全用错了
auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * { 
  // 先找到一个空的frame
  frame_id_t frame_id = -1;
  if(FindOrEvictFrame(&frame_id) == false) return nullptr;

  // 拿到一张空的page了（frame_id），分配一个page_id
  // 貌似这个函数就是模拟一下，从硬盘中取出页号为多少的页。这里就直接顺序数字模拟了
  *page_id = AllocatePage();
  pages_[frame_id].page_id_ = *page_id; // 拿到了分配的号，没使用（没有把他存到page_的meta-data中，这谁找得到）
  // 和page_table_挂钩
  page_table_[*page_id] = frame_id;
  // 需要把page内部的meta-data写一下吗
  return &pages_[frame_id];  // 页框号能够找到这个页  
}

// TODO
// 还是指针出问题了
auto BufferPoolManager::FetchPage(page_id_t page_id, [[maybe_unused]] AccessType access_type) -> Page * {
  //1. 先从页表中查找是否有这个页
  if(page_table_.find(page_id) != page_table_.end()){
    pages_[page_table_[page_id]].pin_count_++;
    // std::cout << "exist in buffer pool" << page_table_[page_id] << std::endl;
    replacer_->SetEvictable(page_table_[page_id], false);
    return &pages_[page_table_[page_id]]; // 这个类型乱七八糟的
  }  
  frame_id_t frame_id = -1;
  // 2. 和NewPage很类似，获得一个空的frame
  if(FindOrEvictFrame(&frame_id)==false) return nullptr;
  pages_[frame_id].pin_count_++;  // 20231208 project2，FetchPageWrite latch报错，加上了pin
  disk_manager_->ReadPage(page_id, pages_[frame_id].data_); //这是不是太不安全了
  pages_[frame_id].page_id_ = page_id;
  page_table_[page_id] = frame_id;
  return &pages_[frame_id];
}

// TODO
auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty, [[maybe_unused]] AccessType access_type) -> bool {
  // 1. 查找 page_id是否存在，不存在返回false，查页表
  if(page_table_.find(page_id) == page_table_.end())  return false;
  frame_id_t frame_id = page_table_[page_id];
  // 2. 查看pin_count，0返回false，否则--pin_count，并设置is_dirty
  if(pages_[frame_id].pin_count_<=0)  return false;
  pages_[frame_id].pin_count_--; // 友元类好像是可以访问私有成员的
  if(!pages_[frame_id].pin_count_)  replacer_->SetEvictable(frame_id, true);
  pages_[frame_id].is_dirty_ |= is_dirty;  // 或运算，感觉无论pin的有多少个进程，脏位是一定要更新的，不能只有最后一个决定
  return true;
  
}

//TODO
// 没考虑多线程
auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool { 
  // 1. 查找 page_id是否存在，不存在返回false，查页表
  if(page_table_.find(page_id) == page_table_.end())  return false;
  frame_id_t frame_id = page_table_[page_id];

  // 1. 写回disk中
  disk_manager_->WritePage(pages_[frame_id].GetPageId(), pages_[frame_id].GetData());
  // 2. 重置dirty
  pages_[frame_id].is_dirty_ = false;
  return true;

}

// TODO
// 没考虑多线程
void BufferPoolManager::FlushAllPages() {
  for(size_t i = 0; i < pool_size_; i++){
    if(pages_[i].GetPageId() != INVALID_PAGE_ID){
      // 1. 写回disk中
      disk_manager_->WritePage(pages_[i].GetPageId(), pages_[i].GetData());
      // 2. 重置dirty
      pages_[i].is_dirty_ = false;
    }
  }
}

// TODO
// 没考虑多线程
auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool { 
  // 1. 查找 page_id是否存在，不存在返回false，查页表
  if(page_table_.find(page_id) == page_table_.end())  return true;
  frame_id_t frame_id = page_table_[page_id];
  // 判断是否能够delete，只判断是否pin吧，如果unpin就看是否需要写回。
  if(pages_[frame_id].GetPinCount() > 0)  return false;
  if(pages_[frame_id].IsDirty()) FlushPage(page_id); // 这里事实上是做了重复的工作

  // 把这一页从clock_replacer的环上取下来，增加到freelist中
  replacer_->Remove(frame_id); // 这里是写page_id,还是写frame_id呢
  free_list_.emplace_back(static_cast<int>(frame_id));
  // reset the page's memory and meta-data
  ResetFrame(frame_id);
  // deallocate the page
  DeallocatePage(page_id);  //这是干嘛的，需要前后加latch，也没做
  return true;
}

auto BufferPoolManager::AllocatePage() -> page_id_t { return next_page_id_++; }

auto BufferPoolManager::FetchPageBasic(page_id_t page_id) -> BasicPageGuard { 
  Page* available_page = FetchPage(page_id);
  if(available_page == nullptr) return {this, nullptr};
  return {this, available_page}; 
}

auto BufferPoolManager::FetchPageRead(page_id_t page_id) -> ReadPageGuard { 
  // std::cout << "FetchPageReading..." << std::endl;
  Page* available_page = FetchPage(page_id);  // 第二个参数应该还是不写
  if(available_page == nullptr) return {this, nullptr};
  // 上read latch  
  // available_page->RLatch();
  // std::cout << "Read Latching..." << std::endl;
  return {this, available_page}; 
}

// TODO
// 上锁了，类中的latch_字段更新， 解锁在哪里
auto BufferPoolManager::FetchPageWrite(page_id_t page_id) -> WritePageGuard { 
  Page* available_page = FetchPage(page_id);  // 第二个参数应该还是不写
  if(available_page == nullptr) return {this, nullptr};
  // 上write latch
  // available_page->WLatch();
  return {this, available_page}; 
}

auto BufferPoolManager::NewPageGuarded(page_id_t *page_id) -> BasicPageGuard { 
  Page* available_page = NewPage(page_id);
  if(available_page == nullptr) return {this, nullptr};
  return {this, available_page}; 
}

// TODO
// 上锁了，类中的latch_字段更新， 解锁在哪里
// 这个要不要pin_count++啊
void BufferPoolManager::ResetFrame(frame_id_t frame_id) {
  // reset the page's memory and meta-data
  pages_[frame_id].ResetMemory();
  // meta-data不知道找齐了没有
  pages_[frame_id].page_id_ = INVALID_PAGE_ID;
  pages_[frame_id].is_dirty_ = false;

  pages_[frame_id].pin_count_=0;
}

auto BufferPoolManager::FindOrEvictFrame(frame_id_t* frame_id) -> bool {
  // 1. 先在free_list中找
  // std::cout << "FindorEvictFrame...." << std::endl;
  if(free_list_.empty() == false){
    // std::cout << "candidate found in free_list_..." << std::endl;
    *frame_id = *free_list_.begin();  // 不知道语法对不对
    replacer_->RecordAccess(*frame_id); // 不知道怎么回事，必须得new一个LRUKNode啊
    free_list_.pop_front();
  }else{
    if(replacer_->Evict(frame_id) == false)  return false; 
    // 1. 找到候选者了，如果是dirty的，把内容flush进硬盘
    if(pages_[*frame_id].is_dirty_) FlushPage(pages_[*frame_id].page_id_);
    // 2. 把这一页从replacer上取下来，page_table_取消对应关系
    page_table_.erase(pages_[*frame_id].page_id_ );
    // 3. 写入硬盘，初始化page
    ResetFrame(*frame_id);
  }
  // 要用这一页了，所以pin上，也不能evict了
  pages_[*frame_id].pin_count_++;
  replacer_->SetEvictable(*frame_id, false);  // replacer是指针还是对象啊

  return true;
}

}  // namespace bustub
