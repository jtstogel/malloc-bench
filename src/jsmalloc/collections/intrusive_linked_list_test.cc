#include "src/jsmalloc/collections/intrusive_linked_list.h"

#include <cstddef>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace jsmalloc {

class TestItemList;

struct TestItem {
  uint64_t value;

  DEFINE_LINKED_LIST_NODE(TestItemList, TestItem, node);
};

DEFINE_LINKED_LIST(TestItemList, TestItem, node);

TEST(TestIntrusiveLinkedList, SingleElement) {
  TestItem fst{ .value = 1 };

  TestItemList ll;
  ll.insert_back(fst);

  EXPECT_FALSE(ll.empty());
  EXPECT_EQ(ll.front(), &fst);
  EXPECT_EQ(ll.back(), &fst);
}

TEST(TestIntrusiveLinkedList, Empty) {
  TestItemList ll;
  EXPECT_TRUE(ll.empty());
  EXPECT_EQ(ll.front(), nullptr);
  EXPECT_EQ(ll.back(), nullptr);
}

TEST(TestIntrusiveLinkedList, Iterates) {
  std::vector<TestItem> vals = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };
  TestItemList ll;
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  std::vector<uint64_t> got;
  for (auto& v : ll) {
    got.push_back(v.value);
  }
  EXPECT_THAT(got, testing::ElementsAre(1, 2, 3));
}

TEST(TestIntrusiveLinkedList, SupportsDeletion) {
  TestItem vals[] = {
    TestItem{ .value = 1 },
    TestItem{ .value = 2 },
    TestItem{ .value = 3 },
  };
  TestItemList ll;
  for (auto& v : vals) {
    ll.insert_back(v);
  }

  ll.remove(vals[1]);

  std::vector<uint64_t> got;
  for (const auto& v : ll) {
    got.push_back(v.value);
  }
  EXPECT_THAT(got, testing::ElementsAre(1, 3));
}

}  // namespace jsmalloc
