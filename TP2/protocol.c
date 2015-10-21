#include "utils.h"
#include "linklayer.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

static struct termios oldtio;
static int tries;

void sig_alarm_handler() {
    ++tries;
}

static int read_valid_string(int fd, char *buf, int (*validator)(char *, int)) {
    int length;
    tries = 0;
    signal(SIGALRM, sig_alarm_handler);
    while (tries < 3) {
		alarm(3);
		while (1) {
            length = serial_read_string(fd,buf);
			if (length == -1)
				break;
			printf("Validating string (size %d): \n", length);
            int i;
            for (i = 0; i < length; ++i)
                printf("0x%x ", buf[i]);
            printf("\n");
            printf("is_valid: %d", is_valid_string(buf, length));
            printf("validator: %d", validator(buf, length));

			if(is_valid_string(buf,length) && validator(buf, length)) {
                printf("Valid string\n"); 
                alarm(0);
				return length;
	   		}
        printf("Non valid string\n"); 
        }
    }
    
    return -1;
}

int llopen(int port, int flag){

    struct termios newtio;

    char serial_name[strlen(MODEMDEVICE) + 2 + 1];
    sprintf(serial_name, "%s%d", MODEMDEVICE, port);

    int fd = open(serial_name, O_RDWR | O_NOCTTY );
    if (fd <0) {perror(serial_name); return -1; }

	/* Open the serial port for sending the message */
    printf("Configuring port\n");

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      return -1;
    }

    bzero(&newtio, sizeof(newtio));

    /* 	Control, input, output flags */
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD; /*  */
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = OPOST;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */


    tcflush(fd, TCIFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
    printf("Port configured\n");

	if(flag == TRANSMITTER)
		return llopen_transmitter(fd);
	else if (flag == RECEIVER)
		return llopen_receiver(fd);
	else
		return -1;
}

int ua_validator (char* buffer, int length){
    return (length == 3) && (buffer[C_FLAG_INDEX] == SERIAL_C_UA);
}

int set_validator (char* buffer, int length){
    return (length == 3) && (buffer[C_FLAG_INDEX] == SERIAL_C_SET);
}

int llopen_transmitter(int fd){
	char ua[MAX_STRING_SIZE];
	char buffer[] = {SERIAL_FLAG,
			SERIAL_A_COM_TRANSMITTER,
			SERIAL_C_SET,
			SERIAL_A_COM_TRANSMITTER^SERIAL_C_SET,
			SERIAL_FLAG};


	printf("Transmitter open sequence\n");
	printf("Sending SET\n");
	write(fd,buffer,5);

	printf("Reading from fd\n");
	if (read_valid_string(fd, buffer, ua_validator) == -1)
        return -1;
    return fd;
}

int llopen_receiver(int fd){
	char set[MAX_STRING_SIZE];
	printf("Receiver open sequence\n");
	printf("Reading from port\n");

    char ua[] = {SERIAL_FLAG,
		    	SERIAL_A_ANS_RECEIVER,
			    SERIAL_C_UA,
			    SERIAL_A_ANS_RECEIVER^SERIAL_C_UA,
			    SERIAL_FLAG};

   
    if(read_valid_string(fd,set,set_validator) == -1)
        return -1;

    write(fd, ua, 5);
    return fd;
}

int llclose(int fd){
	printf("Restoring port configurations\n");
	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
	      perror("tcsetattr");
	      exit(-1);
    }
    close(fd);
}