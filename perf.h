#ifndef JOT_PERF
#define JOT_PERF

#include <stdbool.h>
#include <math.h>
#include <stdint.h>

#ifndef ASSERT
	#include <assert.h>
	#define ASSERT(x) assert(x)
#endif

#ifndef EXPORT
	#define EXPORT 
#endif

typedef int64_t isize;

typedef struct Perf_Counter {
	int64_t counter;
	int64_t runs;
	int64_t frquency;
	int64_t mean_estimate;
	int64_t sum_of_squared_offset_counters;
	int64_t max_counter;
	int64_t min_counter;
} Perf_Counter;

typedef struct Perf_Stats {
	int64_t runs;
	int64_t batch_size;

	double total_s;
	double average_s;
	double min_s;
	double max_s;
	double standard_deviation_s;
	double normalized_standard_deviation_s; //(σ/μ)
} Perf_Stats;

typedef struct Perf_Benchmark {
	isize iter;			
	int64_t start_time;
	int64_t now;																			
	int64_t freq;																			
	Perf_Counter counter;																		
	bool discard;																			
	bool _pad[7];	
} Perf_Benchmark;


EXPORT int64_t		perf_start();
EXPORT void			perf_end(Perf_Counter* counter, int64_t measure);
EXPORT void			perf_end_delta(Perf_Counter* counter, int64_t measure);
EXPORT void			perf_end_atomic(Perf_Counter* counter, int64_t measure);
EXPORT int64_t		perf_end_atomic_delta(Perf_Counter* counter, int64_t measure, bool detailed);
EXPORT Perf_Stats	perf_get_stats(Perf_Counter counter, int64_t batch_size);

//Prevents the compiler from otpimizing away the variable (and thus its value) pointed to by ptr.
EXPORT void perf_do_not_optimize(const void* ptr) ;

//benchmarks a block of code for time seconds and saves the result int `stats_ptr`. See the perf_benchmark_example() function below for an example
#define perf_benchmark_batch(/* Perf_Stats* */ stats_ptr, /*identifier*/ bench_variable_name, /*double*/warmup, /*double*/ time, /*int*/ batch_size);
#define perf_benchmark(/* Perf_Stats* */ stats_ptr, /*identifier*/ bench_variable_name, /*double*/ time);

//Needs implementation:
int64_t platform_perf_counter();
int64_t platform_perf_counter_frequency();
inline static bool    platform_atomic_compare_and_swap64(volatile int64_t* target, int64_t old_value, int64_t new_value);
inline static int32_t platform_atomic_add32(volatile int32_t* target, int32_t value);
inline static int64_t platform_atomic_add64(volatile int64_t* target, int64_t value);
inline static int32_t platform_atomic_sub32(volatile int32_t* target, int32_t value);
inline static int64_t platform_atomic_sub64(volatile int64_t* target, int64_t value);

//INLINE IMPLEMENTATION!
#undef perf_benchmark_batch
#undef perf_benchmark

#define perf_benchmark_batch(stats_ptr, bench, warmup, time, batch_size)						\
    for(Perf_Benchmark bench = {0}; \
		bench.freq = platform_perf_counter_frequency(), \
		bench.start_time = platform_perf_counter(), \
		bench._pad[0] == false;  \
		bench._pad[0] = true, *(stats_ptr) = perf_get_stats(bench.counter, (batch_size)))	\
		for(												\
            /*Init */										\
            int64_t											\
			_total_clocks = (int64_t) ((double) bench.freq * (time)),							\
			_warmup_clocks = (int64_t) ((double) bench.freq * (warmup)),						\
            _before = bench.start_time,								\
            _after = bench.start_time,								\
            _discard_time = 0,								\
            _delta = 0,										\
			_passed_clocks = 0;								\
															\
            /*Check*/										\
            _before = platform_perf_counter(),				\
            _passed_clocks = _before - bench.start_time,	\
            _passed_clocks < _total_clocks + _discard_time; \
															\
            /*Increment*/									\
            _after = platform_perf_counter(),				\
            _delta = _after - _before,						\
            bench.discard ? _discard_time += _delta : 0,	\
            !bench.discard && _passed_clocks >= _warmup_clocks + _discard_time ? perf_end_delta(&bench.counter, _delta) : (void) 0, \
            bench.iter++) \

#define perf_benchmark(stats_ptr, bench, time) perf_benchmark_batch((stats_ptr), bench, (time) / 10, (time), 1)	

