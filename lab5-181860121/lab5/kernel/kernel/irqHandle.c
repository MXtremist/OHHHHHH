#include "x86.h"
#include "device.h"
#include "fs.h"

#define SYS_WRITE 0
#define SYS_FORK 1
#define SYS_EXEC 2
#define SYS_SLEEP 3
#define SYS_EXIT 4
#define SYS_READ 5
#define SYS_SEM 6
#define SYS_GETPID 7
#define SYS_OPEN 8
#define SYS_CLOSE 9
#define SYS_LSEEK 10
#define SYS_REMOVE 11
#define SYS_GETFILESIZE 12

#define STD_OUT 0
#define STD_IN 1
#define SH_MEM 3

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

#define O_WRITE 0x01
#define O_READ 0x02
#define O_CREATE 0x04
#define O_DIRECTORY 0x08

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

extern File fcb[MAX_FILE_NUM];

uint8_t shMem[MAX_SHMEM_SIZE];

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallFork(struct TrapFrame *tf);
void syscallExec(struct TrapFrame *tf);
void syscallSleep(struct TrapFrame *tf);
void syscallExit(struct TrapFrame *tf);
void syscallSem(struct TrapFrame *tf);
void syscallGetPid(struct TrapFrame *tf);

void syscallWriteStdOut(struct TrapFrame *tf);
void syscallReadStdIn(struct TrapFrame *tf);
void syscallWriteShMem(struct TrapFrame *tf);
void syscallReadShMem(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void timerHandle(struct TrapFrame *tf);
void keyboardHandle(struct TrapFrame *tf);

void syscallSemInit(struct TrapFrame *tf);
void syscallSemWait(struct TrapFrame *tf);
void syscallSemPost(struct TrapFrame *tf);
void syscallSemDestroy(struct TrapFrame *tf);

void syscallReadFile(struct TrapFrame *tf);
void syscallWriteFile(struct TrapFrame *tf);
void syscallOpen(struct TrapFrame *tf);
void syscallClose(struct TrapFrame *tf);
void syscallLseek(struct TrapFrame *tf);
void syscallRemove(struct TrapFrame *tf);
void syscallGetFileSize(struct TrapFrame *tf);

void irqHandle(struct TrapFrame *tf)
{ // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));

	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)tf;

	switch (tf->irq)
	{
	case -1:
		break;
	case 0xd:
		GProtectFaultHandle(tf); // return
		break;
	case 0x20:
		timerHandle(tf); // return or iret
		break;
	case 0x21:
		keyboardHandle(tf); // return
		break;
	case 0x80:
		syscallHandle(tf); // return
		break;
	default:
		assert(0);
	}

	pcb[current].stackTop = tmpStackTop;
}

void syscallHandle(struct TrapFrame *tf)
{
	switch (tf->eax)
	{ // syscall number
	case SYS_WRITE:
		syscallWrite(tf);
		break; // for SYS_WRITE
	case SYS_READ:
		syscallRead(tf);
		break; // for SYS_READ
	case SYS_FORK:
		syscallFork(tf);
		break; // for SYS_FORK
	case SYS_EXEC:
		syscallExec(tf);
		break; // for SYS_EXEC
	case SYS_SLEEP:
		syscallSleep(tf);
		break; // for SYS_SLEEP
	case SYS_EXIT:
		syscallExit(tf);
		break; // for SYS_EXIT
	case SYS_SEM:
		syscallSem(tf);
		break; // for SYS_SEM
	case SYS_GETPID:
		syscallGetPid(tf);
		break; // for SYS_GETPID
	case SYS_OPEN:
		syscallOpen(tf);
		break;
	case SYS_CLOSE:
		syscallClose(tf);
		break;
	case SYS_LSEEK:
		syscallLseek(tf);
		break;
	case SYS_REMOVE:
		syscallRemove(tf);
		break;
	case SYS_GETFILESIZE:
		syscallGetFileSize(tf);
		break;
	default:
		break;
	}
}

