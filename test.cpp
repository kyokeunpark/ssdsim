#include "configs.h"
#include "extent_manager.h"
#include "extent_stack.h"
#include "stripe_manager.h"
#include "stripers.h"
#include "gtest/gtest.h"
#include <iostream>
#include <memory>

/****************************************
 * Sampler
 ****************************************/
TEST(SamplerTest, SanityCheckSampler1Value) {
  SanityCheckSampler1 sampler = SanityCheckSampler1(5, 5);
  sample_pair t = sampler.get_size_age_sample(1);
  sizes s = t.first;
  lives l = t.second;

  EXPECT_EQ(s.front(), 5);
  EXPECT_EQ(l.front(), 6.0);
}

/****************************************
 * ObjectManager
 ****************************************/
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

/****************************************
 * StripeManager
 ****************************************/
TEST(StripeManagerTest, StripeManagerInit) {
  StripeManager s_m = StripeManager(7, 2, 2, 2);
  EXPECT_EQ(s_m.num_data_exts_per_locality, 7);
  EXPECT_EQ(s_m.num_local_parities, 2);
  EXPECT_EQ(s_m.num_global_parities, 2);
  EXPECT_EQ(s_m.num_localities_in_stripe, 2);
  EXPECT_EQ(s_m.num_exts_per_stripe, 18);
  EXPECT_EQ(s_m.num_data_exts_per_stripe, 14);
}

TEST(StripeManagerTest, GetDataDcSizeANDGetTotalDataDcSize) {
  StripeManager s_m = StripeManager(7, 2, 2, 2, 0.0);
  s_m.create_new_stripe(5);
  s_m.create_new_stripe(10);
  EXPECT_NEAR(s_m.get_data_dc_size(), 210, 0.1);
  EXPECT_NEAR(s_m.get_total_dc_size(), 270, 0.1);
};

TEST(StripeManagerTest, GetDataDcSizeANDGetTotalDataDcSizeFLOATS) {
  StripeManager s_m = StripeManager(1, 2.0 / 14, 2.0 / 14, 1, 18.0 / 14);
  s_m.create_new_stripe(5);
  s_m.create_new_stripe(10);
  EXPECT_NEAR(s_m.get_data_dc_size(), 15, 0.1);
  EXPECT_NEAR(s_m.get_total_dc_size(), 19.28571428571429, 0.1);
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

/****************************************
 * ExtentManager
 ****************************************/
TEST(ExtentManagerTest, CreateNewExtent) {
  ExtentManager e_m = ExtentManager(100, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(15, 2);
  Extent *e3 = e_m.create_extent();
  EXPECT_EQ(e_m.get_num_ext(), 3);
  EXPECT_EQ(e1->ext_size, 5);
  EXPECT_EQ(e2->ext_size, 15);
  EXPECT_EQ(e2->secondary_threshold, 2);
  EXPECT_EQ(e3->ext_size, 100);
};

TEST(ExtentManagerTest, DeleteExtent) {
  ExtentManager e_m = ExtentManager(100, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(15);
  Extent *e3 = e_m.create_extent();
  e_m.delete_extent(e3);
  EXPECT_EQ(e_m.get_num_ext(), 2);
  EXPECT_EQ(e1->ext_size, 5);
  EXPECT_EQ(e2->ext_size, 15);
  EXPECT_EQ(std::find(e_m.exts.begin(), e_m.exts.end(), e3), e_m.exts.end());
};

TEST(ExtentManagerTest, GetExtTypes) {
  ExtentManager e_m = ExtentManager(100, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(15, 2);
  Extent *e3 = e_m.create_extent();
  EXPECT_EQ(e_m.get_ext_types().empty(), true);
  e3->type = "small";
  auto types = e_m.get_ext_types();
  EXPECT_EQ(types.size(), 1);
  EXPECT_EQ(types["small"], 1);
  e2->type = "small";
  types = e_m.get_ext_types();
  EXPECT_EQ(types.size(), 1);
  EXPECT_EQ(types["small"], 2);
  e1->type = "large";
  types = e_m.get_ext_types();
  EXPECT_EQ(types.size(), 2);
  EXPECT_EQ(types["small"], 2);
  EXPECT_EQ(types["large"], 1);
};

/****************************************
 * ExtentStack
 ****************************************/
TEST(ExtentStack, AddExtentGetLengthOfExtentStack) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  EXPECT_EQ(e_s->get_length_of_extent_stack(), 3);

};

TEST(ExtentStack, AddExtentGetLengthAtKey) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  EXPECT_EQ(e_s->get_length_at_key(2), 2);

};

TEST(ExtentStack, GetExtentAtKey) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
    Extent *e4 = e_m.create_extent(12);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  EXPECT_EQ(e_s->get_extent_at_key(1), e1);
  EXPECT_EQ(e_s->get_extent_at_key(2), e2);
  EXPECT_EQ(e_s->get_extent_at_key(2), e3);
  EXPECT_EQ(e_s->get_extent_at_key(2), nullptr);
  EXPECT_EQ(e_s->get_length_of_extent_stack(), 0);
  EXPECT_EQ(e_s->get_length_at_key(2), 0);
};

TEST(ExtentStack, ContainsExtent) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
    Extent *e4 = e_m.create_extent(12);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  EXPECT_EQ(e_s->contains_extent(e1), true);
  EXPECT_EQ(e_s->contains_extent(e4), false);
  EXPECT_EQ(e_s->contains_extent(nullptr), false);
};

