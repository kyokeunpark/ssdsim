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

static inline int randint(int min, int max)
{
	return (rand() % ((max + 1) + min)) + min;
}

/*
 * Blueprint for implementing samplers for object size and life
 */
class Sampler
{

protected:
	double sim_time;
	unsigned seed;

public:
	Sampler(double sim_time)
	{
		this->sim_time = sim_time;
		this->seed = std::chrono::system_clock::now().time_since_epoch().count();
	}

	/*
	 * Returns a tuple with object size and life
	 * @param: sim_time float
	 */
	virtual tuple get_size_age_sample(const int num_samples = 1) = 0;

};

class SimpleSampler: public Sampler
{

public:
	SimpleSampler(double sim_time): Sampler(sim_time) {}

	tuple get_size_age_sample(const int num_samples = 1)
	{
		return tuple(this->sample_size(num_samples), this->sample_life(num_samples));
	}

private:

	/*
	 * Returns a list of integer as the size of an object, sampled from the
	 * distribution, the result is rounded to integer
	 */
	samples sample_size(const int num_samples)
	{
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
	samples sample_life(const int num_samples)
	{
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
class DeterministicDistributionSampler: public SimpleSampler
{

public:
	DeterministicDistributionSampler(double sim_time): SimpleSampler(sim_time)
	{
		this->seed = 0;
		srand(0);
	}

};

/*
 * A sampler where the life distribution of the Giza CDF has been approximated
 * by a Weibull function.
 *
 * TODO: Need to figure out implementing this properly with C++
 */
class WeibullSampler: public DeterministicDistributionSampler
{

public:
	samples sample_life(const int num_samples)
	{
		samples lives = samples();
		float a = 0.3;
		int scale = 28000;
		return lives;
	}

};

class SanityCheckSampler1: public Sampler
{
	int obj_size, turn;

public:
	SanityCheckSampler1(const double sim_time, const int obj_size):
		Sampler(sim_time)
	{
		this->obj_size = obj_size;
		this->turn = 0;
	}

	tuple get_size_age_sample(const int num_samples)
	{
		return tuple(this->sample_size(), this->sample_life());
	}

	samples sample_size()
	{
		if (this->turn < 1 or !(this->turn % 2))
			return { this->obj_size, this->obj_size };
		else
			return { this->obj_size };
	}

	/*
	 * Half othe created objects live for a day and the rest never get deleted.
	 */
	samples sample_life()
	{
		if (this->turn < 1 || !(this->turn % 2)) {
			this->turn++;

			/*
			 * TODO
			 * Original Python code returns [ self.sim_time + 1, 0.00001 ].
			 * Looking at the other implementation of this function,
			 * they return list of INTEGERS.
			 */
			return { (int)std::ceil(this->sim_time + 1), 1 };
		} else {
			this->turn++;
			return { (int)std::ceil(this->sim_time + 1) };
		}
	}
};

/*
 * 1 Stripe create in total, 0.1 of stripe is deleted each cycle
 */
class StripeLevelSanityCheckSampler2: public Sampler
{
	int ext_size, turn;

public:
	StripeLevelSanityCheckSampler2(const double sim_time, const int ext_size):
		Sampler(sim_time)
	{
		this->ext_size = ext_size;
		this->turn = 0;
	}

	samples sample_size()
	{
		samples sizes = samples();
		samples lst = { (int)(0.9 * this->ext_size),
					(int)(this->ext_size - 0.9 * this->ext_size) };
		for (int i = 1; i < 14; i++)
			sizes.insert(sizes.end(), lst.begin(), lst.end());
		return sizes;
	}

	/*
	 * Half of the created objects live for a day and the rest never get deleted
	 */
	samples sample_life()
	{
		samples lives = samples();
		samples lst;

		if (this->turn < 1 or (this->turn - 1) % 10)
			lst = { (int)(this->sim_time + 1), 1 };
		else
			/*
			 * TODO
			 * Similar story here with SanityCheckSampler1
			 */
			lst = { (int)(this->sim_time + 1), (int)(this->sim_time + 1) };
		this->turn++;

		for (int i = 1; i < 14; i++)
			lives.insert(lives.end(), lst.begin(), lst.end());

		return lives;
	}
}

#endif // __SAMPLERS_H_
