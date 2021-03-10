#include "extent.h"
#include <unordered_map>
class ExtentManager{
    public:
        int ext_size;
        list<Extent *>* exts;

        ExtentManager(int s):ext_size(s){

        }

        int get_num_ext(){
            return exts->size();
        }

        Extent * create_extent(int s = 0, int secondary_threshold=15){
            Extent * e;
            if(s == 0){
                e = new Extent(ext_size, secondary_threshold);
            }else{
                e = new Extent(s, secondary_threshold);
            }
            exts->push_back(e);
            return e;
        }

        unordered_map<string, int> get_ext_types()
        {
            unordered_map<string, int> ret;
            for (Extent * e : *exts)
            {
                ret.emplace(e->type, 0);
                ret[e->type] += 1;
            }
            return ret;
        }
};