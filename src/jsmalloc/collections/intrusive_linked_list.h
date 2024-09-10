#pragma once

#include <cstddef>
#include <cstdint>

#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

template <typename T, ptrdiff_t Offset>
class IntrusiveLinkedList;

class LLNode {
 public:
  LLNode() : LLNode(nullptr, nullptr) {}

 private:
  template <typename, ptrdiff_t>
  friend class IntrusiveLinkedList;

  LLNode(LLNode* prev, LLNode* next) : next_(next), prev_(prev) {}

  /** Inserts `this` after `node`. */
  void insert_after(LLNode& node) {
    next_ = node.next_;
    prev_ = &node;
    DCHECK_NON_NULL(node.next_)->prev_ = this;
    node.next_ = this;
  }

  /** Inserts `this` before `node`. */
  void insert_before(LLNode& node) {
    next_ = &node;
    prev_ = node.prev_;
    node.prev_ = this;
    DCHECK_NON_NULL(prev_)->next_ = this;
  }

  void remove() {
    DCHECK_NON_NULL(this->prev_)->next_ = this->next_;
    DCHECK_NON_NULL(this->next_)->prev_ = this->prev_;
    this->prev_ = nullptr;
    this->next_ = nullptr;
  }

  bool linked() const {
    return next_ != nullptr;
  }

  LLNode* next_ = nullptr;
  LLNode* prev_ = nullptr;
};

template <typename T, intptr_t Offset>
class IntrusiveLinkedList {
  friend LLNode;

 public:
  explicit IntrusiveLinkedList() : head_(&head_, &head_) {}

  class Iterator {
    friend IntrusiveLinkedList;

   public:
    Iterator& operator++() {
      curr_ = curr_->next_;
      return *this;
    }

    Iterator& operator--() {
      curr_ = curr_->prev_;
      return *this;
    }

    bool operator==(const Iterator& other) {
      return curr_ == other.curr_;
    }

    bool operator!=(const Iterator& other) {
      return !(*this == other);
    }

    T& operator*() {
      return IntrusiveLinkedList<T, Offset>::Item(*curr_);
    }

   private:
    explicit Iterator(LLNode& node) : curr_(&node) {}

    LLNode* curr_;
  };

  Iterator begin() {
    return Iterator(*head_.next_);
  }

  Iterator end() {
    return Iterator(head_);
  }

  /**
   * Returns whether `el` is in this list.
   *
   * Assumes that `el` _could_ only be in this list.
   */
  bool empty() const {
    return head_.next_ == &head_;
  }

  /**
   * Returns whether `el` is in this list.
   *
   * Assumes that `el` _could_ only be in this list.
   */
  bool contains(T& el) const {
    return Node(el).linked();
  }

  /**
   * Removes `el` from this list.
   *
   * Assumes that `el` is in the list.
   */
  void remove(T& el) {
    Node(el).remove();
  }

  void insert_back(T& el) {
    Node(el).insert_before(head_);
  }

  void insert_front(T& el) {
    Node(el).insert_after(head_);
  }

  T* front() {
    if (empty()) {
      return nullptr;
    }
    return &Item(*head_.next_);
  }

  T* back() {
    if (empty()) {
      return nullptr;
    }
    return &Item(*head_.prev_);
  }

 private:
  static constexpr T& Item(LLNode& node) {
    return *twiddle::AddPtrOffset<T>(&node, -Offset);
  }

  static constexpr LLNode& Node(T& item) {
    return *twiddle::AddPtrOffset<LLNode>(&item, Offset);
  }

  LLNode head_;
};

#define DEFINE_LINKED_LIST_NODE(ClassName, ItemName, node_field) \
  ::jsmalloc::LLNode node_field;                                 \
  friend ClassName;

#define DEFINE_LINKED_LIST(ClassName, ItemName, node_field) \
  class ClassName : public ::jsmalloc::IntrusiveLinkedList< \
                        ItemName, offsetof(ItemName, node_field)> {}

}  // namespace jsmalloc
