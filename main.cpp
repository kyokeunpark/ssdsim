#include <cstdlib>
#include <iostream>

#include "samplers.h"

int main(int argc, char *argv[]) {
	int ext_size;
	short threshold;

	if (argc == 4) {
		ext_size = atol(argv[2]);
		threshold = atol(argv[3]);

		std::cout << ext_size << ", " << threshold << std::endl;
	} else {
		ext_size = 3 * 1024;
		threshold = 10;
	}

	const unsigned short ave_obj_size = 35000;
	const unsigned long int data_center_size = 3500000 * (unsigned long) ave_obj_size;
	const float striping_cycle = 1.0 / 12.0;
	const float deletion_cycle = striping_cycle;
	const short simul_time = 365;
	const int num_objs = 1000000;

	// Flag to record information about ext size distributions - small object extents, large obj exts, etc
	const bool record_ext_types = false;

	const int total_objs = num_objs / (365 / simul_time);

	const short num_stripes_per_cycle = 100;
	const short num_iterations = 1;
	const short secondary_threshold = threshold;

	SimpleSampler sampler = DeterministicDistributionSampler(simul_time);

	std::cout << data_center_size << std::endl;

    return 0;
}