TEST(ExtentStack, RemoveExtent) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  EXPECT_EQ(e_s->contains_extent(e1), true);
  e_s->remove_extent(e1);
  EXPECT_EQ(e_s->contains_extent(e1), false);
  EXPECT_EQ(e_s->contains_extent(e2), true);
  EXPECT_EQ(e_s->contains_extent(e3), true);
  EXPECT_EQ(e_s->get_length_of_extent_stack(), 2);
  EXPECT_EQ(e_s->get_length_at_key(1), 0);
  EXPECT_EQ(e_s->get_length_at_key(2), 2);
};

TEST(ExtentStack, SingleExtentStackNumStripes) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  EXPECT_EQ(e_s->num_stripes(2),1);
  EXPECT_EQ(e_s->num_stripes(1),3);
};
TEST(ExtentStack, SingleExtentStackPopStripeNumExts) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
    Extent *e4 = e_m.create_extent(12);
  e_s->add_extent(1, e1);
  e_s->add_extent(2, e2);
  e_s->add_extent(2, e3);
  e_s->add_extent(3, e4);
  auto ret = e_s->pop_stripe_num_exts(2);
  EXPECT_EQ(ret.size(), 2);
  EXPECT_EQ(ret.front(), e4);
  EXPECT_EQ(ret.back(), e2);
  ret = e_s->pop_stripe_num_exts(2);
  EXPECT_EQ(ret.size(), 2);
  EXPECT_EQ(ret.front(), e3);
  EXPECT_EQ(ret.back(), e1);
  ret = e_s->pop_stripe_num_exts(2);
  EXPECT_EQ(ret.size(),0);
};

TEST(ExtentStack, MultiExtentStackNumStripes) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<MultiExtentStack> e_s = make_shared<MultiExtentStack>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
    Extent *e4 = e_m.create_extent(12);
  e_s->add_extent(1,e1);
  e_s->add_extent(2,e2);
  e_s->add_extent(2,e3);
  e_s->add_extent(3,e4);
  EXPECT_EQ(e_s->num_stripes(2),1);
  EXPECT_EQ(e_s->num_stripes(1),4);
};


TEST(ExtentStack, MultiExtentStackPopStripeNumExts) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<MultiExtentStack> e_s = make_shared<MultiExtentStack>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
    Extent *e4 = e_m.create_extent(12);
  e_s->add_extent(1, e1);
  e_s->add_extent(2, e2);
  e_s->add_extent(2, e3);
  e_s->add_extent(3, e4);
  auto ret = e_s->pop_stripe_num_exts(2);
  EXPECT_EQ(ret.size(), 2);
  EXPECT_EQ(ret.front(), e2);
  EXPECT_EQ(ret.back(), e3);
  ret = e_s->pop_stripe_num_exts(2);
  EXPECT_EQ(ret.size(),0);
};

