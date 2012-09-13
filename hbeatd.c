#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define HBEATD_VERSION "1.3.0 beta"

#define NODE_COUNT 10000
#define INT_SLEEP 1000
#define BUFLEN 256
#define SRV_GROUP "misc"
#define SRV_PORT 6220
#define SRV_IP "127.0.0.1"
#define TOLERANCE 4
#define SCRIPT_PATH "/etc/hbeatd/rc.d"


/* settings */
int pflag, sflag, vflag;
char *dvalue, *gvalue;
int Pvalue, rvalue, tvalue;

typedef struct {
	unsigned long int ip;
	unsigned long int time;
	unsigned long int uptime;
	unsigned int live;
} node;

static void pulse(void);
static void die(char *s)
{
	fprintf(stderr, "error: %s\n", s);
	exit(1);
}
static void usage(void)
{
	(void)fprintf(stderr, "usage: hbeatd [-v [-s | -p [-d] -i -g] -P]\n");
	(void)fprintf(stderr, "  -p\trun as pulse generator (default)\n");
	(void)fprintf(stderr, "  -s\trun as heartbeat sensor\n");
	(void)fprintf(stderr, "  -v\tverbose mode\n");
	(void)fprintf(stderr, "  -d\tdestination ip adress (pulse only)\n");
	(void)fprintf(stderr, "  -P\tdestination port number (default = 6220)\n");
	(void)fprintf(stderr, "  -i\tinterval in milliseconds (default = 1000)\n");
	(void)fprintf(stderr, "  -g\tserver group (default = misc)\n");
	(void)fprintf(stderr, "  -t\ttolerance level (default = 4)\n");
 
	exit(1);
}
static int fexists(char *fname)
{
	FILE *file;
	if ((file = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "file %s could not be opened\n", fname);
	} else {
		fclose(file);
		return 1;
	}
	return 0;
}
void signal_handler(sig)
int sig;
{
	switch(sig) {
	case SIGHUP:
		printf("hangup signal catched");
		break;
	case SIGTERM:
		printf("terminate signal catched");
		exit(0);
		break;
	}
}


