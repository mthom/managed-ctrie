#ifndef TEST_CTRIE_HPP_INCLUDED
#define TEST_CTRIE_HPP_INCLUDED

#include <future>

#include "gtest/gtest.h"
#include "gc.hpp"
#include "otf_ctrie.hpp"

using namespace otf_gc;

class ctrie_tests : public ::testing::Test
{
protected:
  otf_ctrie ct;
  
  virtual void SetUp()
  {
    for(char c = 'a'; c <= 'z'; ++c) {
      for(int i = 1; i < 65; ++i) {
	ct.insert(ctrie_string(i, c), i);
      }
    }
  }

  virtual void TearDown()
  {}
};
#endif