TEST(ExtentStack, BestEffortExtentStackGetExtAtClosestKey) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
    std::shared_ptr<AbstractExtentStack> e_s = make_shared<BestEffortExtentStack>(s_m);
    ExtentManager e_m = ExtentManager(ext_size, nullptr);
    Extent *e1 = e_m.create_extent(5);
    Extent *e2 = e_m.create_extent(10);
    Extent *e3 = e_m.create_extent(15);
    Extent *e4 = e_m.create_extent(12);
    Extent *e5 = e_m.create_extent();
    Extent *e6 = e_m.create_extent();
  e_s->add_extent(1, e1);
  e_s->add_extent(1, e2);
  e_s->add_extent(2, e3);
  e_s->add_extent(2, e4);
  e_s->add_extent(3, e5);
  e_s->add_extent(3, e6);
  auto ret = e_s->get_extent_at_closest_key(2);
  EXPECT_EQ(ret, e3);
  ret = e_s->get_extent_at_closest_key(4);
  EXPECT_EQ(ret, e5);
  e_s->remove_extent(e1);
  e_s->remove_extent(e2);
  e_s->remove_extent(e3);
  e_s->remove_extent(e4);
  e_s->remove_extent(e5);
  e_s->remove_extent(e6);
  e_s->add_extent(1, e1);
  e_s->add_extent(2, e2);
  e_s->add_extent(7, e3);
  e_s->add_extent(8, e4);
  e_s->add_extent(10, e5);
  e_s->add_extent(11, e6);
  ret = e_s->get_extent_at_closest_key(2);
  EXPECT_EQ(ret, e2);
  ret = e_s->get_extent_at_closest_key(3);
  EXPECT_EQ(ret, e1);
  ret = e_s->get_extent_at_closest_key(6);
  EXPECT_EQ(ret, e3);
  ret = e_s->get_extent_at_closest_key(9);
  EXPECT_EQ(ret, e4);
};

TEST(ExtentStack, ExtentStackRandomizerPopStripeNumExts) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  std::shared_ptr<SingleExtentStack<>> e_s = make_shared<SingleExtentStack<>>(s_m);
  std::shared_ptr<AbstractExtentStack> randomizer = make_shared<ExtentStackRandomizer>(e_s);
  ExtentManager e_m = ExtentManager(ext_size, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(10);
  Extent *e3 = e_m.create_extent(15);
  Extent *e4 = e_m.create_extent(12);
  Extent *e5 = e_m.create_extent();
  Extent *e6 = e_m.create_extent();
  e_s->add_extent(1, e1);
  e_s->add_extent(2, e2);
  e_s->add_extent(2, e3);
  e_s->add_extent(2, e5);
  e_s->add_extent(2, e6);
  e_s->add_extent(3, e4);
  std::shared_ptr<StripeManager> s_m_nonrandomized  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  std::shared_ptr<SingleExtentStack<>> e_s_nonrandomized = make_shared<SingleExtentStack<>>(s_m);
  e_s_nonrandomized->add_extent(1, e1);
  e_s_nonrandomized->add_extent(2, e2);
  e_s_nonrandomized->add_extent(2, e3);
  e_s_nonrandomized->add_extent(2, e5);
  e_s_nonrandomized->add_extent(2, e6);
  e_s_nonrandomized->add_extent(3, e4);
  srand(1);
  auto nonshuffled_res = e_s_nonrandomized->pop_stripe_num_exts(2);
  auto shuffled_res = randomizer->pop_stripe_num_exts(2);
  Extent * shuffled_e1 = shuffled_res.front();
  Extent * nonshuffled_e1 =  nonshuffled_res.front();
  Extent * shuffled_e2 = shuffled_res.back();
  Extent * nonshuffled_e2 =  nonshuffled_res.back();
  EXPECT_EQ(nonshuffled_e1, e4);
  EXPECT_EQ(shuffled_e1, nonshuffled_e1);
  EXPECT_NE(shuffled_e2, nonshuffled_e2);
  EXPECT_EQ(randomizer->contains_extent(shuffled_e1), false);
  EXPECT_EQ(randomizer->contains_extent(shuffled_e2), false);
};

TEST(ExtentStack, WholeObjectExtentStackNumStripes) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  std::shared_ptr<WholeObjectExtentStack> e_s = make_shared<WholeObjectExtentStack>(s_m);
  ExtentManager e_m = ExtentManager(ext_size, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(10);
  Extent *e3 = e_m.create_extent(15);
  Extent *e4 = e_m.create_extent(12);
  Extent *e5 = e_m.create_extent();
  Extent *e6 = e_m.create_extent();
  stack_val v1 = vector<Extent *>();
  v1.push_back(e1);
  v1.push_back(e2);
  stack_val v2 = vector<Extent *>();
  v1.push_back(e4);
  stack_val v3 = vector<Extent *>();
  v3.push_back(e5);
  v3.push_back(e6);
  
  e_s->add_extent(v2);
  e_s->add_extent(v1);
  e_s->add_extent(v3);
  EXPECT_EQ(e_s->num_stripes(2), 2);
  EXPECT_EQ(e_s->num_stripes(3), 1);

};

