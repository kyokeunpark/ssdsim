#ifndef __SAMPLERS_H_
#define __SAMPLERS_H_

#include <cmath>
#include <cstdlib>
#include <chrono>
#include <random>
#include <vector>
#include <utility>

// Some sizes/lives return integer and some returns float.
// Hence, let it be type of float to remove any problems
using sizes = std::vector<float>;
using lives = std::vector<float>;
using sl_tuple = std::pair<sizes, lives>;

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
	float sim_time;
	unsigned seed;

public:
	Sampler(float sim_time)
	{
		this->sim_time = sim_time;
		this->seed = std::chrono::system_clock::now().time_since_epoch().count();
	}

	/*
	 * Returns a sl_tuple with object size and life
	 * @param: sim_time float
	 */
	virtual sl_tuple get_size_age_sample(const int num_samples = 1) = 0;

};

class SimpleSampler: public Sampler
{

public:
	SimpleSampler(float sim_time): Sampler(sim_time) {}

	sl_tuple get_size_age_sample(const int num_samples = 1)
	{
		return sl_tuple(this->sample_size(num_samples), this->sample_life(num_samples));
	}

private:

	/*
	 * Returns a list of integer as the size of an object, sampled from the
	 * distribution, the result is rounded to integer
	 */
	sizes sample_size(const int num_samples)
	{
		sizes sizes_lst = sizes();
		std::mt19937 generator(this->seed);
		std::uniform_real_distribution<double> real_dist(0.0, 100.0);

		for (int i = 0; i < num_samples; i++) {
			double temp = real_dist(generator);

			if (temp < 50)
				sizes_lst.emplace_back(randint(4, 10));
			else if (temp < 65)
				sizes_lst.emplace_back(randint(11, 50));
			else if (temp < 75.1)
				sizes_lst.emplace_back(randint(51, 100));
			else if (temp < 81.3)
				sizes_lst.emplace_back(randint(101, 200));
			else if (temp < 85.5)
				sizes_lst.emplace_back(randint(201, 300));
			else if (temp < 88)
				sizes_lst.emplace_back(randint(301, 400));
			else if (temp < 89.5)
				sizes_lst.emplace_back(randint(401, 500));
			else if (temp < 90.7)
				sizes_lst.emplace_back(randint(501, 600));
			else if (temp < 91.8)
				sizes_lst.emplace_back(randint(601, 700));
			else if (temp < 92.7)
				sizes_lst.emplace_back(randint(701, 800));
			else if (temp < 93.6)
				sizes_lst.emplace_back(randint(801, 900));
			else if (temp < 94)
				sizes_lst.emplace_back(randint(901, 1000));
			else if (temp < 95.2)
				sizes_lst.emplace_back(randint(1001, 1500));
			else if (temp < 96.2)
				sizes_lst.emplace_back(randint(1501, 2000));
			else
				sizes_lst.emplace_back(randint(2001, 3000));
		}

		return sizes_lst;
	}

	/*
	 * Returns a list of integer as the life of an object, sampled from the
	 * distribution, the result is rounded to integer
	 */
	lives sample_life(const int num_samples)
	{
		lives lives_lst = lives();
		std::mt19937 generator(this->seed);
		std::uniform_real_distribution<double> real_dist(0.0, 100.0);

		for (int i = 0; i < num_samples; i++) {
			double temp = real_dist(generator);

			if (temp < 5)
				lives_lst.emplace_back(1);
			else if (temp < 9)
				lives_lst.emplace_back(randint(2, 7));
			else if (temp < 12)
				lives_lst.emplace_back(randint(8, 30));
			else if (temp < 16)
				lives_lst.emplace_back(randint(31, 90));
			else if (temp < 26)
				lives_lst.emplace_back(randint(91, 365));
			else
				lives_lst.emplace_back(std::ceil(this->sim_time + 1));
		}

		return lives_lst;
	}

};

/*
 * Returns deterministic life and size samples
 */
class DeterministicDistributionSampler: public SimpleSampler
{

public:
	DeterministicDistributionSampler(float sim_time): SimpleSampler(sim_time)
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
	lives sample_life(const int num_samples)
	{
		lives lives_lst = lives();
		float a = 0.3;
		int scale = 28000;
		return lives_lst;
	}

};

class SanityCheckSampler1: public Sampler
{
	float obj_size;
	int turn;

public:
	SanityCheckSampler1(const float sim_time, const float obj_size):
		Sampler(sim_time)
	{
		this->obj_size = obj_size;
		this->turn = 0;
	}

	sl_tuple get_size_age_sample(const int num_samples)
	{
		return sl_tuple(this->sample_size(), this->sample_life());
	}

private:
	sizes sample_size()
	{
		if (this->turn < 1 or !(this->turn % 2))
			return { this->obj_size, this->obj_size };
		else
			return { this->obj_size };
	}

	/*
	 * Half othe created objects live for a day and the rest never get deleted.
	 */
	lives sample_life()
	{
		if (this->turn < 1 || !(this->turn % 2)) {
			this->turn++;
			return { this->sim_time + 1, 0.00001 };
		} else {
			this->turn++;
			return { this->sim_time + 1 };
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
	StripeLevelSanityCheckSampler2(const float sim_time, const int ext_size):
		Sampler(sim_time)
	{
		this->ext_size = ext_size;
		this->turn = 0;
	}

	sl_tuple get_size_age_sample(const int num_samples)
	{
		return sl_tuple(this->sample_size(), this->sample_life());
	}

private:
	sizes sample_size()
	{
		sizes sizes_lst = sizes();
		sizes lst = { (float)(0.9 * this->ext_size),
					(float)(this->ext_size - 0.9 * this->ext_size) };
		for (int i = 0; i < 14; i++)
			sizes_lst.insert(sizes_lst.end(), lst.begin(), lst.end());
		return sizes_lst;
	}

	/*
	 * Half of the created objects live for a day and the rest never get deleted
	 */
	lives sample_life()
	{
		lives lives_lst = lives();
		lives lst;

		if (this->turn < 1 or (this->turn - 1) % 10)
			lst = { this->sim_time + 1, 0.00001 };
		else
			lst = { this->sim_time + 1, this->sim_time + 1 };
		this->turn++;

		for (int i = 0; i < 14; i++)
			lives_lst.insert(lives_lst.end(), lst.begin(), lst.end());

		return lives_lst;
	}
};

#endif // __SAMPLERS_H_
