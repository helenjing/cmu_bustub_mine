//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"

/*
 ***************************************
 ！！
        not_full_的逻辑有错，不是LRU
        需要改数据结构，改成priority_queue
 ***************************************
*/
namespace bustub {

/*
 ****************************************
               LRUKNode
 ****************************************
*/
LRUKNode::LRUKNode(size_t replacer_k, frame_id_t frame_id):k_(replacer_k), fid_(frame_id){};    // 明白了写分号，但是花括号也不能少

void LRUKNode::SetEvictable(bool set_evictable){
    is_evictable_ = set_evictable;
}

auto LRUKNode::isEvictable()->bool{
    return is_evictable_;
}

auto LRUKNode::GetHistorySize()->size_t{
    return history_.size();
}

void LRUKNode::NewVisit(size_t timestamp){
    history_.emplace_back(timestamp);
    
}

auto LRUKNode::GetRegisterTimestamp()->size_t{
    return register_timestamp_;
}

auto LRUKNode::GetLastKTimestamp()->size_t{
    std::list<size_t>::iterator it = history_.end();
    for(size_t i = 0; i < k_; i++){ // ！！！可算糊弄过去。list没有重载[]，没有加法和减法。回头去求证一下 
        it--;
    }
    return (history_.size()<k_)? -1 : *(it);  // 数据类型不兼容了，强转吧
    // return (history_.size()<k_)? -1 : history_[(history_.size()-k_)];  // 数据类型不兼容了，强转吧
}

// 这个是更新register_timestamp字段的。
void LRUKNode::FlushKTimestamp(){
    register_timestamp_ = GetLastKTimestamp();
}
// 通知已经被full_踢下去了，需要找机会自己再register一下。
void LRUKNode::KickOff(bool on_off){
    kicked_off_=on_off;
}

auto LRUKNode::isRegistered()->bool{
    return !kicked_off_;
}

auto LRUKNode::GetQueueTag()->bool{
    return queue_tag_;
}
void LRUKNode::SetQueueTag(bool queue_tag){
    queue_tag_ = queue_tag;
}
void LRUKNode::Clear(){
  history_.clear();
  is_evictable_ = true; //因为在调用Clear之后，会调用SetEvictable，进行replacer的metadata的更改，所以这里保持不变
  kicked_off_ = true;  // 目前是没有登记上的状态
  queue_tag_ = 0; 
    
}

/*
 ****************************************
               LRUKReplacer
 ****************************************
*/
LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {
}

//TODO 析构函数也得自己写
LRUKReplacer::~LRUKReplacer(){
    // 内存泄漏的问题不出在unordered_map的value应该delete掉
    // unordered_map insert是浅拷贝，不是深拷贝。
    // 因此我们能做的是把传进去的值（对象），delete掉
}

//TODO
// Evict的时候需要把frame的meta-data重置
auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool { 
    // 先查找not_full_
    std::list<frame_id_t>::iterator it = not_full_.begin();
    while(it != not_full_.end()){  
        if(node_store_.find(*it)==node_store_.end())   not_full_.erase(it++);   // 遇到不合法的
        else if(node_store_.at(*it).isEvictable()==false){ // pin上了，没办法用
            it++;
        }else if(node_store_.at(*it).GetHistorySize()<k_){ // 没满，可以evict
            *frame_id = *it;
            node_store_.at(*it).Clear();    // 重置这一页
            SetEvictable(*frame_id, false);
            not_full_.erase(it);  //把这个元素从not_full_中请出去
            return true;
        }else {  // 满了，从not_full_中踢出去。有些需要转移到full_
            if(node_store_.at(*it).GetQueueTag()==0 && node_store_.at(*it).isRegistered()==true){
                // 如果是登记状态，超过了k_，但是还在not_full_队列中的
                node_store_.at(*it).FlushKTimestamp();   // 这三步应该加锁的
                node_store_.at(*it).SetQueueTag(1);   // 这三步应该加锁的
                full_.push({*it, node_store_.at(*it).GetLastKTimestamp()}); 
            }
            not_full_.erase(it++);  //删除当前元素，并且自增，否则就会变成野指针？
        }
    }
    // 再查找full_
    while(!full_.empty()){
        frame_id_t top_frame = full_.top().first;
        // 遇到不合法的怎么办：
        if(node_store_.find(top_frame) == node_store_.end()){
            full_.pop();
            continue;
        }
        // 检查，是否是可以evict的
        if(node_store_.at(top_frame).isEvictable() == false){
            node_store_.at(top_frame).KickOff(true); // 通知他被踢下去了，下次自己再register一下
            full_.pop();
            continue;
        }
        // 检查，是否last_k_timestamp有更新
        if(node_store_.at(top_frame).GetRegisterTimestamp() != node_store_.at(top_frame).GetLastKTimestamp()){
            full_.pop();  
            node_store_.at(top_frame).FlushKTimestamp();   // 这三步应该加锁的
            full_.push({top_frame, node_store_.at(top_frame).GetLastKTimestamp()}); 
            continue;
        }
        // 终于合格了，给他请出去
        *frame_id = top_frame;
        // evict函数是提供候选者，还是真的噶了这一页？
        node_store_.at(*frame_id).Clear();    // 重置这一页
        SetEvictable(*frame_id, false);
        full_.pop();    // 把它从priority_queue中请出去
        return true;
    }
    return false; 
}

// TODO，insert 一个 access history，应该append在history_的后面
// 我貌似把setEvictable和RecordAccess弄混了，创建LRUKNode是应该在这里吧
void LRUKReplacer::RecordAccess(frame_id_t frame_id, [[maybe_unused]] AccessType access_type) {
    if(node_store_.find(frame_id) == node_store_.end()){
        if(curr_tot_size_ == replacer_size_)    throw Exception("replacer full!");
        LRUKNode* temp_lruknode = new LRUKNode(k_, frame_id);

        node_store_.insert(std::make_pair(frame_id,*temp_lruknode));    // at如果遇到不存在的key，会throw exception
        //------新增
        delete temp_lruknode;
        curr_tot_size_++;
       
    }
    node_store_.at(frame_id).NewVisit(current_timestamp_++);
    
}


// TODO
void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
    if(node_store_.find(frame_id) == node_store_.end()){
        throw Exception("invalid frame_id!");
    }
    // 这貌似是一个统一的逻辑(不管是新的frame还是已经在replacer的frame)
    if(set_evictable == true){  // 能够被替换
        if(node_store_.at(frame_id).isEvictable() == true) return; //本来就能替换的话就不管它
        curr_size_++;   // meta-data需要更改
        node_store_.at(frame_id).SetEvictable(true);
        // 会不会遇到多次在一个list上登记的情况？？
        // re：会的，因为priorityqueue只能访问最top的元素，如果遇到一个pin上的，就不能接续下去，只能pop掉
        // -------- 回头再考虑如何登记一致性的问题---------
        if(node_store_.at(frame_id).isRegistered() && node_store_.at(frame_id).GetHistorySize() != k_) return;    // ==k的时候要转换
        // 在not_full_登记
        if(node_store_.at(frame_id).GetHistorySize() < k_){
            node_store_.at(frame_id).KickOff(false);
            not_full_.emplace_back(frame_id);
        }else{ // 在full_登记
            node_store_.at(frame_id).FlushKTimestamp();
            node_store_.at(frame_id).KickOff(false);
            full_.push({frame_id, node_store_.at(frame_id).GetLastKTimestamp()}); 
            // full_.emplace(frame_id);   // 这貌似有不小的问题
        }
    }else{  //现在不希望被替换了
        if(node_store_.at(frame_id).isEvictable()== true)   curr_size_--;   //meta-data需要改
        node_store_.at(frame_id).SetEvictable(false);
    }
    
}

// TODO
void LRUKReplacer::Remove(frame_id_t frame_id) {
    if(node_store_.find(frame_id) == node_store_.end()) return;
    if(node_store_.at(frame_id).isEvictable() == false)    throw Exception("the frame is pinned now!");
    else{
        curr_tot_size_--;
        curr_size_--;
        node_store_.erase(frame_id);
    }
}

// TODO
auto LRUKReplacer::Size() -> size_t { 
    return curr_size_; 
}

}  // namespace bustub