TEST(ExtentStack, WholeObjectExtentStackPopNumStripes1) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  std::shared_ptr<WholeObjectExtentStack> e_s = make_shared<WholeObjectExtentStack>(s_m);
  ExtentManager e_m = ExtentManager(ext_size, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(10);
  Extent *e3 = e_m.create_extent(15);
  Extent *e4 = e_m.create_extent(12);
  Extent *e5 = e_m.create_extent();
  Extent *e6 = e_m.create_extent();
  Extent *e7 = e_m.create_extent();
  stack_val v1 = vector<Extent *>();
  v1.push_back(e1);
  v1.push_back(e2);
  stack_val v2 = vector<Extent *>();
  v2.push_back(e4);
  stack_val v3 = vector<Extent *>();
  v3.push_back(e5);
  v3.push_back(e6);
  e_s->add_extent(v1);
  e_s->add_extent(v2);
  e_s->add_extent(v3);
  auto ret = e_s->pop_stripe_num_exts(4);
  EXPECT_EQ(ret.size(), 4);
  auto ret_it = ret.begin();
  EXPECT_EQ(*ret_it, e1);
  ret_it++;
  EXPECT_EQ(*ret_it, e2);
  ret_it++;
  EXPECT_EQ(*ret_it, e5);
  ret_it++;
  EXPECT_EQ(*ret_it, e6);


  e_s->add_extent(v1);
  ret = e_s->pop_stripe_num_exts(3);
  ret_it = ret.begin();
  EXPECT_EQ(ret.size(), 3);
  EXPECT_EQ(*ret_it, e1);
  ret_it++;
  EXPECT_EQ(*ret_it, e2);
  ret_it++;
  EXPECT_EQ(*ret_it, e4);
  ret = e_s->pop_stripe_num_exts(4);
  EXPECT_EQ(ret.size(), 0);
};

TEST(ExtentStack, WholeObjectExtentStackGetExtentAtKey) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  std::shared_ptr<WholeObjectExtentStack> e_s = make_shared<WholeObjectExtentStack>(s_m);
  ExtentManager e_m = ExtentManager(ext_size, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(10);
  Extent *e3 = e_m.create_extent(15);
  Extent *e4 = e_m.create_extent(12);
  Extent *e5 = e_m.create_extent();
  Extent *e6 = e_m.create_extent();
  Extent *e7 = e_m.create_extent();
  stack_val v1 = vector<Extent *>();
  v1.push_back(e1);
  v1.push_back(e2);
  stack_val v2 = vector<Extent *>();
  v2.push_back(e4);
  stack_val v3 = vector<Extent *>();
  v3.push_back(e5);
  v3.push_back(e6);
  e_s->add_extent(v1);
  e_s->add_extent(v2);
  e_s->add_extent(v3);
  auto ret = e_s->get_extent_at_key(-1);
  EXPECT_EQ(ret, e4);
  ret = e_s->get_extent_at_key(-1);
  EXPECT_EQ(ret, e1);
  ret = e_s->get_extent_at_key(-1);
  EXPECT_EQ(ret, e5);
  ret = e_s->get_extent_at_key(-1);
  EXPECT_EQ(ret, nullptr);
};
  
TEST(ExtentStack, WholeObjectExtentStackContainsRemoveExtent) {
  int ext_size = 3*1024;
  std::shared_ptr<StripeManager> s_m  = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  std::shared_ptr<WholeObjectExtentStack> e_s = make_shared<WholeObjectExtentStack>(s_m);
  ExtentManager e_m = ExtentManager(ext_size, nullptr);
  Extent *e1 = e_m.create_extent(5);
  Extent *e2 = e_m.create_extent(10);
  Extent *e3 = e_m.create_extent(15);
  Extent *e4 = e_m.create_extent(12);
  Extent *e5 = e_m.create_extent();
  Extent *e6 = e_m.create_extent();
  Extent *e7 = e_m.create_extent();
  stack_val v1 = vector<Extent *>();
  v1.push_back(e1);
  v1.push_back(e2);
  stack_val v2 = vector<Extent *>();
  v2.push_back(e4);
  stack_val v3 = vector<Extent *>();
  v3.push_back(e5);
  v3.push_back(e6);
  e_s->add_extent(v1);
  e_s->add_extent(v2);
  e_s->add_extent(v3);
  EXPECT_EQ(e_s->contains_extent(e3), false);
  EXPECT_EQ(e_s->contains_extent(e4), true);
  e_s->remove_extent(e4);
  EXPECT_EQ(e_s->contains_extent(e4), false);
  e_s->remove_extent(e1);
  EXPECT_EQ(e_s->contains_extent(e1), false);
  e_s->remove_extent(e2);
  EXPECT_EQ(e_s->contains_extent(e2), false);
  e_s->remove_extent(e5);
  EXPECT_EQ(e_s->contains_extent(e5), false);
  e_s->remove_extent(e6);
  EXPECT_EQ(e_s->contains_extent(e6), false);
  auto ret = e_s->pop_stripe_num_exts(3);
  EXPECT_EQ(ret.size(), 0);
};

