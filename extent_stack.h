#include "stripe_manager.h"
#include <map>
#include <list>
template <class extent_stack_value_type, class extent_stack_key_type>
class ExtentStack{
    public:
        StripeManager * stripe_manager;
        //ordered by key
        map<extent_stack_key_type, list<extent_stack_value_type>*> * extent_stack;
        ExtentStack(StripeManager * s_m):stripe_manager(s_m), 
        extent_stack(new map<extent_stack_key_type, list<extent_stack_value_type>*>())
        {}
        virtual int num_stripes() = 0;
        virtual list<Extent *> pop_stripe_num_exts(int stripe_size) = 0;
        void add_extent(extent_stack_key_type key, extent_stack_value_type ext)
        {
            extent_stack->emplace(key, new list<Extent>());
            (*extent_stack)[key]->push_back(ext);
        }

        virtual int get_length_of_extent_stack()
        {
            int length = 0;
            for(auto& kv : *extent_stack){
                length+= kv.second->size();
            }
            return length;
        }

        virtual int get_length_at_key(extent_stack_key_type key)
        {
            auto it = extent_stack->find(key);
            return it == extent_stack->end()?0:it->second->size();
        }

        /* pop at index 0 not sure if this is still a stack..
        def get_extent_at_key(self, key):
        """Returns an extent at the given key.
        """
        ext = self.extent_stack[key].pop(0)
        if (len(self.extent_stack.get(key)) == 0):
            del self.extent_stack[key]
        return ext 
        */
        virtual Extent* get_extent_at_key(extent_stack_key_type key)
        {
            if(extent_stack->find(key) == extent_stack->end())
            {
                return NULL;
            }
            list<extent_stack_value_type> * exts = extent_stack->find(key)->second;
            extent_stack_value_type ret = exts->front();
            exts->pop_front();
            if(exts->size() == 0)
            {
                delete exts;
                extent_stack->erase(key);
            }
            return ret;
        }

        virtual bool contains_extent(Extent * extent)
        {
            for(auto& kv : *extent_stack){
                if(find(kv.second->begin(), kv.second->end(), extent) != kv.second->end())
                {
                    return true;
                }
            }
            return false;
        }

        //can end early
        virtual void remove_extent(Extent * extent)
        {
            for(auto& kv : *extent_stack){
                kv.second->remove(extent);
                if(kv.second->size() == 0)
                {
                    delete kv.second;
                    extent_stack->erase(kv.first);
                }
            }
        }
};

class SingleExtentStack: public ExtentStack<Extent *, int>
{
    using ExtentStack::ExtentStack;
    int num_stripes(int stripe_size)
    {
        return get_length_of_extent_stack()/stripe_size;
    }

    list<Extent *> pop_stripe_num_exts(int stripe_size)
    {
        list<Extent *> ret;
        int num_left_to_add = stripe_size;
        if(get_length_of_extent_stack() < num_left_to_add)
        {
            return ret;
        }
        for(auto& kv : *extent_stack)
        {
            list<Extent*> * ext_lst = kv.second;
            for(int i = 0; 
                i < (kv.second->size() > num_left_to_add?num_left_to_add:kv.second->size());
                i++)
            {
                ret.push_back(ext_lst->front());
                ext_lst->pop_front();
            }
            if(ext_lst->size() == 0)
            {
                delete kv.second;
                extent_stack->erase(kv.first);
            }
            num_left_to_add = stripe_size - ret.size();
        }
        return ret;
    }
};

class MultiExtentStack:public ExtentStack<Extent *, int>
{
    using ExtentStack::ExtentStack;
    int num_stripes(int stripe_size)
    {
        int num_stripes = 0;
        for(auto& kv: *extent_stack)
        {
            num_stripes += kv.second->size()/stripe_size;
        }
        return num_stripes;
    }

    list<Extent *> pop_stripe_num_exts(int stripe_size)
    {
        list<Extent *> ret;
        for(auto& kv : *extent_stack)
        {
            list<Extent*> * ext_lst = kv.second;
            if(ext_lst->size() >= stripe_size)
            {
                for(int i = 0; i < stripe_size; i++)
                {
                    ret.push_back(ext_lst->front());
                    ext_lst->pop_front();
                }
                if(ext_lst->size() == 0)
                {
                    delete kv.second;
                    extent_stack->erase(kv.first);
                }
                return ret;
            }
        }
        return ret;
    }
};