int main(int argc, char *argv[])
{
	(void)printf("hbeatd version %s\n%s\n\n", HBEATD_VERSION, "Copyright (c) 2012 Comfirm AB");

	int c;
	pflag = sflag = vflag = 0;
	dvalue = NULL;
	gvalue = NULL;
	Pvalue = rvalue = tvalue = 0;

	while ((c = getopt(argc, argv, "spvhd:P:i:g:t:")) != -1)
	{
		switch (c) {
			case 's':
				sflag = 1;
				pflag = 0;
				break;
			case 'p':
				pflag = 1;
				sflag = 0;
				break;
			case 'v':
				vflag = 1;
				break;
			case 'h':
				usage();
				break;
			case 'd':
			     dvalue = optarg;
			     break;
			case 'P':
			     Pvalue = atoi(optarg);
			     break;
		     	case 'i':
			     rvalue = atoi(optarg);
			     break;
			case 'g':
			     gvalue = optarg;
			     break;
			case 't':
			     tvalue = atoi(optarg);
			     break;
			case '?':
			default:
				usage();
		}
	}
	
	if(dvalue == NULL)
		dvalue = SRV_IP;
	if(Pvalue == 0)
		Pvalue = SRV_PORT;
	if(rvalue == 0)
		rvalue = INT_SLEEP;
	if(tvalue == 0)
		tvalue = TOLERANCE;
	if(gvalue == NULL)
		gvalue = SRV_GROUP;

	if(!sflag)
	{
		printf("PULSE MODE\n");
		printf("Sending heartbeats to %s:%d(%s)...\n", dvalue, Pvalue, gvalue);
	}
	else
	{
		printf("SENSOR MODE\n");
		printf("Listening on *:%d...\n", Pvalue);
	}

	/* daemonize */
	if(!vflag)
	{
		int f;
		
		/* close all descriptors */
		for (f = getdtablesize(); f >= 0; --f)
			close(f);
	
		/* fork */
		f = fork();
		if (f < 0)
			die("could not fork process");

		/* exit parent */
		if (f > 0)
			exit(0);
		
		/* continue */
		setsid();
		umask(027);
		chdir("/");
		
		/* handle signals */
		signal(SIGCHLD,SIG_IGN); /* ignore child */
		signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
		signal(SIGTTOU,SIG_IGN);
		signal(SIGTTIN,SIG_IGN);
		signal(SIGHUP,signal_handler); /* catch hangup signal */
		signal(SIGTERM,signal_handler); /* catch kill signal */

		/* done */
	}

	/* pulse mode */
	if(!sflag)
	{
		(void)pulse();
		exit(0);
	}
	
	/* sensor mode */
	if(!fexists(SCRIPT_PATH))
	{
		die("/etc/hbeatd/rc.d does not exist");
	}
	
	/* socket */
	struct sockaddr_in sock, si_other;
	struct in_addr addr;
	
	int s, slen = sizeof(si_other);
	char buf[BUFLEN];

	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		die("couldn't create socket");

	memset((char *) &sock, 0, sizeof(sock));
	memset((char *) &addr, 0, sizeof(addr));
	
	sock.sin_family = AF_INET;
	sock.sin_port = htons(Pvalue);
	sock.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(s, (struct sockaddr *)&sock, sizeof(sock)) == -1)
		die("bind() failed");

	
	time_t seconds;
	unsigned int i, n, count, round;
	int add = 1;
	node nodes[NODE_COUNT];
	
	round = count = 0;

	while(1)
	{
		round++;
		
		if(recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen) == -1)
			die("recvfrom() failed");

		if(vflag)
			printf("Recieved heartbeat from %s(%s)\n", inet_ntoa(si_other.sin_addr), buf);

		/* get current time */
		unsigned long int time_now = (unsigned long int)time(NULL);
		
		/* first, search for it in the list */
		add = 1;
		for(i = 0; i < count; i++)
		{
			/* is it there? */
			if(nodes[i].ip == si_other.sin_addr.s_addr)
			{
				/* resurrected from the dead? :0 */
				if(nodes[i].live == 0)
				{
					/* spread the good news... */
					pid_t pID = fork();
					if (pID == 0)
					{
						addr.s_addr = nodes[i].ip;
						char *ip_str = inet_ntoa(addr);
						printf("resurrected: %s\n", ip_str);
				
						// time_now - nodes[i].time
						execl(SCRIPT_PATH, SCRIPT_PATH, "up", ip_str, (char *)0);
						exit(0);
					}
					nodes[i].live = 1;
				}
				
				/* update time*/
				nodes[i].time = time_now;
				nodes[i].uptime = time_now;
				add = 0;
			}
			else
			{
				/* while at it, check the nodes timestamp */
				if(nodes[i].live == 1 && time_now - nodes[i].time >= tvalue)
				{
					nodes[i].live = 0;
					/* do something (run script) */
					
					pid_t pID = fork();
					if (pID == 0)
					{
						addr.s_addr = nodes[i].ip;
						char *ip_str = inet_ntoa(addr);
						//printf("dead: %s\n", ip_str);
				
						// time_now - nodes[i].uptime
						execl(SCRIPT_PATH, SCRIPT_PATH, "rm", ip_str, (char *)0);
						exit(0);
					}
				}
			}
		}
		
		if(add)
		{
			char *ip_str = inet_ntoa(si_other.sin_addr);
			printf("new: %s\n", ip_str);
			
			node node_new = { si_other.sin_addr.s_addr, time_now, time_now, 1 };
			nodes[count] = node_new;
			
			count = count + 1;
		}
	}

	close(s);
	return 0;
}

static void pulse(void)
{
	struct sockaddr_in sock;
	int s, i, slen = sizeof(sock);
	
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		die("couldn't create socket");

	memset((char *) &sock, 0, sizeof(sock));
	sock.sin_family = AF_INET;
	sock.sin_port = htons(Pvalue);

	if (inet_aton(dvalue, &sock.sin_addr) == 0)
		die("inet_aton() failed");

	while (1)
	{
		if(vflag)
			printf("[ heartbeat ]\n");
			
		if (sendto(s, gvalue, strlen(gvalue), 0, (struct sockaddr *)&sock, slen) == -1)
			die("failed to send");


		nanosleep(rvalue * 1000);
	}

	close(s);
}

