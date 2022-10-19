## Analysis of oops message
Error message specifies that NULL pointer deference caused the fault. The address (0000000000000000) is also a clue.

	Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
	
The below lines specify the state of register at the time of fault

	Mem abort info:
	  ESR = 0x96000045
	  EC = 0x25: DABT (current EL), IL = 32 bits
	  SET = 0, FnV = 0
	  EA = 0, S1PTW = 0
	  FSC = 0x05: level 1 translation fault
	Data abort info:
	  ISV = 0, ISS = 0x00000045
	  CM = 0, WnR = 1
	user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042085000
	[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000

The error that occurred Oops with the error code 96000045. The [#1] specifies that the error occurred once.

	Internal error: Oops: 96000045 [#1] SMP
	Modules linked in: hello(O) scull(O) faulty(O)
	
Shows CPU number, the process ID where the error occured as well as the command that triggered the error. Tainted G specifies that the module was proprietory (probably because of GPL license specified in .bb file) 

	CPU: 0 PID: 158 Comm: sh Tainted: G           O      5.15.18 #1
	Hardware name: linux,dummy-virt (DT)
	pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
	
Shows the location of the program counter. It was at location 0x14 (relative to start of the function). The [faulty] specifies the module that caused the fault.

	pc : faulty_write+0x14/0x20 [faulty]
	
Link register is used to store the return value. vfs_write could be the function that called this function. 

	lr : vfs_write+0xa8/0x2b0
	
The stack pointer was pointing to a kernel space address. The stack is specified below. Addresses below 0xc0000000 are from user space, hence the recurring address 0000005580292a70 could point to the user space buffer passed in.

	sp : ffffffc008d23d80
	x29: ffffffc008d23d80 x28: ffffff80020d8000 x27: 0000000000000000
	x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
	x23: 0000000040001000 x22: 0000000000000012 x21: 0000005580292a70
	x20: 0000005580292a70 x19: ffffff8002087000 x18: 0000000000000000
	x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
	x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
	x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
	x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
	x5 : 0000000000000001 x4 : ffffffc0006f0000 x3 : ffffffc008d23df0
	x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000

The call trace, beggining from the call by faulty_write at 0x14 which caused the issue is detailed below. Can be useful for finding the rootcause of the error 

	Call trace:
	 faulty_write+0x14/0x20 [faulty]
	 ksys_write+0x68/0x100
	 __arm64_sys_write+0x20/0x30
	 invoke_syscall+0x54/0x130
	 el0_svc_common.constprop.0+0x44/0xf0
	 do_el0_svc+0x40/0xa0
	 el0_svc+0x20/0x60
	 el0t_64_sync_handler+0xe8/0xf0
	 el0t_64_sync+0x1a0/0x1a4
	Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
	---[ end trace 3d7ddda4c9111df2 ]---
	
	
By looking at the dissassembly of the faulty.ko module using objdump, we can find the dissasembly of the faulty_write function:

	faulty.ko:     file format elf64-littleaarch64
	Disassembly of section .text:
	0000000000000000 <faulty_write>:
	   0:	d503245f 	bti	c
	   4:	d2800001 	mov	x1, #0x0                   	// #0
	   8:	d2800000 	mov	x0, #0x0                   	// #0
	   c:	d503233f 	paciasp
	  10:	d50323bf 	autiasp
	  14:	b900003f 	str	wzr, [x1]
	  18:	d65f03c0 	ret
  	1c:	d503201f 	nop
  	
We can see that at the address 0x14, a store instruction is executed which stores the value in register wzr to the location pointed by [x1] which is #0x0 which does not map to a valid address in physical space and hence caused the fault.