class BestEffortExtentStack:public SingleExtentStack
{
    using SingleExtentStack::SingleExtentStack;
    //double check correctness
    Extent * get_extent_at_closest_key(int key)
    {
        if(extent_stack->size() == 1)
        {
            return get_extent_at_key(extent_stack->begin()->first);
        }
        if(key < extent_stack->begin()->first)
        {
            return get_extent_at_key(extent_stack->begin()->first);
        }
        if(key > prev(extent_stack->end())->first)
        {
            return get_extent_at_key(prev(extent_stack->end())->first);
        }
        auto next_pos= extent_stack->lower_bound(key);
        if(next_pos == extent_stack->end())
        {
            next_pos = prev(extent_stack->end());
        }
        auto prev_pos = next_pos == extent_stack->begin()?extent_stack->begin(): prev(next_pos);
        int next_key = next_pos->first;
        int prev_key = prev_pos->first;
        if(next_key - key < key - prev_key)
        {
            return get_extent_at_key(next_key);
        }
        return get_extent_at_key(prev_key);
    }
};


typedef list<Extent *>* extent_stack_ext_lst;
class WholeObjectExtentStack:ExtentStack<extent_stack_ext_lst, int>
{
    using ExtentStack::ExtentStack;
    int num_stripes(int stripe_size)
    {
        return get_length_of_extent_stack()/stripe_size;
    }

    void add_extent(extent_stack_ext_lst ext_lst)
    {
        int key = ext_lst->size();
        extent_stack->emplace(key, new list<extent_stack_ext_lst>());
        extent_stack->find(key)->second->push_back(ext_lst);
    }
    int get_length_of_extent_stack() override{
        int length = 0;
        for(auto& kv : * extent_stack)
        {
            for(extent_stack_ext_lst l : *kv.second)
            length += l->size();
        }
        return length;
    }


    /*error prone double check*/
    list<Extent *> fill_gap(int num_left_to_add)
    {
        list<Extent *> ret;
        int temp = num_left_to_add;
        while(temp > 0) 
        {
            auto it = extent_stack->upper_bound(temp);
            if(it != extent_stack->begin())
            {
                it = prev(it);
            }
            extent_stack_ext_lst lst = it->second->front();
            it->second->pop_front();
            temp -= lst->size();
            for (auto e : *lst)
            {
                lst->pop_front();
                ret.push_back(e);
            }
            delete lst;
            if(it->second->size() == 0)
            {
                delete it->second;
                extent_stack->erase(it->first);
            }
        }
        return ret;
    }

    /*error prone double check*/
    list<Extent *> pop_stripe_num_exts(int stripe_size) 
    {
        list<Extent *> ret;
        int num_left_to_add = stripe_size;
        if(get_length_of_extent_stack() < num_left_to_add)
        {
            return ret;
        }
        int largest_key = prev(extent_stack->end())->first;
        extent_stack_ext_lst longest_lst = prev(extent_stack->end())->second->front();
        prev(extent_stack->end())->second->pop_front();
        for(int i = 0;i < (num_left_to_add > largest_key?largest_key:num_left_to_add); i++)
        {
            ret.push_back(longest_lst->front());
            longest_lst->pop_front();
        }
        num_left_to_add = stripe_size - ret.size();
        if(longest_lst->size() == 0)
        {
            delete longest_lst;
            extent_stack->erase(largest_key);
        }
        if(longest_lst->size() > 0)
        {
            add_extent(longest_lst);
        }
        if(num_left_to_add > 0)
        {
            for(Extent * e: fill_gap(num_left_to_add))
            {
                ret.push_back(e);
            }
        }
        return ret;
    }


    /* not sure if this is correct??????
    def get_length_at_key(self, key):
        """Returns the number of extents in extent stack at key
        """
        return self.get_length_of_extent_stack()
    */
   int get_length_at_key(int key) override
   {
       return get_length_of_extent_stack();
   }

   /*what's the point of passing key in....
   def get_extent_at_key(self, key):
        """Returns an extent at the given key.
        """
        key = min(self.extent_stack.keys())
        ext = self.extent_stack[key].pop(0)[0]
        if (len(self.extent_stack.get(key)) == 0):
            del self.extent_stack[key]
        return ext
   */
   Extent * get_extent_at_key(int k) override
   {
       Extent * ext = extent_stack->begin()->second->front()->front();
       extent_stack->begin()->second->pop_front();
       if(extent_stack->begin()->second->size() == 0)
       {
           extent_stack->erase(extent_stack->begin()->first);
       }
       return ext;
   }

    bool contains_extent(Extent * extent) override
    {
        for(auto& kv : *extent_stack){
            for(extent_stack_ext_lst l : *kv.second)
            {
                if(find(l->begin(), l->end(), extent) != l->end())
                {
                    return true;
                }
            }
        }
        return false;
    }

    void remove_extent(Extent * extent) override
    {
        for(auto& kv : *extent_stack){
            for(extent_stack_ext_lst l : *kv.second)
            {
                auto it = find(l->begin(), l->end(), extent);
                if( it != l->end())
                {
                    l->remove(extent);
                    if(l->size() == 0)
                    {
                        kv.second->remove(l);
                        delete l;
                    }
                }
            }
            if(kv.second->size() == 0)
            {
                delete kv.second;
                extent_stack->erase(kv.first);
            }
        }
    }
};