/****************************************
 * Striper
 ****************************************/
TEST(StriperTest, SimpleStriperCreateStripeWithSingleExtentStack) {
  int ext_size = 3*1024;
  auto s_m = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  auto e_m = make_shared<ExtentManager>(ext_size, nullptr);
  auto e_s = make_shared<SingleExtentStack<>>(s_m);
  std::shared_ptr<SimpleStriper> striper = make_shared<SimpleStriper>(s_m, e_m);
  for (int i = 0; i < 60; i++) {
    auto e = e_m->create_extent(i);
    e->timestamp = 123;
    e_s->add_extent(i, e);
  }
  auto costs = striper->create_stripes(e_s, 365.0);
  EXPECT_EQ(costs.stripes, 1);
  EXPECT_EQ(costs.reads, 735);
  EXPECT_EQ(costs.writes, 735);
}

TEST(StriperTest, SimpleStriperCreateStripeWithMultiExtentStack) {
  int ext_size = 3*1024;
  auto s_m = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  auto e_m = make_shared<ExtentManager>(ext_size, nullptr);
  auto e_s = make_shared<MultiExtentStack>(s_m);
  auto striper = make_shared<SimpleStriper>(s_m, e_m);
  for (int i = 0; i < 4; i++) {
    for (int j = 1; j < 60; j++) {
      auto e = e_m->create_extent(j);
      e->timestamp = 123;
      e_s->add_extent(i, e);
    }
  }
  auto costs = striper->create_stripes(e_s, 365.0);
  EXPECT_EQ(costs.stripes, 1);
  EXPECT_EQ(costs.reads, 105);
  EXPECT_EQ(costs.writes, 105);
}

TEST(StriperTest, SimpleStriperCreateStripeWithMultiExtentStack2) {
  int ext_size = 3*1024;
  stripe_costs total = {0};
  auto s_m = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  auto e_m = make_shared<ExtentManager>(ext_size, nullptr);
  auto e_s = make_shared<MultiExtentStack>(s_m);
  auto striper = make_shared<SimpleStriper>(s_m, e_m);
  for (int i = 0; i < 4; i++) {
    for (int j = 1; j < 60; j++) {
      auto e = e_m->create_extent(j);
      e->timestamp = 123;
      e_s->add_extent(i, e);
    }
  }
  for (int i = 0; i < 3; i++) {
    auto costs = striper->create_stripes(e_s, 365.0);
    total += costs;
  }
  EXPECT_EQ(total.stripes, 3);
  EXPECT_EQ(total.reads, 903);
  EXPECT_EQ(total.writes, 903);
}

TEST(StriperTest, ExtentStackStriperWithSimpleExtentStack) {
  int ext_size = 3*1024;
  stripe_costs total = {0};
  auto s_m = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  auto e_m = make_shared<ExtentManager>(ext_size, nullptr);
  auto e_s = make_shared<SingleExtentStack<>>(s_m);
  auto striper = make_shared<ExtentStackStriper>(make_shared<SimpleStriper>(s_m, e_m));
  for (int i = 0; i < 4; i++) {
    for (int j = 1; j < 60; j++) {
      auto e = e_m->create_extent(j);
      e->timestamp = 123;
      e_s->add_extent(i, e);
    }
  }
  for (int i = 0; i < 3; i++) {
    auto costs = striper->create_stripes(e_s, 365.0);
    total += costs;
  }
  EXPECT_EQ(total.stripes, 16);
  EXPECT_EQ(total.reads, 6438);
  EXPECT_EQ(total.writes, 6438);
}

TEST(StriperTest, NumStripesStriperWithSimpleExtentStack) {
  int ext_size = 3*1024;
  str_costs total = {0};
  auto s_m = make_shared<StripeManager>(7, 2, 2, 2, 0.0);
  auto e_m = make_shared<ExtentManager>(ext_size, nullptr);
  auto e_s = make_shared<MultiExtentStack>(s_m);
  auto striper = make_shared<NumStripesStriper>(2, make_shared<SimpleStriper>(s_m, e_m));
  for (int i = 0; i < 4; i++) {
    for (int j = 1; j < 60; j++) {
      auto e = e_m->create_extent(j);
      e->timestamp = 123;
      e_s->add_extent(i, e);
    }
  }
  for (int i = 0; i < 3; i++) {
    auto costs = striper->create_stripes(e_s, 365.0);
    total += costs;
  }
  EXPECT_EQ(total.stripes, 6);
  EXPECT_EQ(total.reads, 2002);
  EXPECT_EQ(total.writes, 2002);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
