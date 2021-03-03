//PLACEHOLDER for managers! pls change it to actual sampler class!!!
#include <chrono>
#include <vector>
using samples = std::vector<int>;
using tuple = std::pair<samples, samples>;
class Sampler {

protected:
	double sim_time;
	unsigned seed;

public:
	Sampler(double sim_time) {
		this->sim_time = sim_time;
		this->seed = std::chrono::system_clock::now().time_since_epoch().count();
	}

	tuple get_size_age_sample(const int num_samples = 1){return tuple({},{});};

};