//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#include "mmap_vector.hpp"

#include <string>
#include <unordered_set>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

class Setup_map_test : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(Setup_map_test, ilist_to_set) {
  int vec_list[] = {1, 3, 5, 7, 9};

  std::unordered_set<int> my_set;

  const std::vector<int> ilist(&vec_list[0], &vec_list[5]);

  my_set.insert(ilist.begin(), ilist.end());

  for (auto v : vec_list) {
    EXPECT_TRUE(my_set.find(v) != my_set.end());
  }
  EXPECT_FALSE(my_set.find(100) != my_set.end());
}

TEST_F(Setup_map_test, mmap_vector_to_set) {
  mmap_lib::vector<int> dense("lgdb_bench", "mmap_vector_test_persist");
  dense.clear();
  dense.emplace_back(1);
  dense.emplace_back(3);
  dense.emplace_back(5);
  dense.emplace_back(7);
  dense.emplace_back(9);

  std::unordered_set<int> my_set;

  my_set.insert(dense.begin(), dense.end());

  for (auto v : {1, 3, 5, 7, 9}) {
    EXPECT_TRUE(my_set.find(v) != my_set.end());
  }
  EXPECT_FALSE(my_set.find(100) != my_set.end());
}

TEST_F(Setup_map_test, iter_test_set) {
  mmap_lib::vector<int> dense("lgdb_bench", "mmap_vector_test_persist");

  std::unordered_set<int> my_set;

  my_set.insert(dense.begin(), dense.end());

  for (auto v : {1, 3, 5, 7, 9}) {
    EXPECT_TRUE(my_set.find(v) != my_set.end());
  }

  EXPECT_FALSE(my_set.find(100) != my_set.end());

  dense.reserve(101);
  dense.set(100, 100);
}
