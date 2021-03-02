#ifndef __SAMPLERS_H_
#define __SAMPLERS_H_

#include <cmath>
#include <cstdlib>
#include <chrono>
#include <random>
#include <vector>
#include <utility>

using samples = std::vector<int>;
using tuple = std::pair<samples, samples>;

static inline int randint(int min, int max) {
	return (rand() % ((max + 1) + min)) + min;
}

/*
 * Blueprint for implementing samplers for object size and life
 */
class Sampler {

protected:
	double sim_time;
	unsigned seed;

public:
	Sampler(double sim_time) {
		this->sim_time = sim_time;
		this->seed = std::chrono::system_clock::now().time_since_epoch().count();
	}

	/*
	 * Returns a tuple with object size and life
	 * @param: sim_time float
	 */
	virtual tuple get_size_age_sample(const int num_samples = 1) = 0;

};

class SimpleSampler: public Sampler {

public:
	SimpleSampler(double sim_time): Sampler(sim_time) {}

	tuple get_size_age_sample(const int num_samples = 1) {
		return tuple(this->sample_size(num_samples), this->sample_life(num_samples));
	}

private:

	/*
	 * Returns a list of integer as the size of an object, sampled from the
	 * distribution, the result is rounded to integer
	 */
	samples sample_size(const int num_samples) {
		samples sizes = samples();
		std::mt19937 generator(this->seed);
		std::uniform_real_distribution<double> real_dist(0.0, 100.0);

		for (int i = 0; i < num_samples; i++) {
			double temp = real_dist(generator);

			if (temp < 50)
				sizes.emplace_back(randint(4, 10));
			else if (temp < 65)
				sizes.emplace_back(randint(11, 50));
			else if (temp < 75.1)
				sizes.emplace_back(randint(51, 100));
			else if (temp < 81.3)
				sizes.emplace_back(randint(101, 200));
			else if (temp < 85.5)
				sizes.emplace_back(randint(201, 300));
			else if (temp < 88)
				sizes.emplace_back(randint(301, 400));
			else if (temp < 89.5)
				sizes.emplace_back(randint(401, 500));
			else if (temp < 90.7)
				sizes.emplace_back(randint(501, 600));
			else if (temp < 91.8)
				sizes.emplace_back(randint(601, 700));
			else if (temp < 92.7)
				sizes.emplace_back(randint(701, 800));
			else if (temp < 93.6)
				sizes.emplace_back(randint(801, 900));
			else if (temp < 94)
				sizes.emplace_back(randint(901, 1000));
			else if (temp < 95.2)
				sizes.emplace_back(randint(1001, 1500));
			else if (temp < 96.2)
				sizes.emplace_back(randint(1501, 2000));
			else
				sizes.emplace_back(randint(2001, 3000));
		}

		return sizes;
	}

	/*
	 * Returns a list of integer as the life of an object, sampled from the
	 * distribution, the result is rounded to integer
	 */
	samples sample_life(const int num_samples) {
		samples lives = samples();
		std::mt19937 generator(this->seed);
		std::uniform_real_distribution<double> real_dist(0.0, 100.0);

		for (int i = 0; i < num_samples; i++) {
			double temp = real_dist(generator);

			if (temp < 5)
				lives.emplace_back(1);
			else if (temp < 9)
				lives.emplace_back(randint(2, 7));
			else if (temp < 12)
				lives.emplace_back(randint(8, 30));
			else if (temp < 16)
				lives.emplace_back(randint(31, 90));
			else if (temp < 26)
				lives.emplace_back(randint(91, 365));
			else
				lives.emplace_back(std::ceil(this->sim_time + 1));
		}

		return lives;
	}

};

/*
 * Returns deterministic life and size samples
 */
class DeterministicDistributionSampler: public SimpleSampler {

public:
	DeterministicDistributionSampler(double sim_time): SimpleSampler(sim_time) {
		this->seed = 0;
		srand(0);
	}

};

#endif // __SAMPLERS_H_
