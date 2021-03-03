#include "extent.h"
#include <vector>
using namespace std;
class Stripe{
    public:
        double obsolete;
        int num_data_blocks;
        int num_localities;
        double free_space;
        vector<int>* localities;
        int ext_size;
        double timestamp;
        int stripe_size;
        int primary_threshold;
        list<Extent*>* extents;
        
        Stripe(int num_data_extents_per_locality, 
        int num_localities, int ext_size, int primary_threshold)
        :obsolete(0),num_data_blocks(num_data_extents_per_locality),
        num_localities(num_localities), 
        free_space(num_localities*num_data_extents_per_locality),
        localities(new vector<int>(num_localities, 0)), ext_size(ext_size), timestamp(0),
        stripe_size(0), primary_threshold(primary_threshold),extents(new list<Extent*>())
        {
            for(int i = 0; i < num_data_blocks * num_localities; ++i)
            {
                this->stripe_size += ext_size;
            }
        }

        //????the python code doesnt seem right, need to ask///
        /*    def update_obsolete(self, obsolete):
        """
        Updates the amount of obsolete data in this stripe.

        Returns the percent of obsolete data.
        :param obsolete: float
        :rtype: float
        """
        self.obsolete += obsolete*/
        double update_obsolete(double obsolete)
        {
            this->obsolete += obsolete;
            return this->obsolete/stripe_size * 100;
        }

        double get_obsolete_percentage(){
            return obsolete/stripe_size*100
        }

        int get_num_data_exts()
        {
            return num_data_blocks * num_localities;
        }

        void add_extent(Extent * ext)
        {
            if(free_space)
            {
                extents->push_back(ext);
                free_space-=1;
                int locality = 0;
                while((*localities)[locality] == num_data_blocks)
                {
                    locality += 1
                }
                (*localities)[locality] += 1;
                ext->locality = locality;
                if(ext->timestamp > timestamp)
                {
                    this->timestamp = ext->timestamp;
                }
            }else
            {
                cerr << "Attempt to add extent to a full stripe";
            }
        }

        void del_extent(Extent * ext)
        {
            ext->remove_objects();
            (*localities)[ext->locality] -= 1;
            obsolete -= ext->obsolete_space;
            extents->remove(ext);
            free_space += 1;
        }


};
