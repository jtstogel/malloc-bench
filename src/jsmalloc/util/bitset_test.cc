#include "src/jsmalloc/util/bitset.h"

#include "gtest/gtest.h"

#include "src/jsmalloc/util/allocable.h"

namespace jsmalloc {

TEST(BitSetTest, SetAndTest) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->Test(0), false);
  b->Set(0, true);
  EXPECT_EQ(b->Test(0), true);
  b->Set(0, false);
  EXPECT_EQ(b->Test(0), false);
}

TEST(BitSetTest, CountTrailingOnes) {
  auto b = MakeAllocable<BitSet64>(10);
  EXPECT_EQ(b->FindFirstUnsetBit(), 0);
  b->Set(0, true);
  EXPECT_EQ(b->FindFirstUnsetBit(), 1);
}

TEST(BitSet4096Test, SetAndTest) {
  auto b = MakeAllocable<BitSet4096>(200);
  EXPECT_EQ(b->Test(0), false);
  b->Set(0, true);
  EXPECT_EQ(b->Test(0), true);
  b->Set(0, false);
  EXPECT_EQ(b->Test(0), false);
}

TEST(BitSet4096Test, CountTrailingOnesBasic) {
  auto b = MakeAllocable<BitSet4096>(200);
  EXPECT_EQ(b->FindFirstUnsetBit(), 0);
  b->Set(0, true);
  EXPECT_EQ(b->FindFirstUnsetBit(), 1);
}

TEST(BitSet4096Test, CountTrailingOnesAcrossMultipleLevels) {
  auto b = MakeAllocable<BitSet4096>(200);
  for (int i = 0; i < 200; i++) {
    b->Set(i, true);
    EXPECT_EQ(b->FindFirstUnsetBit(), i + 1);
  }

  for (int i = 200 - 1; i >= 0; i--) {
    b->Set(i, false);
    EXPECT_EQ(b->FindFirstUnsetBit(), i);
  }
}

TEST(BitSet4096Test, CountTrailingOnesSparse) {
  auto b = MakeAllocable<BitSet4096>(200);
  for (int i = 0; i < 200; i++) {
    b->Set(i, true);
  }

  b->Set(66, false);
  EXPECT_EQ(b->FindFirstUnsetBit(), 66);
}

TEST(BitSet512Test, SetAndTest) {
  auto b = MakeAllocable<BitSet512>(200);
  EXPECT_EQ(b->Test(0), false);
  b->Set(0, true);
  EXPECT_EQ(b->Test(0), true);
  b->Set(0, false);
  EXPECT_EQ(b->Test(0), false);
}

TEST(BitSet512Test, CountTrailingOnesBasic) {
  auto b = MakeAllocable<BitSet512>(200);
  EXPECT_EQ(b->FindFirstUnsetBit(), 0);
  b->Set(0, true);
  EXPECT_EQ(b->PopCount(), 1);
  EXPECT_EQ(b->FindFirstUnsetBit(), 1);
}

TEST(BitSet, StaticallyAllocated) {
  BitSet<201> b;
  EXPECT_FALSE(b.Test(200));
  b.Set(200, true);
  EXPECT_TRUE(b.Test(200));
}

}  // namespace jsmalloc
