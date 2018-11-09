#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

#ifdef __i386__
#define __NR_LED 383
#else
#define __NR_LED 333
#endif

#define BUFFER_LENGTH 30
#define isDigit(c)	('0' <= (c) && (c) <= '9')	/* Unsigned int € [0, 4294967295] */

long ledctl(unsigned int num);
int isInt(char *cad);
void print_error();

int main(int argc, char **argv) {
	//printf("Hello\n");

	unsigned int num, len;		/* Num = number, len = length of int in chars */
	char buf2[BUFFER_LENGTH];	/* Auxiliar buffer */
	char *buf;					/* We use it to save the number */
	char **trash = NULL;		/* For saving the rest of the char in STRTOUL function*/
	
	/* Error Comprobation: check number of args */
	if(argc !=2) {
		fprintf(stderr,"El nº de argumentos es incorrecto.\n");
		return -EXIT_FAILURE;	
	}

	
	buf=argv[1];
	printf("Buffer = %s\n", buf);

	/* Hay que borrar 0x */
	if(strlen(argv[1])>2) {
		if( argv[1][0] == '0'  && ( (argv[1][1] == 'x') || (argv[1][1] == 'X') ) ) {
			len = strlen(argv[1])-2 ;
			memcpy(buf2, &argv[1][2], len);
			buf2[len] = '\0';
			buf = buf2;
		}
	}
	printf("Buffer sin '0x' = %s\n", buf);
		/* Error Comprobation -> correct argument */
	if(!isInt(buf)) {
		fprintf(stderr,"El arguemnto es incorrecto.\n");
		//print_error();
		return -EXIT_FAILURE;
	}
		/* Converts argument to unsigned integer : USE STRTOUL FUNCTION*/
	num = strtoul(buf, trash, 10);	/* Save in num the numbers of buf in base 10 */
	printf("Buffer en unsigned integer = %d\n", num);

	if(ledctl(num) == -1) {
		perror("Se ha producido un error");
		print_error();
		return -EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}

/* Calls the syscall 333(ledctl) */
long ledctl(unsigned int num) {		////////* ESTA FALLANDO AQUI *//////// 
	printf("Mask = %d", num);
	return (long)syscall(__NR_LED, num);	// 0 OK; -1 ERR
}						

/* Returns a predeterminate error msg */
void print_error() {
	printf("Valid argument:\n");
	printf("\t./ledctl 0-7\n");
	printf("\t./ledctl -h\n");
}

/* Returns isDigit(cad) */
int isInt(char *cad) {
	int i;
	for(i = 0; i < strlen(cad); ++i){
		if(!isDigit(cad[i])) return 0;
	}
	return 1;	
}




























