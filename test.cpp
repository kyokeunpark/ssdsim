#include "configs.h"
#include "gtest/gtest.h"
#include <iostream>

TEST(SamplerTest, SanityCheckSampler1Value) {
    SanityCheckSampler1 sampler = SanityCheckSampler1(5, 5);
    sample_pair t = sampler.get_size_age_sample(1);
    sizes s = t.first;
    lives l = t.second;

    EXPECT_EQ(s.front(), 5);
    EXPECT_EQ(l.front(), 6.0);
}

TEST(ObjectManagerTest, CreateOneNewObject) {
    ObjectManager o_m =
        ObjectManager(make_shared<EventManager>(),
                      make_shared<DeterministicDistributionSampler>(365));
    object_lst objs = o_m.create_new_object(1);
    EXPECT_EQ(objs.size(), 1);
    // cout << objs[0].second;
};

TEST(ObjectManagerTest, CreateThreeNewObject) {
    ObjectManager o_m =
        ObjectManager(make_shared<EventManager>(),
                      make_shared<DeterministicDistributionSampler>(365));
    object_lst objs = o_m.create_new_object(3);
    EXPECT_EQ(objs.size(), 3);
    for (auto &kv : objs) {
        cout << kv.second << endl;
    }
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
