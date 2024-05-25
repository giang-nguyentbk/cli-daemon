#include <stdio.h>
#include "traceUtils.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

// unsigned long int calc_time_diff(struct timespec t_start, struct timespec t_end)
// {
// 	unsigned long int diff = 0;

// 	if(t_end.tv_nsec < t_start.tv_nsec)
// 	{
// 		diff = (t_end.tv_sec - t_start.tv_sec)*1000000000 - (t_start.tv_nsec - t_end.tv_nsec);
// 	} else
// 	{
// 		diff = (t_end.tv_sec - t_start.tv_sec)*1000000000 + (t_end.tv_nsec - t_start.tv_nsec);
// 	}

// 	return diff; 
// }

void * test_thread(void *data)
{
	(void)data;

	prctl(PR_SET_NAME, "test_thread");
	LOG_INFO("Frequency gain = %.3f\n", 2.145);
	LOG_ERROR("MeasGroup %d not found!\n", 1);

	sleep(2);

	LOG_ABN("Unknown direction %s!\n", "DL_1");
	LOG_DEBUG("Debugging mode is enabled!\n");

	return NULL;
}

int main(void)
{
	/* Used for LOG_MACRO time consuming measurement */
	// struct timespec t_start;
	// struct timespec t_end;

	// clock_gettime(CLOCK_REALTIME, &t_start);
	LOG_INFO("New cycle was started!\n");
	// printf("New cycle was started!\n");
	// clock_gettime(CLOCK_REALTIME, &t_end);

	// unsigned long int difftime = calc_time_diff(t_start, t_end);
	// LOG_INFO("Time needed for LOG_INFO = %lu (ns) -> %lu (ms)!\n", difftime, difftime/1000000);

	LOG_ERROR("Failed to get power boost on port %d\n", 3);

	pthread_t threadid;
	pthread_create(&threadid, NULL, &test_thread, NULL);

	sleep(1);

	LOG_ABN("DB key /abc/def/ not found, use default value = %d!\n", 22);
	LOG_DEBUG("This trace used for debugging!\n");
	
	pthread_join(threadid, NULL);

	return 0;
}