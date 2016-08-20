#ifndef TEST_CTRIE_HPP_INCLUDED
#define TEST_CTRIE_HPP_INCLUDED

#include "gtest/gtest.h"
#include "gc.hpp"
#include "otf_ctrie.hpp"

using namespace otf_gc;

class ctrie_tests : public ::testing::Test
{
protected:
  std::thread collector_thread;
  otf_ctrie ct;
  
  virtual void SetUp()
  {
    collector_thread = std::thread([]() {
	gc::collector->template run<otf_ctrie::otf_ctrie_policy, otf_ctrie_tracer>();
      });
    
    for(char c = 'a'; c <= 'z'; ++c) {
      for(int i = 1; i < 65; ++i) {
	ct.insert(ctrie_string(i, c), i);
      }
    }
  }

  virtual void TearDown()
  {
    gc::collector->stop();

    std::this_thread::sleep_for(std::chrono::microseconds(5));
    
    if(collector_thread.joinable())
      collector_thread.join();
  }
};
#endif
