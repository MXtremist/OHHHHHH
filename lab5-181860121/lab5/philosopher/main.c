#include "lib.h"
#include "types.h"
#define MAX 5
sem_t mcount, forks[5];
int cnt;
void Philosopher(int id)
{
	while (1)
	{
		printf("Philosopher %d: think\n", id);
		sleep(getrand(128));
		sem_wait(&mcount);
		sem_wait(&forks[id - 1]);	//left
		sem_wait(&forks[id % 5]);	//right
		printf("Philosopher %d: eat\n", id);
		cnt++;
		if (cnt >= MAX)
			break;
		sleep(getrand(128));
		sem_post(&forks[id % 5]);	//right
		sem_post(&forks[id - 1]);	//left
		sem_post(&mcount);
	}
}

int main(void)
{
	// TODO in lab4
	printf("philosopher\n");
	for (int i = 0; i < 5; i++)
		sem_init(&forks[i], 1);
	sem_init(&mcount, 4);
	cnt = 0;
	for (int i = 0; i < 5; i++)
	{
		int ret = fork();
		if (ret == 0)
		{
			setrand(i + 1);
			Philosopher(i + 1);
			break;
		}
	}
	exit();
	return 0;
}
