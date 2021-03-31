#include "configs.h"
#include "stripe_manager.h"
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
};

TEST(ObjectManagerTest, GetSingleObjectById) {
    ObjectManager o_m =
        ObjectManager(make_shared<EventManager>(),
                      make_shared<DeterministicDistributionSampler>(365));
    object_lst objs = o_m.create_new_object(1);
    EXPECT_EQ(objs.size(), 1);
    EXPECT_EQ(o_m.get_object(0), objs[0].first);
};
TEST(ObjectManagerTest, GetMultipleObjectsById) {
    ObjectManager o_m =
        ObjectManager(make_shared<EventManager>(),
                      make_shared<DeterministicDistributionSampler>(365));
    object_lst objs = o_m.create_new_object(3);
    EXPECT_EQ(objs.size(), 3);
    EXPECT_EQ(o_m.get_object(0), objs[0].first);
    EXPECT_EQ(o_m.get_object(1), objs[1].first);
    EXPECT_EQ(o_m.get_object(2), objs[2].first);
};

TEST(ObjectManagerTest, DeleteObject) {
    ObjectManager o_m =
        ObjectManager(make_shared<EventManager>(),
                      make_shared<DeterministicDistributionSampler>(365));
    object_lst objs = o_m.create_new_object(3);
    EXPECT_EQ(objs.size(), 3);
    EXPECT_EQ(o_m.get_object(0), objs[0].first);
    o_m.remove_object(o_m.get_object(0));
    EXPECT_EQ(o_m.get_object(0), nullptr);
};

TEST(StripeManagerTest, GetDataDcSizeANDGetTotalDataDcSize) {
    StripeManager s_m = StripeManager(7, 2, 2, 2, 0.0);
    s_m.create_new_stripe(5);
    s_m.create_new_stripe(10);
    EXPECT_NEAR(s_m.get_data_dc_size(), 210, 3);
    EXPECT_NEAR(s_m.get_total_dc_size(), 270, 3);
};

TEST(StripeManagerTest, GetDataDcSizeANDGetTotalDataDcSizeFLOATS) {
    StripeManager s_m = StripeManager(1, 2.0 / 14, 2.0 / 14, 1, 18.0 / 14);
    s_m.create_new_stripe(5);
    s_m.create_new_stripe(10);
    EXPECT_NEAR(s_m.get_data_dc_size(), 15, 3);
    EXPECT_NEAR(s_m.get_total_dc_size(), 19.28571428571429, 3);
};

TEST(StripeManagerTest, DeleteStripe) {
    StripeManager s_m = StripeManager(1, 2.0 / 14, 2.0 / 14, 1, 18.0 / 14);
    s_m.create_new_stripe(5);
    s_m.create_new_stripe(10);
    Stripe *s = s_m.stripes->front();
    s_m.delete_stripe(s);
    EXPECT_EQ(find(s_m.stripes->begin(), s_m.stripes->end(), s),
              s_m.stripes->end());
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
