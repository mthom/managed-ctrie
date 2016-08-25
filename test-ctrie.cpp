#include <iostream>
#include <future>
#include <functional>
#include <string>
#include <type_traits>

#include "otf_ctrie.hpp"
#include "gtest/gtest.h"
#include "test-ctrie.hpp"

using namespace std;
using namespace kl_ctrie;

TEST_F(ctrie_tests, Contains64SingletonStringsOfEachChar)
{
  for(char c = 'a'; c <= 'z'; ++c) {
    for(int i = 1; i < 65; ++i) {
      auto ptr = ct.lookup(ctrie_string(i, c));
      
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(*ptr, i);
    }
  }
}

TEST_F(ctrie_tests, RemoveAndCantBeFound)
{
  const int* ptr = ct.remove("aaaaa");
  
  ASSERT_TRUE(ptr != nullptr);
  ASSERT_EQ(*ptr, 5);
  
  ct.insert("aaaaa", 5);
  ASSERT_NE(ct.lookup("aaaaa"), nullptr);
  ASSERT_EQ(*ct.lookup("aaaaa"), 5);
}

TEST_F(ctrie_tests, ConcurrentInsertsAndRemoves)
{
  auto length_adder = [this](const unsigned len) {
    for(unsigned lenn = len; lenn < 1000; lenn += 10) {
      for(char c = 'a'; c <= 'z'; ++c) {
	ct.insert(ctrie_string(lenn, c), lenn);
      }
    }
  };
  
  std::vector<std::future<void>> futures;
  futures.reserve(std::thread::hardware_concurrency());

  for(unsigned i = 0; i < std::thread::hardware_concurrency(); ++i)
    futures.push_back(std::async(length_adder, 65 + i));

  for(auto& future : futures)
    future.get();
  
  for(unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
    for(unsigned lenn = 65 + i; lenn < 1000; lenn += 10) {
      for(char c = 'a'; c <= 'z'; ++c) {
	const int* ptr = ct.lookup(ctrie_string(lenn, c));
	
	ASSERT_NE(ptr, nullptr);
	ASSERT_EQ(*ptr, lenn);
      }
    }
  }

  for(char c = 'a'; c <= 'z'; ++c) {
    for(unsigned i = 1; i < 65; ++i) {
      const int* ptr = ct.lookup(ctrie_string(i, c));
      
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(*ptr, i);
    }
  }

  auto length_remover = [this](const unsigned len) {
    for(unsigned lenn = len; lenn < 1000; lenn += 10)
      for(char c = 'a'; c <= 'z'; ++c) {
	ctrie_string key(lenn, c);
	ASSERT_NE(ct.lookup(key), nullptr);
	ct.remove(key);
      }
  };
  
  futures.clear();
  
  for(unsigned i = 0; i < std::thread::hardware_concurrency(); ++i)
    futures.push_back(std::async(length_remover, 65 + i));

  for(auto& future : futures)
    future.get();

  for(char c='a'; c <= 'z'; ++c) {
    for(unsigned i = 1; i < 65; ++i) {
      const int* ptr = ct.lookup(ctrie_string(i, c));
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(*ptr, i);
    }
  }

  for(unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
    for(unsigned lenn = 65 + i; lenn < 1000; lenn += 10) {    
      for(char c='a'; c <= 'z'; ++c) {
	ASSERT_EQ(ct.lookup(ctrie_string(lenn, c)), nullptr);
      }
    }
  }
}

    
TEST_F(ctrie_tests, InterleavedConcurrentInsertsLookupsAndRemoves)
{
  auto inserter_remover = [this](unsigned mul) {
    for(char c = 'a'; c <= 'z'; ++c) {
      for(unsigned i = 1; i <= 64 / std::thread::hardware_concurrency(); ++i) {
	if(i & 1)
	  ct.remove(ctrie_string(mul * i, c));
	else {
	  ct.insert(ctrie_string(65 * mul, c), 65 * mul);
	  ASSERT_NE(ct.lookup(ctrie_string(65 * mul, c)), nullptr);
	}
      }
    }
  };

  std::vector<std::future<void>> futures;
  futures.reserve(std::thread::hardware_concurrency());

  for(unsigned i = 1; i <= std::thread::hardware_concurrency(); ++i)
    futures.push_back(std::async(inserter_remover, i));
  
  for(auto& future : futures)
    future.get();

  for(char c = 'a'; c <= 'z'; ++c) {
    for(unsigned i = 1; i <= 64 / std::thread::hardware_concurrency(); ++i) {
      for(unsigned mul = 1; mul <= std::thread::hardware_concurrency(); ++mul) {
	if(i & 1) {
	  ASSERT_EQ(ct.lookup(ctrie_string(mul * i, c)), nullptr);
	} else {
	  ASSERT_NE(ct.lookup(ctrie_string(65 * mul, c)), nullptr);
	  ASSERT_EQ(*ct.lookup(ctrie_string(65 * mul, c)), 65 * mul);
	}
      }
    }
  }
}

TEST_F(ctrie_tests, SingleThreadedInsertsAndRemoves)
{
  for(unsigned lenn = 65; lenn < 2500; lenn += 10)
    for(char c = 'a'; c <= 'z'; ++c) {
      ct.insert(ctrie_string(lenn, c), lenn);
    }

  for(unsigned lenn = 65; lenn < 2500; lenn += 10)
    for(char c='a'; c <= 'z'; ++c) {
      ASSERT_NE(ct.lookup(ctrie_string(lenn, c)), nullptr);
    }

  for(unsigned lenn = 65; lenn < 2500; lenn += 10) {
    for(char c = 'a'; c <= 'z'; ++c) {
      ASSERT_NE(ct.lookup(ctrie_string(lenn, c)), nullptr);
      ct.remove(ctrie_string(lenn, c));
    }
  }

  for(unsigned lenn = 65; lenn < 2500; lenn += 10) {
    for(char c='a'; c <= 'z'; ++c) {
      ASSERT_EQ(ct.lookup(ctrie_string(lenn, c)), nullptr);
    }
  }
}

TEST_F(ctrie_tests, Snapshots) {
  otf_ctrie ss = ct.snapshot();

  mt()->set_root_callback([&ss, this]() {
      list<void*> ss_and_ct_roots = ss.ct_callback();
      ss_and_ct_roots.append(ct.ct_callback());

      return ss_and_ct_roots;      
    });

  for(char c = 'a'; c <= 'z'; ++c) {
    for(int i = 1; i < 65; ++i) {
      auto ptr = ss.lookup(ctrie_string(i, c));
      
      ASSERT_NE(ptr, nullptr);
      ASSERT_EQ(*ptr, i);
    }
  }
  
  for(unsigned lenn = 65; lenn < 2500; lenn += 10) 
    for(char c = 'a'; c <= 'z'; ++c) {
      ct.insert(ctrie_string(lenn, c), lenn);
    }

  for(unsigned lenn = 65; lenn < 2500; lenn += 10) 
    for(char c = 'a'; c <= 'z'; ++c) {
      auto ptr = ss.lookup(ctrie_string(lenn, c));
      
      ASSERT_EQ(ptr, nullptr);
    }  
}

int main(int argc, char** argv)
{
   gc::initialize();
   ::testing::InitGoogleTest(&argc, argv);

   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   RUN_ALL_TESTS();
   
   return RUN_ALL_TESTS();   
}
