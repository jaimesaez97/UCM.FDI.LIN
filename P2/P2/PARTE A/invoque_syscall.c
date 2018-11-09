#include <linux/errno.h>
#include <sys/syscall.h>
#define _GNU_SOURCE
#include <linux/unistd.h>
#include <linux/ftrace.h>

#ifdef __i386__
	#define __NR_HELLO	383
#else
	#define __NR_HELLO	332
#endif

long lin_hello(void){
	return (long)syscall(__NR_HELLO);
}

int main(void){
	return lin_hello();
}
