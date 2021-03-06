/*
 * Copyright 2014 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thrift/lib/cpp2/protocol/CompactProtocol.h>
#include <thrift/lib/cpp2/protocol/BinaryProtocol.h>
#include <thrift/lib/cpp2/protocol/SimpleJSONProtocol.h>
#include <thrift/lib/cpp/protocol/TBinaryProtocol.h>
#include <thrift/lib/cpp/protocol/TCompactProtocol.h>
#include <thrift/lib/cpp/protocol/TSimpleJSONProtocol.h>
#include <thrift/lib/cpp/transport/TBufferTransports.h>

#include "thrift/test/gen-cpp2/DebugProtoTest_types.h"
#include "thrift/test/gen-cpp/DebugProtoTest_types.h"

#include <math.h>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

using namespace thrift::test::debug;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace std;

namespace thrift { namespace test { namespace debug {

bool Empty::operator<(Empty const& other) const {
  // It is empty, so all are equal.
  return false;
}

}}}

cpp2::OneOfEach ooe;
cpp2::Nested nested;

map<string, vector<set<map<int, int>>>> nested_foo = {
  {"foo",
    {
      {
        { {3, 2}, {4, 5} },
        { {2, 1}, {1, 6} }
      }
    }
  },
  {"bar",
    {
      {
        { {1, 0}, {5, 0} }
      },
      {
        { {0, 0}, {5, 5} }
      }
    }
  }
};

template <typename Object>
void testOoe(Object& ooe) {
  ASSERT_EQ(ooe.im_true, true);
  ASSERT_EQ(ooe.im_false, false);
  ASSERT_EQ((uint8_t)ooe.a_bite, 0xd6);
  ASSERT_EQ(ooe.integer16, 27000);
  ASSERT_EQ(ooe.integer32, 1 << 24);
  ASSERT_EQ(ooe.integer64, (uint64_t)6000 * 1000 * 1000);
  ASSERT_EQ(ooe.double_precision, M_PI);
  ASSERT_EQ(ooe.float_precision, (float)12.345);
  ASSERT_EQ(ooe.some_characters, "JSON THIS! \"\1");
  ASSERT_EQ(ooe.zomg_unicode, "\xd7\n\a\t");
  ASSERT_EQ(ooe.base64, "\1\2\3\255");
  ASSERT_EQ(ooe.rank_map.size(), 2);
  ASSERT_EQ(ooe.rank_map[567419810], (float)0.211184);
  ASSERT_EQ(ooe.rank_map[507959914], (float)0.080382);
}


template <typename Object>
void testNested(Object& obj) {
  ASSERT_EQ(nested_foo, obj.foo);
  ASSERT_EQ(42, obj.bar);
}

void testObj(cpp2::OneOfEach& obj) { testOoe(obj); }
void testObj(OneOfEach& obj) { testOoe(obj); }
void testObj(cpp2::Nested& obj) { testNested(obj); }
void testObj(Nested& obj) { testNested(obj); }

template <typename Cpp2Writer, typename Cpp2Reader, typename CppProtocol,
          typename CppType, typename Cpp2Type>
void runTest(Cpp2Type& obj) {
  Cpp2Type obj2;
  Cpp2Writer prot;
  Cpp2Reader protReader;

  // Verify writing with cpp2
  size_t bufSize = obj.serializedSize(&prot);
  IOBufQueue queue(IOBufQueue::cacheChainLength());

  prot.setOutput(&queue, bufSize);
  obj.write(&prot);

  bufSize = queue.chainLength();
  auto buf = queue.move();

  // Try deserialize with cpp2
  protReader.setInput(buf.get());
  obj2.read(&protReader);
  testObj(obj2);

  // Try deserialize with cpp
  std::shared_ptr<TTransport> buf2(
    new TMemoryBuffer(buf->writableData(), bufSize));
  CppProtocol cppProt(buf2);
  CppType cppObj;
  cppObj.read(&cppProt);
  testObj(cppObj);

  // Try to serialize with cpp
  buf2.reset(new TMemoryBuffer());
  CppProtocol cppProt2(buf2);
  cppObj.write(&cppProt2);

  std::string buffer = dynamic_pointer_cast<TMemoryBuffer>(
    buf2)->getBufferAsString();
  std::unique_ptr<folly::IOBuf> buf3(
    folly::IOBuf::wrapBuffer(buffer.data(), buffer.size()));

  protReader.setInput(buf3.get());
  obj2.read(&protReader);
  testObj(obj2);
}

template <typename Cpp2Writer, typename Cpp2Reader>
void runSkipTest() {
  Cpp2Writer prot;
  Cpp2Reader protReader;
  size_t bufSize = nested.serializedSize(&prot);
  IOBufQueue queue(IOBufQueue::cacheChainLength());
  prot.setOutput(&queue, bufSize);
  nested.write(&prot);

  bufSize = queue.chainLength();
  auto buf = queue.move();
  protReader.setInput(buf.get());
  cpp2::NotNested notNested;
  notNested.read(&protReader);
  ASSERT_EQ(42, notNested.bar);
}

template <typename Cpp2Writer, typename Cpp2Reader>
void runDoubleTest() {
  cpp2::Doubles dbls;
  dbls.inf = HUGE_VAL;
  dbls.neginf = -HUGE_VAL;
  dbls.nan = NAN;
  dbls.repeating = 9.0 / 11.0;
  dbls.big = std::numeric_limits<double>::max();
  dbls.small = std::numeric_limits<double>::epsilon();
  dbls.zero = 0.0;
  dbls.negzero = -0.0;

  Cpp2Writer prot;
  Cpp2Reader protReader;
  size_t bufSize = dbls.serializedSize(&prot);
  IOBufQueue queue(IOBufQueue::cacheChainLength());
  prot.setOutput(&queue, bufSize);
  dbls.write(&prot);

  bufSize = queue.chainLength();
  auto buf = queue.move();
  protReader.setInput(buf.get());
  cpp2::Doubles dbls2;
  dbls2.read(&protReader);

  ASSERT_EQ(std::numeric_limits<double>::infinity(), dbls2.inf);
  ASSERT_EQ(-std::numeric_limits<double>::infinity(), dbls2.neginf);
  ASSERT_TRUE(std::isnan(dbls2.nan));
  ASSERT_EQ(9.0 / 11.0, dbls2.repeating);
  ASSERT_EQ(std::numeric_limits<double>::max(), dbls2.big);
  ASSERT_EQ(std::numeric_limits<double>::epsilon(), dbls2.small);
  ASSERT_EQ(0.0, dbls2.zero);
  ASSERT_EQ(-0.0, dbls2.zero);
}

TEST(protocol2, binary) {
  runTest<BinaryProtocolWriter, BinaryProtocolReader, TBinaryProtocol,
          OneOfEach>(ooe);
  runTest<BinaryProtocolWriter, BinaryProtocolReader, TBinaryProtocol,
          Nested>(nested);
}

TEST(protocol2, binarySkipping) {
  runSkipTest<BinaryProtocolWriter, BinaryProtocolReader>();
}

TEST(protocol2, binaryDoubles) {
  runDoubleTest<BinaryProtocolWriter, BinaryProtocolReader>();
}

TEST(protocol2, compact) {
  runTest<CompactProtocolWriter, CompactProtocolReader, TCompactProtocol,
          OneOfEach>(ooe);
  runTest<CompactProtocolWriter, CompactProtocolReader, TCompactProtocol,
          Nested>(nested);
}

TEST(protocol2, compactSkipping) {
  runSkipTest<CompactProtocolWriter, CompactProtocolReader>();
}

TEST(protocol2, compactDoubles) {
  runDoubleTest<CompactProtocolWriter, CompactProtocolReader>();
}

TEST(protocol2, simpleJson) {
  runTest<SimpleJSONProtocolWriter,
          SimpleJSONProtocolReader, TSimpleJSONProtocol, OneOfEach>(ooe);
  runTest<SimpleJSONProtocolWriter,
          SimpleJSONProtocolReader, TSimpleJSONProtocol, Nested>(nested);
}

TEST(protocol2, simpleJsonSkipping) {
  runSkipTest<SimpleJSONProtocolWriter, SimpleJSONProtocolReader>();
}

TEST(protocol2, simpleJsonDoubles) {
  runDoubleTest<SimpleJSONProtocolWriter, SimpleJSONProtocolReader>();
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  ooe.im_true   = true;
  ooe.im_false  = false;
  ooe.a_bite    = 0xd6;
  ooe.integer16 = 27000;
  ooe.integer32 = 1 << 24;
  ooe.integer64 = (uint64_t)6000 * 1000 * 1000;
  ooe.double_precision = M_PI;
  ooe.float_precision  = (float)12.345;
  ooe.some_characters  = "JSON THIS! \"\1";
  ooe.zomg_unicode     = "\xd7\n\a\t";
  ooe.base64 = "\1\2\3\255";
  ooe.string_string_map["one"] = "two";
  ooe.string_string_hash_map["three"] = "four";
  ooe.rank_map[567419810] = (float)0.211184;
  ooe.rank_map[507959914] = (float)0.080382;

  nested.foo = nested_foo;
  nested.bar = 42;

  return RUN_ALL_TESTS();
}
