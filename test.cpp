#include <iostream>
#include "gtest/gtest.h"
#include "samplers.h"

TEST(SamplerTest, SanityCheckSampler1Value) {
	SanityCheckSampler1 sampler = SanityCheckSampler1(5, 5);
<<<<<<< HEAD
	sl_tuple t = sampler.get_size_age_sample(1);
=======
	sample_pair t = sampler.get_size_age_sample(1);
>>>>>>> 7f6401ec5b7c9cc930845fb8eb9b5b5fe1528bc6
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