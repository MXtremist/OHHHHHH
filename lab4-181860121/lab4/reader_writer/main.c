#include "lib.h"
#include "types.h"

sem_t write_mutex, Rcount_mutex, RW_mutex;
int Rcount;

void Reader(int id)
{
	while (1)
	{
		sem_wait(&RW_mutex);
		sleep(getrand(64));
		sem_post(&RW_mutex);
		sem_wait(&Rcount_mutex);
		//sleep(getrand(128));
		read(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		Rcount++;
		//printf("++, %d Rcount\n", Rcount);
		write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		if (Rcount == 1)
		{
			sem_wait(&write_mutex);
		}
		sem_post(&Rcount_mutex);
		printf("Reader %d: read, total %d reader\n", id, Rcount);
		sleep(getrand(64));
		sem_wait(&Rcount_mutex);
		read(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		Rcount--;
		//printf("--, %d Rcount\n", Rcount);
		write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		//sleep(getrand(128));
		if (Rcount == 0)
		{
			sem_post(&write_mutex);
		}
		sem_post(&Rcount_mutex);
	}
}

void Writer(int id)
{
	while (1)
	{
		sem_wait(&RW_mutex);
		sem_wait(&write_mutex);
		printf("Writer %d: write\n", id);
		sleep(getrand(128));
		sem_post(&write_mutex);
		sem_post(&RW_mutex);
	}
}

int main(void)
{
	// TODO in lab4
	printf("reader_writer\n");
	sem_init(&write_mutex, 1);
	sem_init(&Rcount_mutex, 1);
	sem_init(&RW_mutex, 3);
	Rcount = 0;
	write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
	for (int i = 0; i < 6; i++)
	{
		int ret = fork();
		if (ret == 0)
		{
			setrand(i);
			if (i < 3) // % 2 == 0)
			{
				Writer(i + 1); // / 2 + 1);
			}
			else
			{
				Reader(i - 2); // / 2 + 1);
			}
			break;
		}
	}
	exit();
	return 0;
}
