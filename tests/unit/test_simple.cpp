#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <gtest/gtest.h>

// The fixture for testing class Foo.
class SimpleTest : public ::testing::Test
{
 protected:

    SimpleTest()
    {
    }

    virtual ~SimpleTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }

};

// Tests that a simple mustache tag is replaced
TEST_F(SimpleTest, TestBasicAssert)
{
  EXPECT_EQ(1, 1);
}
