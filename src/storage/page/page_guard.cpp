#include "storage/page/page_guard.h"
#include "buffer/buffer_pool_manager.h"

namespace bustub {

BasicPageGuard::BasicPageGuard(BasicPageGuard &&that) noexcept {
    this->bpm_ = that.bpm_;
    this->is_dirty_ = that.is_dirty_;
    this->page_= that.page_;
    // std::cout << "move constructor:page address" << this->page_ << std::endl;
}

void BasicPageGuard::Drop() {
    
    bpm_->UnpinPage(PageId(), is_dirty_);
}

auto BasicPageGuard::operator=(BasicPageGuard &&that) noexcept -> BasicPageGuard & { return *this; }

BasicPageGuard::~BasicPageGuard(){};  // NOLINT

ReadPageGuard::ReadPageGuard(ReadPageGuard &&that) noexcept = default;

auto ReadPageGuard::operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard & { 
    this->guard_.bpm_ = that.guard_.bpm_;   // 因为是友元，所以可以访问私有的字段
    this->guard_.is_dirty_ = that.guard_.is_dirty_;
    this->guard_.page_ = that.guard_.page_;
    return *this; 
}

void ReadPageGuard::Drop() {
    guard_.page_->RUnlatch();   // 友元能不能访问私有成员
    // guard_.Drop();
    guard_.bpm_->UnpinPage(guard_.PageId(), guard_.is_dirty_);
}

ReadPageGuard::~ReadPageGuard() {
    guard_.Drop();
}  // NOLINT

WritePageGuard::WritePageGuard(WritePageGuard &&that) noexcept = default;

auto WritePageGuard::operator=(WritePageGuard &&that) noexcept -> WritePageGuard & { 
    this->guard_.bpm_ = that.guard_.bpm_;   // 因为是友元，所以可以访问私有的字段
    this->guard_.is_dirty_ = that.guard_.is_dirty_;
    this->guard_.page_ = that.guard_.page_;
    return *this; 
}

void WritePageGuard::Drop() {
    guard_.page_->WUnlatch();   // 友元能不能访问私有成员
    // guard_.bpm_->UnpinPage(guard_.PageId(), guard_.is_dirty_);
    guard_.Drop();
}

WritePageGuard::~WritePageGuard() {}  // NOLINT

}  // namespace bustub