#include <stdlib.h>
static void perf_benchmark_example() 
{	
	Perf_Stats stats = {0};
	perf_benchmark(&stats, it, 3) {
		volatile double result = sqrt((double) it.iter); (void) result; //make sure the result is not optimized away
	};

	//Sometimes it is necessary to do contiguous setup in order to
	// have data to benchmark with. In such a case every itration where the setup
	// occurs will be havily influenced by it. We can discard this iteration by
	// setting it.discard = true;

	//We benchmark the free function. In order to have something to free we need
	// to call malloc. But we dont care about malloc in this test => malloc 100
	// items and then free each. Discard the expensive malloc op.
	void* ptrs[100] = {0};
	int count = 0;
	perf_benchmark(&stats, it, 3) {
		if(count > 0)
			free(ptrs[--count]);
		else
		{
			count = 100;
			for(int i = 0; i < 100; i++)
				ptrs[i] = malloc(256);

			it.discard = true;
		}
	};
}

#endif

#if (defined(JOT_ALL_IMPL) || defined(JOT_PERF_IMPL)) && !defined(JOT_PERF_HAS_IMPL)
#define JOT_PERF_HAS_IMPL

	
	EXPORT void perf_end_delta(Perf_Counter* counter, int64_t delta)
	{
		ASSERT(counter != NULL && delta >= 0 && "invalid Global_Perf_Counter_Running submitted");

		int64_t runs = counter->runs;
		counter->runs += 1; 
		if(runs == 0)
		{
			counter->frquency = platform_perf_counter_frequency();
			counter->max_counter = INT64_MIN;
			counter->min_counter = INT64_MAX;
			counter->mean_estimate = delta;
		}
	
		int64_t offset_delta = delta - counter->mean_estimate;
		counter->counter += delta;
		counter->sum_of_squared_offset_counters += offset_delta*offset_delta;
		counter->min_counter = MIN(counter->min_counter, delta);
		counter->max_counter = MAX(counter->max_counter, delta);
	}
	
	EXPORT int64_t perf_end_atomic_delta(Perf_Counter* counter, int64_t delta, bool detailed)
	{
		ASSERT(counter != NULL && delta >= 0 && "invalid Global_Perf_Counter_Running submitted");
		int64_t runs = platform_atomic_add64(&counter->runs, 1); 
		
		//only save the stats that dont need to be updated on the first run
		if(runs == 0)
		{
			counter->frquency = platform_perf_counter_frequency();
			counter->max_counter = INT64_MIN;
			counter->min_counter = INT64_MAX;
			counter->mean_estimate = delta;
		}
	
		platform_atomic_add64(&counter->counter, delta);

		if(detailed)
		{
			int64_t offset_delta = delta - counter->mean_estimate;
			platform_atomic_add64(&counter->sum_of_squared_offset_counters, offset_delta*offset_delta);

			do {
				if(counter->min_counter <= delta)
					break;
			} while(platform_atomic_compare_and_swap64(&counter->min_counter, counter->min_counter, delta) == false);

			do {
				if(counter->max_counter >= delta)
					break;
			} while(platform_atomic_compare_and_swap64(&counter->max_counter, counter->max_counter, delta) == false);
		}

		return runs;
	}
	
	EXPORT int64_t perf_start()
	{
		return platform_perf_counter();
	}

	EXPORT void perf_end(Perf_Counter* counter, int64_t measure)
	{
		int64_t delta = platform_perf_counter() - measure;
		perf_end_delta(counter, delta);
	}

	EXPORT void perf_end_atomic(Perf_Counter* counter, int64_t measure)
	{
		int64_t delta = platform_perf_counter() - measure;
		perf_end_atomic_delta(counter, delta, true);
	}
	EXPORT Perf_Stats perf_get_stats(Perf_Counter counter, int64_t batch_size)
	{
		if(batch_size <= 0)
			batch_size = 1;

		if(counter.frquency == 0)
			counter.frquency = platform_perf_counter_frequency();

		ASSERT(counter.min_counter * counter.runs <= counter.counter && "min must be smaller than sum");
		ASSERT(counter.max_counter * counter.runs >= counter.counter && "max must be bigger than sum");
        
		//batch_size is in case we 'batch' our tested function: 
		// ie instead of measure the tested function once we run it 100 times
		// this just means that each run is multiplied batch_size times
		int64_t iters = batch_size * (counter.runs);
        
		double batch_deviation_s = 0;
		if(counter.runs > 1)
		{
			double n = (double) counter.runs;
			double sum = (double) counter.counter;
			double sum2 = (double) counter.sum_of_squared_offset_counters;
            
			//Welford's algorithm for calculating varience
			double varience_ns = (sum2 - (sum * sum) / n) / (n - 1.0);

			//deviation = sqrt(varience) and deviation is unit dependent just like mean is
			batch_deviation_s = sqrt(fabs(varience_ns)) / (double) counter.frquency;
		}

		double total_s = 0.0;
		double mean_s = 0.0;
		double min_s = 0.0;
		double max_s = 0.0;

		ASSERT(counter.min_counter * counter.runs <= counter.counter);
		ASSERT(counter.max_counter * counter.runs >= counter.counter);
		if(counter.frquency != 0)
		{
			total_s = (double) counter.counter / (double) counter.frquency;
			min_s = (double) counter.min_counter / (double) (batch_size * counter.frquency);
			max_s = (double) counter.max_counter / (double) (batch_size * counter.frquency);
		}
		if(iters != 0)
			mean_s = total_s / (double) iters;

		ASSERT(mean_s >= 0 && min_s >= 0 && max_s >= 0);

		//We assume that summing all measure times in a batch 
		// (and then dividing by its size = making an average)
		// is equivalent to picking random samples from the original distribution
		// => Central limit theorem applies which states:
		// deviation_sampling = deviation / sqrt(samples)
        
		// We use this to obtain the original deviation
		// => deviation = deviation_sampling * sqrt(samples)
        
		// but since we also need to take the average of each batch
		// to get the deviation of a single element we get:
		// deviation_element = deviation_sampling * sqrt(samples) / samples
		//                   = deviation_sampling / sqrt(samples)

		double sqrt_batch_size = sqrt((double) batch_size);
		Perf_Stats stats = {0};

		//since min and max are also somewhere within the confidence interval
		// keeping the same confidence in them requires us to also apply the same correction
		// to the distance from the mean (this time * sqrt_batch_size because we already 
		// divided by batch_size when calculating min_s)
		stats.min_s = mean_s + (min_s - mean_s) * sqrt_batch_size; 
		stats.max_s = mean_s + (max_s - mean_s) * sqrt_batch_size; 

		//the above correction can push min to be negative 
		// happens mostly with noop and generally is not a problem
		if(stats.min_s < 0.0)
			stats.min_s = 0.0;
		if(stats.max_s < 0.0)
			stats.max_s = 0.0;
	
		stats.total_s = total_s;
		stats.standard_deviation_s = batch_deviation_s / sqrt_batch_size;
		stats.average_s = mean_s; 
		stats.batch_size = batch_size;
		stats.runs = iters;

		if(stats.average_s > 0)
			stats.normalized_standard_deviation_s = stats.standard_deviation_s / stats.average_s;

		//statss must be plausible
		ASSERT(stats.runs >= 0);
		ASSERT(stats.batch_size >= 0);
		ASSERT(stats.total_s >= 0.0);
		ASSERT(stats.average_s >= 0.0);
		ASSERT(stats.min_s >= 0.0);
		ASSERT(stats.max_s >= 0.0);
		ASSERT(stats.standard_deviation_s >= 0.0);
		ASSERT(stats.normalized_standard_deviation_s >= 0.0);

		return stats;
	}
	
	EXPORT void perf_do_not_optimize(const void* ptr) 
	{ 
		#if defined(__GNUC__) || defined(__clang__)
			__asm__ __volatile__("" : "+r"(ptr))
		#else
			static volatile int __perf_always_zero = 0;
			if(__perf_always_zero == 0x7FFFFFFF)
			{
				volatile int* vol_ptr = (volatile int*) (void*) ptr;
				//If we would use the following line the compiler could infer that 
				//we are only really modifying the value at ptr. Thus if we did 
				// perf_do_not_optimize(long_array) it would gurantee no optimize only at the first element.
				//The precise version is also not very predictable. Often the compilers decide to only keep the first element
				// of the array no metter which one we actually request not to optimize. 
				//
				// __perf_always_zero = *vol_ptr;
				__perf_always_zero = vol_ptr[*vol_ptr];
			}
		#endif
    }


#endif