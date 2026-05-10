#include <gtest/gtest.h>
#include <llob/concepts.hpp>

struct alignas(64) GoodSlot { char data[64]; };
struct alignas(64) TooBig   { char data[65]; };
struct BadAlignment          { char data[32]; };
struct NonPolymorphic        { void foo() {} };
struct Polymorphic           { virtual void foo() {} };
struct POD                   { int x; float y; };
class  TypeWithDestructor    { public: ~TypeWithDestructor() {} };

TEST(FitsCacheLine, PassesForCorrectType)    { EXPECT_TRUE(llob::FitsCacheLine<GoodSlot>); }
TEST(FitsCacheLine, FailsForOversizedType)   { EXPECT_FALSE(llob::FitsCacheLine<TooBig>); }
TEST(FitsCacheLine, FailsForWrongAlignment)  { EXPECT_FALSE(llob::FitsCacheLine<BadAlignment>); }

TEST(IsNotVirtual, PassesForNonPolymorphicType) { EXPECT_TRUE(llob::IsNotVirtual<NonPolymorphic>); }
TEST(IsNotVirtual, FailsForPolymorphicType)     { EXPECT_FALSE(llob::IsNotVirtual<Polymorphic>); }

TEST(IsPowerOfTwo, PassesForPowersOfTwo) {
    EXPECT_TRUE(llob::IsPowerOfTwo<1>);
    EXPECT_TRUE(llob::IsPowerOfTwo<2>);
    EXPECT_TRUE(llob::IsPowerOfTwo<4>);
    EXPECT_TRUE(llob::IsPowerOfTwo<64>);
}
TEST(IsPowerOfTwo, FailsForNonPowersOfTwo) {
    EXPECT_FALSE(llob::IsPowerOfTwo<0>);
    EXPECT_FALSE(llob::IsPowerOfTwo<3>);
    EXPECT_FALSE(llob::IsPowerOfTwo<5>);
    EXPECT_FALSE(llob::IsPowerOfTwo<7>);
}

TEST(IsTriviallyCopyable, PassesForPOD)              { EXPECT_TRUE(llob::IsTriviallyCopyable<POD>); }
TEST(IsTriviallyCopyable, FailsForTypeWithDestructor) { EXPECT_FALSE(llob::IsTriviallyCopyable<TypeWithDestructor>); }
