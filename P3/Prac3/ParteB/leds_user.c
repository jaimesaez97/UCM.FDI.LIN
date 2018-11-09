#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	/* For sleep function */
#include <fcntl.h>	/* For O_RDWR macro */
#include <errno.h>


#define BUFFER_SIZE 		512		/* tam = 20640 */
#define MAX_TAM_CPUINFO 	512*32

#define ALL_LEDS_OFF		000000

#define NR_LEDS				8
#define NR_BYTES_COLOR		6	
#define BUFFER_LENGTH		(NR_LEDS)*(NR_BYTES_COLOR + 3) + 1 

#define BLINKSTICK_PATH 	"/dev/usb/blinkstick0"	
#define CPUINFO_PATH 		"/proc/cpuinfo"

#define TURN_LEDS_OFF 		""		
#define LEDS_PATH			"/home/kernel/Prac3/ParteB/leds.txt"
#define COLORS_PATH			"/home/kernel/Prac3/ParteB/colors.txt"
#define RED_PATH			"/home/kernel/Prac3/ParteB/reds.txt"
#define BLUE_PATH			"/home/kernel/Prac3/ParteB/blues.txt"
#define GREEN_PATH			"/home/kernel/Prac3/ParteB/greens.txt"
#define MAX_COLOR		 	250		/* Number of hex-sequence in "colors.txt" */
#define MAX_LEDS  			250		/* Number of integer in "leds.txt" */
#define MAX_REDS			256		/* Red components fills 2 BYTE -> 2^8 s*/

static int set_all_leds_color(char *s, int fdBlinkstick){
	char buf[BUFFER_LENGTH * 10];		/* To turn all leds at the same time -> NO LATENCY */
	sprintf(buf, "0:%s,1:%s,2:%s,3:%s,4:%s,5:%s,6:%s,7:%s", s, s, s, s, s, s, s, s);
	write(fdBlinkstick, buf, strlen(buf));
	return 0;
}

static int sec_turn_off(int opt){		// opt -> 0 = red, 1 = blue, 2 = green

	int ret = 0;

	int i;
	int fdBlinkstick;
	FILE *fColor;
	char *act_color;
		/* Files opening */
	switch(opt){
		case 0: fColor = fopen(RED_PATH, "r"); break;
		case 1: fColor = fopen(BLUE_PATH, "r"); break;
		case 2: fColor = fopen(GREEN_PATH, "r"); break;
	}
	if((fdBlinkstick = open(BLINKSTICK_PATH, O_WRONLY)) < 0){
		ret = -EFAULT;
		goto out_err;
	}
		/* Reserving space for color */
	act_color = malloc(NR_BYTES_COLOR + 1);
	for(i = 0; i < MAX_REDS; ++i){
		if(fscanf(fColor, "%s", act_color) < 0){
			ret = -EIO;
			goto out_err;
		}
		if(set_all_leds_color(act_color, fdBlinkstick) < 0){
			ret = -EIO;
			goto out_err;
		}
	}
	close(fdBlinkstick);
	fclose(fColor);
	free(act_color);
	return ret;
out_err:
	if(ret == -EIO)
		free(act_color);

	close(fdBlinkstick);
	fclose(fColor);
	return ret;
}

static int leds_locos(){
	int ret = 0;	// RET VALUE

		/* Variable Declaration */
    int fdBlinkstick;						/* /dev/usb/blinkstick0 file descriptor */
    FILE *fLeds;							/* leds.txt file struct */
    FILE *fColors;							/* colors.txt file descriptor */
    int i;									/* counter (< NUM_LEDS) */
    char *act_color;						/* buffer to print the actual color in blinkstick */
    int act_led;							/* buffer to print to the actual led in blinkstick */
	char buf[BUFFER_LENGTH];				/* buffer to print to the blinkstick */

    	/* Files opening */
    fLeds = fopen(LEDS_PATH, "r");
    fColors = fopen(COLORS_PATH, "r");
    if((fdBlinkstick = open(BLINKSTICK_PATH, O_WRONLY)) < 0){
    	ret = -EFAULT;
    	goto out_err;
    }

    act_color = malloc(NR_BYTES_COLOR + 1);
    	/* Algorithm */
    for(i = 0; i < MAX_LEDS; i++){
    		/* Read leds.txt untill next integer */
    	if(fscanf(fLeds, "%i", &act_led) < 0){
    		ret = -EIO;
    		goto out_err;
    	}
    		/* Read 6-byte from colors file to color buffer */
    	if(fscanf(fColors, "%s", act_color) < 0){
    		ret = -EIO;
    		goto out_err;
    	}
    	sprintf(buf, "%d:%s", act_led, act_color);
		write(fdBlinkstick, buf, strlen(buf));
    }
	write(fdBlinkstick, "", 0);
    free(act_color);
    close(fdBlinkstick);
    fclose(fColors);
    fclose(fLeds);
    return ret;		/* CORRECT return value */

out_err:
	if(ret == -EIO)
		free(act_color);

	close(fdBlinkstick);
    fclose(fColors);
    fclose(fLeds);
    return ret;		/* CORRECT return value */
}

#define b_separator			"======================================\n======================================\n"	/* to print_friendly */
#define l_separator			"--------------------------------------\n"											/* to print_friendly */

static int print_menu(){
	char *buf;						/* To print_friendly */
	int option;						/* Option choosed */

	write(1, l_separator, strlen(l_separator));
	buf = "Option 1: 		EXIT\n";
	write(1, buf, strlen(buf));
	write(1, l_separator, strlen(l_separator));
	buf = "Option 2: 		Crazy Leds\n";
	write(1, buf, strlen(buf));
	write(1, l_separator, strlen(l_separator));
	buf = "Option 3: 		Red Power Off\n";
	write(1, buf, strlen(buf));
	write(1, l_separator, strlen(l_separator));
	buf = "Option 4: 		Blue Power Off\n";
	write(1, buf, strlen(buf));
	write(1, l_separator, strlen(l_separator));
	buf = "Option 5: 		Green Power Off\n";
	write(1, buf, strlen(buf));
	write(1, l_separator, strlen(l_separator));
	buf = "Option 6: 		ALL IN ONE\n";
	write(1, buf, strlen(buf));
	write(1, l_separator, strlen(l_separator));
	buf = "Choose an option (2..6) or 1 to EXIT:...\n\n";
	write(1, buf, strlen(buf));
	scanf("%d", &option);

	return option;
}

int main(int argc, char **argv) {
	int opt;

	while((opt = print_menu()) > 0){
		switch(opt){
			case 1: return 0;
			case 2: leds_locos(); break;
			case 3: sec_turn_off(0); break;
			case 4: sec_turn_off(1); break;
			case 5: sec_turn_off(2); break;
			case 6: leds_locos(); sec_turn_off(0); sec_turn_off(1); sec_turn_off(2); break;
			default: printf("Not a Correct Value [1..5]\n");
		}
	}
	

	return 0;
}
