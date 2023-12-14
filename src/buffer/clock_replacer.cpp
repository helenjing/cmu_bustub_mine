//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// clock_replacer.cpp
//
// Identification: src/buffer/clock_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/clock_replacer.h"
#include <iostream>
namespace bustub {

ClockReplacer::ClockReplacer(size_t num_pages) {
    max_tot_cnt = num_pages;
    unpin_ptr = new Node(); // 头指针
    pin_ptr = new Node();   // 头指针
    unpin_ptr->next = unpin_ptr;
    unpin_ptr->pre = unpin_ptr;
    
}

//ClockReplacer::~ClockReplacer() = default;
ClockReplacer::~ClockReplacer(){
    while(pin_ptr != nullptr){
        Node* tmp = pin_ptr->next;
        delete pin_ptr;
        pin_ptr = tmp;
    }
    unpin_ptr->pre->next = nullptr;
    while(unpin_ptr != nullptr){
        Node* tmp = unpin_ptr->next;
        delete unpin_ptr;
        unpin_ptr = tmp;
    }
}


//TODO
/**
   * Remove the victim frame as defined by the replacement policy.
   * @param[out] frame_id id of frame that was removed, nullptr if no victim was found
   * @return true if a victim frame was found, false otherwise
   */
auto ClockReplacer::Victim(frame_id_t *frame_id) -> bool {
    //1. 找到需要victim的node
    //2. delete掉
    
    if(unpin_cnt == 0){
        frame_id = nullptr;
        return false; 
    }

    Node* ptr = unpin_ptr->pre;
    while(ptr->ref == true){
        //std::cout << ptr->id << std::endl;
        if(ptr != unpin_ptr)    ptr->ref = false;
        ptr = ptr->pre;
    }
    std::cout << "victim:" << std::endl;
    std::cout << ptr->id << std::endl;
    //std::cout << frame_id << std::endl;
    //std::cout << *frame_id << std::endl;
    *frame_id = ptr->id;

    //delete掉
    ptr = DeleteUnpin(ptr);

    delete ptr;
    return true;
}

//TODO
  /**
   * Pins a frame, indicating that it should not be victimized until it is unpinned.
   * @param frame_id the id of the frame to pin
   */
void ClockReplacer::Pin(frame_id_t frame_id) {
    // 1. 找到这个Node（unpin_ptr, pin_ptr）
    // 2. 找不到就自己分配一个
    // 3. 插入unpin的队伍

    if(FindPin(frame_id) != nullptr)  return;

    Node* ptr = FindUnpin(frame_id);
    if(ptr != nullptr){
        ptr = DeleteUnpin(ptr);
    }else{
        ptr = new Node();
        ptr->isPinned = true;
        ptr->id = frame_id;
    }
    InsertPin(ptr);
}

//TODO
  /**
   * Unpins a frame, indicating that it can now be victimized.
   * @param frame_id the id of the frame to unpin
   */
void ClockReplacer::Unpin(frame_id_t frame_id) {
    // 1. 找到这个Node（unpin_ptr, pin_ptr）
    // 2. 找不到就自己分配一个
    // 3. 插入pin的队伍
    if(FindUnpin(frame_id) != nullptr)  return;

    Node* ptr = FindPin(frame_id);
    if(ptr != nullptr){
        ptr = DeletePin(ptr);
    }else{
        ptr = new Node();
        ptr->isPinned = false;
        ptr->id = frame_id;
    }
    InsertUnpin(ptr);
}

//TODO
 /** @return the number of elements in the replacer that can be victimized */
auto ClockReplacer::Size() -> size_t { 
    return unpin_cnt; 
}

/**
 * private functions 
*/
ClockReplacer::Node* ClockReplacer::FindUnpin(frame_id_t frame_id){
    // 如果没找到就返回nullptr, 找到就返回对应的指针。
    Node* ptr = unpin_ptr->pre;
    while(ptr != unpin_ptr){
        if(ptr->id == frame_id)   return ptr;
        ptr = ptr->next;
    }
    return nullptr;
}

ClockReplacer::Node* ClockReplacer::FindPin(frame_id_t frame_id){
    // 如果没找到就返回nullptr, 找到就返回对应的指针。
    Node* ptr = pin_ptr->next;
    while(ptr != nullptr){
        if(ptr->id == frame_id)   return ptr;
        ptr = ptr->next;
    }
    return nullptr;
}

void ClockReplacer::InsertUnpin(Node* ptr){
    //头插处理双链表，处理cnt
    ptr->next = unpin_ptr->next;
    ptr->pre = unpin_ptr;
    unpin_ptr->next = ptr;
    ptr->next->pre = ptr;

    ptr->isPinned = false;
    unpin_cnt++;
}

void ClockReplacer::InsertPin(Node* ptr){
    //头插处理双链表，处理cnt
    ptr->next = pin_ptr->next;
    pin_ptr->next = ptr;
    if(ptr->next != nullptr)    ptr->next->pre = ptr;
    ptr->pre = pin_ptr;

    ptr->isPinned = true;
    pin_cnt++;
}

ClockReplacer::Node* ClockReplacer:: DeleteUnpin(Node* ptr){
    //头插处理双链表，处理cnt
    ptr->pre->next = ptr->next;
    ptr->next->pre = ptr->pre;

    ptr->isPinned = true;
    ptr->pre = nullptr;
    ptr->next = nullptr;
    unpin_cnt--;
    return ptr;
}

ClockReplacer::Node* ClockReplacer:: DeletePin(Node* ptr){
    //头插处理双链表，处理cnt
    ptr->pre->next = ptr->next;
    if(ptr->next != nullptr)    ptr->next->pre = ptr->pre;
    
    ptr->isPinned = false;
    ptr->pre = nullptr;
    ptr->next = nullptr;
    pin_cnt--;
    return ptr;
}

}  // namespace bustub
