#include "lib.h"
#include "types.h"
#define MAX_PRODUCE_BUFFER 10
#define MAX_STEP 20

sem_t mutex, house_surplus, product_surplus;
int step_control = 0;
void consumer()
{
	//printf("Consumer: pid:%d\n", getpid());
	int rand;
	while (1)
	{
		sem_wait(&product_surplus);	//wait until have product to consume
		sem_wait(&mutex);
		step_control++;
		if (step_control >= MAX_STEP)
			break;
		rand = (getrand(64));
		sem_post(&mutex);
		sem_post(&house_surplus);	//consumed, house++
		sleep(rand);
		printf("Consumer: consume\n");
	}
}

void producer(int id)
{
	//printf("Producer %d: pid:%d\n", id, getpid());
	int rand = (getrand(96) + 64);
	while (1)
	{
		printf("Producer %d: produce\n", id);
		sleep(rand);
		sem_wait(&house_surplus);	//wait until have place to store
		sem_wait(&mutex);
		step_control++;
		if (step_control >= MAX_STEP)
			break;
		rand = (getrand(96) + 64);
		sem_post(&mutex);
		sem_post(&product_surplus);	//produced, product--
	}
}

int main(void)
{
	// TODO in lab4
	printf("bounded_buffer\n");
	sem_init(&mutex, 1);
	sem_init(&house_surplus, MAX_PRODUCE_BUFFER);
	sem_init(&product_surplus, 0);
	for (int i = 0; i < 5; i++)
	{
		int ret = fork();
		if (ret == 0)
		{
			setrand(i + 1);
			//make 1 consumer and 4 producer
			if (i == 0)
				consumer();
			else
			{
				producer(i);
			}
			break;
		}
	}
	exit();
	return 0;
}
