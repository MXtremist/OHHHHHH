/* Real Mode Hello World */
.code16
start:
	cli #关闭中断
    inb $0x92, %al #启动A20总线
    orb $0x02, %al
    outb %al, $0x92
    data32 addr32 lgdt gdtDesc #加载GDTR
    movl %cr0, %eax #启动保护模式
    orb $0x01, %al
    movl %eax, %cr0 #设置CR0的PE位(第0位)为1
    data32 ljmp $0x08, $start32 #跳转切换至保护模式

.code32
start32: 
    movw    $(2 << 3), %ax
	movw    %ax, %ds          # %DS = %AX
	movw    %ax, %es          # %ES = %AX
	movw    %ax, %ss          # %SS = %AX

    movw    $(3 << 3), %ax
	movw    %ax, %gs          # %GS = %AX

    movl $(128 << 20), %esp
	sub $16, %esp  
    #init DS ES FS GS SS ESP        
    jmp bootMain      
           
gdt:        
    .word 0,0 
    .byte 0,0,0,0

    .word 0xffff,0       
    .byte 0,0x9a,0xcf,0   
             
    .word 0xffff,0                  
    .byte 0,0x92,0xcf,0  
         
    .word 0xffff,0x8000            
    .byte 0x0b,0x92,0xcf,0     

gdtDesc:        
    .word (gdtDesc - gdt -1)        
    .long gdt


