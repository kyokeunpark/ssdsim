#include <iostream>
#include "gtest/gtest.h"
#include "samplers.h"

TEST(SamplerTest, SanityCheckSampler1Value) {
	SanityCheckSampler1 sampler = SanityCheckSampler1(5, 5);
	tuple t = sampler.get_size_age_sample(1);
	sizes s = t.first;
	lives l = t.second;

	EXPECT_EQ(s.front(), 5);
	EXPECT_EQ(l.front(), 6.0);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}