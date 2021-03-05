#ifndef __CONFIGS_H_
#define __CONFIGS_H_

#include "data_center.h"

DataCenter stripe_level_with_no_exts_config(const unsigned long data_center_size,
			const float striping_cycle, const float simul_time, const int ext_size,
			const short threshold, const short num_stripes_per_cycle,
			const float deletion_cycle, const int num_objs);

#endif // __CONFIGS_H_
