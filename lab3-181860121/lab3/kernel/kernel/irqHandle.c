#include "x86.h"
#include "device.h"

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallFork(struct TrapFrame *tf);
void syscallExec(struct TrapFrame *tf);
void syscallSleep(struct TrapFrame *tf);
void syscallExit(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void timerHandle(struct TrapFrame *tf);
void keyboardHandle(struct TrapFrame *tf);

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
	case 0:
		syscallWrite(tf);
		break; // for SYS_WRITE
	case 1:
		syscallFork(tf);
		break; // for SYS_FORK
	case 2:
		syscallExec(tf);
		break; // for SYS_EXEC
	case 3:
		syscallSleep(tf);
		break; // for SYS_SLEEP
	case 4:
		syscallExit(tf);
		break; // for SYS_EXIT
	default:
		break;
	}
}

void ChangeProcess()
{
	/* time all used, or dead, or blocked*/
	int i = (current + 1) % MAX_PCB_NUM;
	for (; i != current; i = (i + 1) % MAX_PCB_NUM)
	{
		if (i == 0) //IDLE
			continue;
		if (pcb[i].state == STATE_RUNNABLE)
		{
			break;
		}
	}
	if (i == current && (pcb[i].state == STATE_BLOCKED || pcb[i].state == STATE_DEAD)) //No Runnable Program, goto IDLE
		i = 0;
	current = i;
	//putChar('0' + current);
	pcb[current].state = STATE_RUNNING;
	pcb[current].timeCount = 0;
	/*change stacktop */
	uint32_t tmpStackTop = pcb[current].stackTop;
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

void timerHandle(struct TrapFrame *tf)
{
	// TODO in lab3
	for (int i = 0; i < MAX_TIME_COUNT; i++)
	{
		if (pcb[i].state == STATE_BLOCKED)
		{
			pcb[i].sleepTime--;
			if (pcb[i].sleepTime == 0) //wake
			{
				pcb[i].state = STATE_RUNNABLE;
			}
		}
	}
	if (pcb[current].state == STATE_RUNNING && pcb[current].timeCount < MAX_TIME_COUNT)
	{
		pcb[current].timeCount++;
		if (pcb[current].timeCount == MAX_TIME_COUNT)
		{
			/* time all used, maybe need to change state? */
			pcb[current].state = STATE_RUNNABLE;
			ChangeProcess();
		}
	}
	else
	{
		ChangeProcess();
	}
	return;
}

void keyboardHandle(struct TrapFrame *tf)
{
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0)
		return;
	putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
	return;
}

void syscallWrite(struct TrapFrame *tf)
{
	switch (tf->ecx)
	{ // file descriptor
	case 0:
		syscallPrint(tf);
		break; // for STD_OUT
	default:
		break;
	}
}

void syscallPrint(struct TrapFrame *tf)
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

void syscallFork(struct TrapFrame *tf)
{
	// TODO in lab3
	//find a dead pdb
	int i = (current + 1) % MAX_PCB_NUM;
	for (; i != current; i = (i + 1) % MAX_PCB_NUM)
	{
		if (pcb[i].state == STATE_DEAD)
		{
			break;
		}
	}
	if (i == current) //failed
	{
		pcb[current].regs.eax = -1;
		//putChar('1');
	}
	else
	{
		/*copy the whole memory */
		enableInterrupt();
		for (int j = 0; j < 0x100000; j++)
		{
			*(uint8_t *)(j + (i + 1) * 0x100000) = *(uint8_t *)(j + (current + 1) * 0x100000);
			asm volatile("int $0x20");
		}
		disableInterrupt();
		//putChar('0' + i);s
		pcb[i].stackTop = (uint32_t) & (pcb[i].regs);
		pcb[i].prevStackTop = (uint32_t) & (pcb[i].stackTop);
		/*set something, including seg regs */
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		/*same reg */
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;

		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/*not same regs*/
		pcb[i].regs.cs = USEL(1 + i * 2);
		pcb[i].regs.ss = USEL(2 + i * 2);
		pcb[i].regs.ds = USEL(2 + i * 2);

		/*set return val */
		pcb[current].regs.eax = i;
		pcb[i].regs.eax = 0;
	}

	return;
}

void syscallExec(struct TrapFrame *tf)
{
	// TODO in lab3
	/*get filename in tmp */
	int sel = tf->ds; //TODO segment selector for user data, need further modification
	char *str = (char *)tf->ecx;
	int size = tf->edx;
	int i = 0;
	char tmp[256]; //filename no more than 256 bits
	char character = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0"
					 : "=r"(character)
					 : "r"(str + i));
		tmp[i] = character;
	}
	tmp[i] = '\0';
	uint32_t entry;
	/*loadElf, get entry and ret */
	/*
	for (int j = 0; tmp[j] != 0; j++)
	{
		putChar(tmp[j]);
	}
	putChar('\n');
	*/
	int ret = loadElf(tmp, (current + 1) * 0x100000, &entry);
	if (ret == -1) //failed
	{
		pcb[current].regs.eax = -1;
	}
	else
	{
		/* set eip */
		pcb[current].regs.eip = entry;
	}

	return;
}

void syscallSleep(struct TrapFrame *tf)
{
	// TODO in lab3
	if (tf->ecx <= 0)
		return; //illegal
	pcb[current].state = STATE_BLOCKED;
	//pcb[current].timeCount = MAX_TIME_COUNT;
	pcb[current].sleepTime = tf->ecx;
	asm volatile("int $0x20");
	return;
}

void syscallExit(struct TrapFrame *tf)
{
	// TODO in lab3
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void GProtectFaultHandle(struct TrapFrame *tf)
{
	assert(0);
	return;
}
