#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <stdint.h>
#include <sys/time.h>

/** An accurate timestamp can be obtained with an ioctl(2) call after reading
  * a message from the socket:
  *
  *  struct timeval tv;
  *  ioctl(s, SIOCGSTAMP, &tv);
  *
  * The timestamp has a resolution of one microsecond and is set automatically
  * at the reception of a CAN frame.
  */
void can_receive_timestamp(int socket)
{
	struct timeval tv;
	ioctl(socket, SIOCGSTAMP, &tv);
	printf("%0d:%03d:%03d s:ms:us\n", tv.tv_sec, tv.tv_usec/1000, tv.tv_usec%1000);
}

int can_receive(char *ifn)
{
	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	struct ifreq ifr;

	const char *ifname = ifn;
	struct timeval start, end;

	if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Error in socket bind");
		return -2;
	}
	int i,j;
	printf("warmup...\n");
	for (j=0; j < 10; ++j) {
		for (i=0; i < 1000; ++i) {
			nbytes = read(s, &frame, sizeof(struct can_frame));
			if (nbytes != sizeof(struct can_frame)) {
				printf("ERROR: can read() error. read() returned %d\n", nbytes);
			}
			//can_receive_timestamp(s);
			//printf("id:%d, len:%d, data: 0x%x:0x%x\n", frame.can_id,
			//		frame.can_dlc,
			//		((uint32_t*)&frame.data)[0],
			//		((uint32_t*)&frame.data)[1]);
		}
	}

	printf("measure...\n");
	int sec=0, usec=0;
	for (j=0; j < 10; ++j) {
		gettimeofday(&start, NULL);
		for (i=0; i < 1000; ++i) {
			nbytes = read(s, &frame, sizeof(struct can_frame));
		}
		gettimeofday(&end, NULL);
		sec += end.tv_sec-start.tv_sec;
		usec += end.tv_usec-start.tv_usec;
		printf("%d frames in %0d:%03d:%03d s:ms:us\n", i, end.tv_sec-start.tv_sec,
						(end.tv_usec-start.tv_usec)/1000,
						(end.tv_usec-start.tv_usec)%1000);
	}
	usec=((usec + sec*1000*1000)/j) % (1000*1000);
	sec /= j;

	printf("average: %0d:%03d:%03d s:ms:us\n", sec, usec/1000, usec%1000);
	usec=((usec + sec*1000*1000)/i) % (1000*1000);
	sec /= i;
	printf("average per read: %0d:%03d:%03d s:ms:us\n", sec, usec/1000, usec%1000);
	return 0;
}



int can_send(char *ifn)
{
	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	struct ifreq ifr;

	const char *ifname = ifn;
	uint32_t high = 0, low = 0;
	struct timeval start, end;

	if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Error in socket bind");
		return -2;
	}

	frame.can_id  = 0x1;
	frame.can_dlc = 8;

	int i,j;
	printf("warmup...\n");
	for (j=0; j < 10; ++j) {
		for (i=0; i < 1000; ++i) {
			((uint32_t*)frame.data)[0] = high++;
			((uint32_t*)frame.data)[1] = low++;

			nbytes = write(s, &frame, sizeof(struct can_frame));
			usleep(250);
		}
	}

	printf("measure...\n");
	int sec=0, usec=0;
	for (j=0; j < 10; ++j) {
		gettimeofday(&start, NULL);
		for (i=0; i < 1000; ++i) {
			((uint32_t*)frame.data)[0] = high++;
			((uint32_t*)frame.data)[1] = low++;

			nbytes = write(s, &frame, sizeof(struct can_frame));
			usleep(250);
		}
		gettimeofday(&end, NULL);
		sec += end.tv_sec-start.tv_sec;
		usec += end.tv_usec-start.tv_usec;
		printf("%d frames in %0d:%03d:%03d s:ms:us\n", i, end.tv_sec-start.tv_sec,
						(end.tv_usec-start.tv_usec)/1000,
						(end.tv_usec-start.tv_usec)%1000);
	}
	usec=((usec + sec*1000*1000)/j) % (1000*1000);
	sec /= j;

	printf("average: %0d:%03d:%03d s:ms:us\n", sec, usec/1000, usec%1000);
	usec=((usec + sec*1000*1000)/i) % (1000*1000);
	sec /= i;
	printf("average per write: %0d:%03d:%03d s:ms:us\n", sec, usec/1000, usec%1000);
	return 0;
}

int main()
{
#ifdef SEND
	printf("can_send\n");
	return can_send("vcan0");
#else
	printf("can_receive\n");
	return can_receive("vcan0");
#endif

}