void timerHandle(struct TrapFrame *tf)
{
	uint32_t tmpStackTop;
	int i = (current + 1) % MAX_PCB_NUM;
	while (i != current)
	{
		if (pcb[i].state == STATE_BLOCKED)
		{
			pcb[i].sleepTime--;
			if (pcb[i].sleepTime == 0)
			{
				pcb[i].state = STATE_RUNNABLE;
				//putInt(i);
			}
		}
		i = (i + 1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
		pcb[current].timeCount != MAX_TIME_COUNT)
	{
		pcb[current].timeCount++;
		return;
	}
	else
	{
		if (pcb[current].state == STATE_RUNNING)
		{
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		i = (current + 1) % MAX_PCB_NUM;
		while (i != current)
		{
			if (i != 0 && pcb[i].state == STATE_RUNNABLE)
			{
				break;
			}
			i = (i + 1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE)
		{
			i = 0;
		}
		current = i;
		// putChar('0' + current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t) & (pcb[current].stackTop);
		asm volatile("movl %0, %%esp" ::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
	return;
}

void keyboardHandle(struct TrapFrame *tf)
{
	// TODO in lab4
	uint32_t key;
	key = getKeyCode();
	//putChar(getChar(key));
	if ((bufferTail + 1) % MAX_KEYBUFFER_SIZE != bufferHead)
	{
		keyBuffer[bufferTail] = key;
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
	}
	else
	{
		//overflow
		putString("KeyboardHandle:Keybuffer overflow!");
		return;
	}
	//putInt(-dev[STD_IN].value);
	if (dev[STD_IN].value < 0 && getChar(key) == '\n')
	{
		//putString("hello");
		ProcessTable *pt = (ProcessTable *)((uint32_t)(dev[STD_IN].pcb.prev) - (uint32_t) & (((ProcessTable *)0)->blocked));
		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;
		dev[STD_IN].value++;
	}
	return;
}

void syscallWrite(struct TrapFrame *tf)
{
	switch (tf->ecx)
	{ // file descriptor
	case STD_OUT:
		if (dev[STD_OUT].state == 1)
		{
			syscallWriteStdOut(tf);
		}
		break; // for STD_OUT
	case SH_MEM:
		if (dev[SH_MEM].state == 1)
		{
			syscallWriteShMem(tf);
		}
		break; // for SH_MEM
	default:
		syscallWriteFile(tf);
		break;
	}
}

void syscallWriteStdOut(struct TrapFrame *tf)
{
	int sel = tf->ds; //TODO segment selector for user data, need further modification
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(character)
					 : "r"(str + i));
		if (character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if (displayRow == 25)
			{
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80)
			{
				displayRow++;
				displayCol = 0;
				if (displayRow == 25)
				{
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
	}

	updateCursor(displayRow, displayCol);
	//TODO take care of return value
}

void syscallWriteShMem(struct TrapFrame *tf)
{
	// TODO in lab4
	int sel = tf->ds;
	uint8_t *str = (uint8_t *)tf->edx;
	int size = tf->ebx;
	int index = tf->esi;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	uint8_t c;
	while (i < size)
	{
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(c)
					 : "r"(str + i));
		shMem[i + index] = c;
		i++;
		if (i + index == MAX_SHMEM_SIZE)
		{
			//shmem overflow
			putString("syscallWriteShMem:ShMem overflow!");
			i = -1;
			break;
		}
	}
	pcb[current].regs.eax = i;
	return;
}

void syscallRead(struct TrapFrame *tf)
{
	switch (tf->ecx)
	{
	case STD_IN:
		if (dev[STD_IN].state == 1)
		{
			syscallReadStdIn(tf);
		}
		break;
	case SH_MEM:
		if (dev[SH_MEM].state == 1)
		{
			syscallReadShMem(tf);
		}
		break;
	default:
		syscallReadFile(tf);
		break;
	}
}

void syscallReadStdIn(struct TrapFrame *tf)
{
	// TODO in lab4
	if (dev[STD_IN].value < 0)
	{
		pcb[current].regs.eax = -1;
		return;
	}
	else if (dev[STD_IN].value > 0)
		return;

	/*get blocked */
	pcb[current].blocked.next = dev[STD_IN].pcb.next;
	pcb[current].blocked.prev = &(dev[STD_IN].pcb);
	dev[STD_IN].pcb.next = &(pcb[current].blocked);
	(pcb[current].blocked.next)->prev = &(pcb[current].blocked);

	dev[STD_IN].value--;

	pcb[current].state = STATE_BLOCKED;
	pcb[current].sleepTime = -1; //Never wake up until kbd awake it

	asm volatile("int $0x20");

	/*after kbdhandle, awake */
	int sel = tf->ds;
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	char character;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	while (bufferHead != bufferTail)
	{
		character = getChar(keyBuffer[bufferHead]);
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
		if (character == 0)
			continue;
		asm volatile("movb %0, %%es:(%1)" ::"r"(character), "r"(str + i));
		i++;
		putChar(character);
		//putInt(i);
		if (i == size - 1)
		{
			//buffer overflow
			putString("syscallReadStdIn:Input overflow!");
			break;
		}
	}
	character = 0;
	asm volatile("movb %0, %%es:(%1)" ::"r"(character), "r"(str + i)); //add \0 to the end
	pcb[current].regs.eax = i;
	return;
}

void syscallReadShMem(struct TrapFrame *tf)
{
	// TODO in lab4
	int sel = tf->ds;
	uint8_t *str = (uint8_t *)tf->edx;
	int size = tf->ebx;
	int index = tf->esi;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	uint8_t c;
	while (i < size)
	{
		c = shMem[i + index];
		asm volatile("movb %0, %%es:(%1)" ::"r"(c), "r"(str + i));
		i++;
		if (i + index == MAX_SHMEM_SIZE)
		{
			//shmem overflow
			putString("syscallReadShMem:ShMem overflow!");
			i = -1;
			break;
		}
	}
	pcb[current].regs.eax = i;
	return;
}

void syscallFork(struct TrapFrame *tf)
{
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++)
	{
		if (pcb[i].state == STATE_DEAD)
		{
			break;
		}
	}
	if (i != MAX_PCB_NUM)
	{
		pcb[i].state = STATE_PREPARING;

		enableInterrupt();
		for (j = 0; j < 0x100000; j++)
		{
			*(uint8_t *)(j + (i + 1) * 0x100000) = *(uint8_t *)(j + (current + 1) * 0x100000);
		}
		disableInterrupt();

		pcb[i].stackTop = (uint32_t) & (pcb[i].stackTop) -
										   ((uint32_t) & (pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t) & (pcb[i].stackTop) -
											   ((uint32_t) & (pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		pcb[i].regs.ss = USEL(2 + i * 2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1 + i * 2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2 + i * 2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/*XXX set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else
	{
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct TrapFrame *tf)
{
	int sel = tf->ds;
	char *str = (char *)tf->ecx;
	char tmp[128];
	int i = 0;
	char character = 0;
	int ret = 0;
	uint32_t entry = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	asm volatile("movb %%es:(%1), %0"
				 : "=r"(character)
				 : "r"(str + i));
	while (character != 0)
	{
		tmp[i] = character;
		i++;
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(character)
					 : "r"(str + i));
	}
	tmp[i] = 0;

	ret = loadElf(tmp, (current + 1) * 0x100000, &entry);
	if (ret == -1)
	{
		tf->eax = -1;
		return;
	}
	tf->eip = entry;
	return;
}

void syscallSleep(struct TrapFrame *tf)
{
	if (tf->ecx == 0)
	{
		return;
	}
	else
	{
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = tf->ecx;
		asm volatile("int $0x20");
		return;
	}
	return;
}

void syscallExit(struct TrapFrame *tf)
{
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct TrapFrame *tf)
{
	switch (tf->ecx)
	{
	case SEM_INIT:
		syscallSemInit(tf);
		break;
	case SEM_WAIT:
		syscallSemWait(tf);
		break;
	case SEM_POST:
		syscallSemPost(tf);
		break;
	case SEM_DESTROY:
		syscallSemDestroy(tf);
		break;
	default:
		break;
	}
}

void syscallSemInit(struct TrapFrame *tf)
{
	// TODO in lab4
	int value = (int)tf->edx;
	int i = 0;
	for (; i < MAX_SEM_NUM; i++)
	{
		if (sem[i].state == 0)
			break;
	}
	if (i == MAX_SEM_NUM)
	{
		putString("syscallSemInit:No useable sem!");
		pcb[current].regs.eax = -1;
		return;
	}
	sem[i].state = 1;
	sem[i].value = value;
	sem[i].pcb.next = &(sem[i].pcb);
	sem[i].pcb.prev = &(sem[i].pcb);
	pcb[current].regs.eax = i;
	return;
}

void syscallSemWait(struct TrapFrame *tf)
{
	// TODO in lab4
	int index = tf->edx;
	if (sem[index].state != 1)
	{
		putString("syscallSemWait:Sem Not Used!");
		pcb[current].regs.eax = -1;
		return;
	}
	pcb[current].regs.eax = 0;
	sem[index].value--;
	if (sem[index].value < 0)
	{
		//add
		pcb[current].blocked.next = sem[index].pcb.next;
		pcb[current].blocked.prev = &(sem[index].pcb);
		sem[index].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		//set
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = -1;
		asm volatile("int $0x20");
	}
	return;
}

void syscallSemPost(struct TrapFrame *tf)
{
	// TODO in lab4
	int index = tf->edx;
	if (sem[index].state != 1)
	{
		putString("syscallSemPost:Sem Not Used!");
		pcb[current].regs.eax = -1;
		return;
	}
	sem[index].value++;
	pcb[current].regs.eax = 0;
	if (!(sem[index].value > 0))
	{
		ProcessTable *pt = (ProcessTable *)((uint32_t)(sem[index].pcb.prev) - (uint32_t) & (((ProcessTable *)0)->blocked));
		sem[index].pcb.prev = (sem[index].pcb.prev)->prev;
		(sem[index].pcb.prev)->next = &(sem[index].pcb);
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;
	}
	return;
}

void syscallSemDestroy(struct TrapFrame *tf)
{
	// TODO in lab4
	int index = tf->edx;
	if (sem[index].state != 1)
	{
		putString("syscallSemDestroy:Sem Not Used!");
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[index].value != 0)
	{
		putString("DestroyWarning!");
	}
	pcb[current].regs.eax = 0;
	sem[index].state = 0;
	return;
}

void syscallGetPid(struct TrapFrame *tf)
{
	pcb[current].regs.eax = current;
	return;
}

void GProtectFaultHandle(struct TrapFrame *tf)
{
	assert(0);
	return;
}

void syscallOpen(struct TrapFrame *tf)
{
	//TODO:fcb[0] for STD_OUT, 1 for STD_IN, 2 not used, 3 for Sh_MEM
	int flag = tf->edx;
	char destFilePath[256];
	int sel = tf->ds;
	uint8_t *str = (uint8_t *)tf->ecx;
	int size = tf->ebx;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	uint8_t c;
	while (i < size)
	{
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(c)
					 : "r"(str + i));
		destFilePath[i] = c;
		i++;
	}
	destFilePath[i] = '\0';

	int ret = 0;
	SuperBlock superBlock;
	int inodeOffset = 0;
	Inode inode;
	ret = readSuperBlock(&superBlock);
	if (ret == -1)
	{
		putString("Failed to load SuperBlock.\n");
		return;
	}
	ret = readInode(&superBlock, &inode, &inodeOffset, destFilePath);
	if (ret == -1)
	{
		if ((flag & O_CREATE) != O_CREATE)
		{
			putString("Failed to Find file ");
			putString(destFilePath);
			return;
		}
		Inode fatherInode;
		int fatherInodeOffset;
		int length = stringLen(destFilePath);
		if (destFilePath[0] != '/' || destFilePath[length - 1] == '/')
		{
			putString("Incorrect destination file path.\n");
			return;
		}
		ret = stringChrR(destFilePath, '/', &size);
		if (ret == -1)
		{ // no '/' in destFilePath
			putString("Incorrect destination file path.\n");
			return;
		}
		char tmp = *((char *)destFilePath + size + 1);
		*((char *)destFilePath + size + 1) = 0; // destFilePath is dir ended with '/'.
		ret = readInode(&superBlock, &fatherInode, &fatherInodeOffset, destFilePath);
		*((char *)destFilePath + size + 1) = tmp;
		if (ret == -1)
		{
			putString("Failed to read father inode.\n");
			return;
		}
		int destFileType = ((flag & O_DIRECTORY) == O_DIRECTORY) ? DIRECTORY_TYPE : REGULAR_TYPE;
		allocInode(&superBlock, &fatherInode, fatherInodeOffset, &inode, &inodeOffset, destFilePath + size + 1, destFileType);
		if ((flag & O_DIRECTORY) == O_DIRECTORY)
		{
			mkdir(destFilePath);
		}
	}

	int firstFree = -1;
	int index = -1;
	for (int s = 4; s < MAX_FILE_NUM; s++)
	{
		if (fcb[s].state == 1)
		{
			if (fcb[s].inodeOffset == inodeOffset)
			{
				index = s;
				break;
			}
		}
		else
		{
			if (firstFree == -1)
				firstFree = s;
		}
	}
	if (index != -1)
	{
		//find
		pcb[current].regs.eax = index;
	}
	else
	{
		//if ((flag & O_CREATE) != O_CREATE)
		//{

		//}
		if (firstFree == -1)
		{
			putString("File overflow!");
			return;
		}
		pcb[current].regs.eax = firstFree;
		index = firstFree;
	}
	fcb[index].flags = flag;
	fcb[index].state = 1;
	fcb[index].inodeOffset = inodeOffset;
	fcb[index].offset = 0;
	fcb[index].length = inode.size;
	return;
}

void syscallClose(struct TrapFrame *tf)
{
	int fd = tf->ecx;
	fcb[fd].state = 0;
	return;
}

void syscallLseek(struct TrapFrame *tf)
{
	int fd = tf->ecx;
	int offset = tf->edx;
	int whence = tf->ebx;
	int flag = fcb[fd].flags;
	if ((flag & O_WRITE) == O_WRITE || (flag & O_READ) == O_READ)
	{
		if (whence == SEEK_SET)
			fcb[fd].offset = offset;
		else if (whence == SEEK_CUR)
			fcb[fd].offset += offset;
		else if (whence == SEEK_END)
			fcb[fd].offset = fcb[fd].length + offset;
		else
		{
			putString("Wrong seek parameter!");
			return;
		}
		if (fcb[fd].offset > fcb[fd].length || fcb[fd].offset < 0)
		{
			pcb[current].regs.eax = -1;
			putString("Seek a wrong offset!");
			return;
		}
		pcb[current].regs.eax = fcb[fd].offset;
	}
	else
	{
		pcb[current].regs.eax = -1;
		putString("Lseek not permitted!");
	}
	return;
}

void syscallRemove(struct TrapFrame *tf)
{
	char destFilePath[256];
	int sel = tf->ds;
	uint8_t *str = (uint8_t *)tf->ecx;
	int size = tf->edx;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	uint8_t c;
	while (i < size)
	{
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(c)
					 : "r"(str + i));
		destFilePath[i] = c;
		i++;
	}
	destFilePath[i] = '\0';
	int ret = rm(destFilePath);
	pcb[current].regs.eax = ret;
	if (ret == -1)
	{
		putString("Failed to remove file!");
	}
	return;
}

void syscallReadFile(struct TrapFrame *tf)
{
	int fd = tf->ecx;
	if ((fcb[fd].flags & O_READ) != O_READ)
	{
		putString("Read Permission Denied!");
		pcb[current].regs.eax = -1;
		return;
	}
	int sel = tf->ds;
	uint8_t *str = (uint8_t *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	uint8_t c;

	SuperBlock superblock;
	readSuperBlock(&superblock);
	Inode inode;
	int offset = fcb[fd].offset;
	int inodeOffset = fcb[fd].inodeOffset;
	uint8_t buffer[superblock.blockSize];

	int blockIndex = offset / superblock.blockSize;
	diskRead((void *)(&inode), sizeof(Inode), 1, inodeOffset);
	readBlock(&superblock, &inode, blockIndex, buffer);

	int insideoffset = offset % superblock.blockSize;
	while (i < size)
	{
		if (insideoffset == superblock.blockSize) //read a new block
		{
			blockIndex++;
			int ret = readBlock(&superblock, &inode, blockIndex, buffer);
			if (ret == -1)
				break;
			insideoffset = 0;
			continue;
		}
		c = buffer[insideoffset];
		asm volatile("movb %0, %%es:(%1)" ::"r"(c), "r"(str + i));
		i++;
		insideoffset++;
	}
	pcb[current].regs.eax = i;
	fcb[fd].offset += i;
	return;
}

void syscallWriteFile(struct TrapFrame *tf)
{
	int fd = tf->ecx;
	if ((fcb[fd].flags & O_WRITE) != O_WRITE)
	{
		putString("Write Permission Denied!");
		pcb[current].regs.eax = -1;
		return;
	}
	int sel = tf->ds;
	uint8_t *str = (uint8_t *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	uint8_t c = '1';

	SuperBlock superblock;
	readSuperBlock(&superblock);
	Inode inode;
	int offset = fcb[fd].offset;
	int inodeOffset = fcb[fd].inodeOffset;
	uint8_t buffer[superblock.blockSize];

	int blockIndex = offset / superblock.blockSize;
	diskRead((void *)(&inode), sizeof(Inode), 1, inodeOffset);
	while (blockIndex + 1 > inode.blockCount)
	{
		int ret = allocBlock(&superblock, &inode, inodeOffset);
		if (ret == -1)
		{
			putString("Failed to alloc block while writing file!");
			pcb[current].regs.eax = -1;
			return;
		}
	}
	readBlock(&superblock, &inode, blockIndex, buffer);

	int insideoffset = offset % superblock.blockSize;
	while (i < size)
	{
		if (insideoffset == superblock.blockSize)
		{
			writeBlock(&superblock, &inode, blockIndex, buffer);
			blockIndex++;
			while (blockIndex + 1 > inode.blockCount)
			{
				int ret = allocBlock(&superblock, &inode, inodeOffset);
				if (ret == -1)
				{
					putString("Failed to alloc block while writing file!");
					pcb[current].regs.eax = -1;
					return;
				}
			}
			readBlock(&superblock, &inode, blockIndex, buffer);
			insideoffset = 0;
			continue;
		}
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(c)
					 : "r"(str + i));
		buffer[insideoffset] = c;
		i++;
		insideoffset++;
	}
	writeBlock(&superblock, &inode, blockIndex, buffer);
	pcb[current].regs.eax = i;
	fcb[fd].offset += i;
	fcb[fd].length += i;
	inode.size += i;
	diskWrite((void *)&inode, sizeof(Inode), 1, inodeOffset);
	return;
}

void syscallGetFileSize(struct TrapFrame *tf)
{
	int fd = tf->ecx;
	pcb[current].regs.eax = fcb[fd].length;
}
