#include "extent_object.h"
#include "object_manager.h"
#include "extent_manager.h"
#include "extent_stack.h"
#include <cstdlib>
#include <map>
#include <list>
#include <memory>
class ObjectPacker
{
    public:
        virtual void add_obj() = 0;
        virtual void pack_objects() = 0;
};

class GenericObjectPacker
{
    public:
        shared_ptr<ObjectManager>object_manager;
        shared_ptr<ExtentManager>extent_manager;
        int num_objs_in_pool;
        map<string, int>ext_types;
        int threshold;
        shared_ptr<unordered_map<Extent_Object*, double > > object_pool;
        shared_ptr<map<int, Extent *>> current_extents;
        bool record_ext_types;
        GenericObjectPacker(shared_ptr<ObjectManager>o_m, shared_ptr<ExtentManager>e_m, int num_o = 100, int threshold = 10,bool record_ext_t = false,
            shared_ptr<unordered_map<Extent_Object*, double >> obj_p = make_shared<unordered_map<Extent_Object*, double > >(), 
            shared_ptr<map<int, Extent *>> crt_exts = make_shared<map<int, Extent *>>()
            ): object_manager(o_m), extent_manager(e_m),
                 num_objs_in_pool(num_o),object_pool(obj_p),current_extents(crt_exts),
                 record_ext_types(record_ext_t)
        {
            srand(0);
        }
        void add_obj(Extent* obj, int size)
        {

        }
        void add_objs(unordered_map<Extent_Object*, double >  objlst)
        {
            object_pool->emplace(object_pool->end(), objlst.begin(), objlst.end());
        }
        virtual void pack_objects(ExtentStack<class V, class K> extent_stack, int key = 0) = 0;

        template<class V, class K>
        void generate_exts_at_key(ExtentStack<V,K> * extent_stack, int num_exts, int key)
        {
            int num_exts_at_key = extent_stack->get_length_at_key(key);
            while (num_exts_at_key < num_exts)
            {
                auto objs = object_manager->create_new_object();
                unordered_map<Extent_Object*, double > objlst;
                for(auto o : objs)
                {
                    objlst.emplace(o, o->size);
                }
                add_objs(objlst);
                pack_objects(extent_stack);
                num_exts_at_key = extent_stack.get_length_at_key(key);
            }
        } 

        template<class V, class K, class sim_T>
        void generate_stripes(ExtentStack<V,K> * extent_stack, sim_T simulation_time)
        {
            if (object_pool->size() < num_objs_in_pool)
            {
                auto objs = object_manager->create_new_object(num_objs_in_pool - object_pool->size());
                unordered_map<Extent_Object*, double > objlst;
                for(auto o : objs)
                {
                    objlst.emplace(o, o->size);
                }
                add_objs(objlst);
            }
            pack_objects(extent_stack);
        }

};

class SimpleObjectPacker : public GenericObjectPacker
{
    public:
    using GenericObjectPacker::GenericObjectPacker;
    void pack_objects(ExtentStack<class V, class K> extent_stack, int key = 0){}
};

class SimpleGCObjectPacker : public SimpleObjectPacker
{
    public:
        using SimpleObjectPacker::SimpleObjectPacker;
        template<class V, class K>
        void gc_extent(shared_ptr<Extent> ext, shared_ptr<ExtentStack<V, K>> extent_stack, list<Extent_Object *> objs)
        {}

        void generate_extents()
        {
            if (object_pool->size() < num_objs_in_pool)
            {
                auto objs = object_manager->create_new_object(num_objs_in_pool - object_pool->size());
                unordered_map<Extent_Object*, double > objlst;
                for(auto o : objs)
                {
                    objlst.emplace(o, o->size);
                }
                add_objs(objlst);
            }    
        }
        void generate_objs(int space)
        {
            while (space > 0){
                auto objs = object_manager->create_new_object();
                space -= objs[0]->size;
                unordered_map<Extent_Object*, double > objlst;
                objlst.emplace(objs[0], objs[0]->size);
                add_objs(objlst);
            }
        }

};

class MixedObjObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class MixedObjGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class AgeBasedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class AgeBasedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerBaseline:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerBaseline:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerSmallerObj:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerSmallerObj:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};
class SizeBasedObjectPackerDynamicStrategy:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerDynamicStrategy:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerSmallerWholeObjFillGap:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};
class SizeBasedGCObjectPackerSmallerWholeObjFillGap:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class SizeBasedObjectPackerLargerWholeObj:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class SizeBasedGCObjectPackerLargerWholeObj:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class MortalImmortalObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class MortalImmortalGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class RandomizedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class RandomizedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class AgeBasedRandomizedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class AgeBasedRandomizedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};

class GenerationBasedObjectPacker:public SimpleObjectPacker{
    public:
    using SimpleObjectPacker::SimpleObjectPacker;
};

class GenerationBasedGCObjectPacker:public SimpleGCObjectPacker{
    public:
    using SimpleGCObjectPacker::SimpleGCObjectPacker;
};