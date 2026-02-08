#include <linux/kvm.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>

/* Change depending on stuff your running. */
#define RAM_SIZE 0x20000000 /* 4KB RAM default. */
#define GUEST_BINARY "imgs/vmlinuz"
#define GUEST_INITRD "imgs/initramfs.img"
#define BOOT_PARAM_ADDR 0x10000
#define CMDLINE_ADDR    0x20000
#define KERNEL_ADDR     0x100000
#define INITRD_ADDR     0x08000000
#define CMDLINE "console=ttyS0 earlyprintk=serial root=/dev/ram0"

struct termios orig_termios;

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

int main() {
		int kvm_fd, vm_fd, vcpu_fd, run_size;
		struct kvm_run *run;
		struct kvm_sregs sregs;
		struct kvm_regs regs;
		struct kvm_userspace_memory_region region = {0};
		struct stat st;

		/* Initialize KVM */
		kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
		vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);

		ioctl(vm_fd, KVM_SET_TSS_ADDR, 0xfffbd000);
		__u64 map_addr = 0xfffbc000;
		ioctl(vm_fd, KVM_SET_IDENTITY_MAP_ADDR, &map_addr);
		ioctl(vm_fd, KVM_CREATE_IRQCHIP, 0);
		struct kvm_pit_config pit = {0};
		ioctl(vm_fd, KVM_CREATE_PIT2, &pit);

		/* Setup guest memory */
		void *mem = mmap(NULL, RAM_SIZE, PROT_READ | PROT_WRITE,
						MAP_SHARED | MAP_ANONYMOUS, -1, 0);

		/* Load guest binary into memory. */
		int kfd = open(GUEST_BINARY, O_RDONLY);
		if (kfd >= 0) {
			fstat(kfd, &st);
			read(kfd, mem + KERNEL_ADDR, st.st_size);
			close(kfd);
		}

		int ifd = open(GUEST_INITRD, O_RDONLY);
		if (ifd >= 0) {
			fstat(ifd, &st);
			read(ifd, mem + INITRD_ADDR, st.st_size);
			close(ifd);
		}

		unsigned char *bp = (unsigned char *)mem + BOOT_PARAM_ADDR;
		memset(bp, 0, 4096);
		bp[0x210] = 0xFF;              
		bp[0x211] = 0x80;              
		*(unsigned int *)(bp + 0x228) = CMDLINE_ADDR;
		*(unsigned int *)(bp + 0x218) = INITRD_ADDR;
		*(unsigned int *)(bp + 0x21c) = (unsigned int)st.st_size;
		strcpy((char *)mem + CMDLINE_ADDR, CMDLINE);

		region.slot = 0;
		region.guest_phys_addr = 0;
		region.memory_size = RAM_SIZE;
		region.userspace_addr = (unsigned long)mem;
		ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region);

		/* Setup VCPU */
		vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
		run_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
		run = mmap(NULL, run_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);

		/* Setup registers */
		ioctl(vcpu_fd, KVM_GET_SREGS, &sregs);
		sregs.cs.base = sregs.ds.base = sregs.es.base = sregs.fs.base = sregs.gs.base = sregs.ss.base = KERNEL_ADDR;
		sregs.cs.selector = sregs.ds.selector = sregs.es.selector = sregs.fs.selector = sregs.gs.selector = sregs.ss.selector = KERNEL_ADDR >> 4;
		ioctl(vcpu_fd, KVM_SET_SREGS, &sregs);

		memset(&regs, 0, sizeof(regs));
		regs.rip = 0x200;
		regs.rsi = BOOT_PARAM_ADDR;
		regs.rflags = 0x2;
		ioctl(vcpu_fd, KVM_SET_REGS, &regs);

		tcgetattr(STDIN_FILENO, &orig_termios);
		atexit(restore_terminal);
		struct termios raw = orig_termios;
		raw.c_lflag &= ~(ECHO | ICANON);
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

		/* Execution loop. */
		printf("[OK] tvrt is starting...\n");
		while (1) {
				ioctl(vcpu_fd, KVM_RUN, 0);
				switch (run->exit_reason) {
						case KVM_EXIT_IO:	
								/* Prints anything that the guest writes to port 0xE9 */
								if (run->io.direction == KVM_EXIT_IO_OUT && (run->io.port == 0xE9 || run->io.port == 0x3f8)) {
										putchar(*(((char *)run) + run->io.data_offset));
										fflush(stdout);
								} else if (run->io.direction == KVM_EXIT_IO_IN && run->io.port == 0x3f8) {
										char c = 0;
										if (read(STDIN_FILENO, &c, 1) > 0)
											*(((char *)run) + run->io.data_offset) = c;
								}
								break;
						case KVM_EXIT_HLT:
								puts("\n[HT] Guest halted.");
								return 0;
						case KVM_EXIT_FAIL_ENTRY:
								return 1;
						default:
								break;

				}
		}
}
