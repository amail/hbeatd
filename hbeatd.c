#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define HBEATD_VERSION "1.1.0 beta"

#define INT_SLEEP 1
#define BUFLEN 1
#define SRV_PORT 6220
#define SRV_IP "127.0.0.1"
#define SCRIPT_PATH "/etc/hbeatd/rc.d"


/* settings */
char *dvalue;
int Pvalue;
int rvalue;


static void pulse(void);
static void die(char *s)
{
	fprintf(stderr, "error: %s\n", s);
	exit(1);
}
static void usage(void)
{
	(void)fprintf(stderr, "usage: hbeatd [-v [-s | -p [-d]] -P -i]\n");
	(void)fprintf(stderr, "  -p\trun as pulse generator (default)\n");
	(void)fprintf(stderr, "  -s\trun as heartbeat sensor\n");
	(void)fprintf(stderr, "  -v\tverbose mode\n");
	(void)fprintf(stderr, "  -d\tdestination ip adress (pulse only)\n");
	(void)fprintf(stderr, "  -P\tdestination port number\n");
	(void)fprintf(stderr, "  -i\tinterval in seconds (default = 1)\n");
 
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

int main(int argc, char *argv[])
{
	int c;
	int pflag, sflag, vflag;
	
	pflag = sflag = vflag = 0;
	dvalue = NULL;
	Pvalue = rvalue = 0;

	while ((c = getopt(argc, argv, "spvhd:P:i:")) != -1)
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

	/* let's start it */
	(void)printf("hbeatd version %s\n%s\n\n", HBEATD_VERSION, "Copyright (c) 2012 Comfirm AB");

	if(!sflag)
	{
		printf("PULSE MODE\n");
		printf("Sending heartbeats to %s:%d...\n", dvalue, Pvalue);
		(void)pulse();
		exit(0);
	}
	
	printf("SENSOR MODE\n");
	printf("Listening on *:%d...\n", Pvalue);
	
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
	
	if(bind(s, &sock, sizeof(sock)) == -1)
		die("bind");

	/* analyser
	   ========
	  1) put the found nodes in an array
	  2) continue collecting
	  3) compare the two lists for changes
	  	if they match, goto step 2
	  	else goto step 4
	  4) ALERT
	*/
	int i, n;
	unsigned long int *nodes = NULL;
	unsigned long int *nodes_b = NULL;
	unsigned long int node_list1[50];
	unsigned long int node_list2[50];
	unsigned long int dead_nodes[50];
	
	int *count = NULL;
	int count_l1 = 0;
	int count_l2 = 0;
	int count_dead = 0;
	
	int complete = 0;
	
	int add = 1;
	int found = 0;

	/* init */
	nodes = node_list1;
	nodes_b = node_list2;
	count = &count_l1;

	while(1)
	{
		if(recvfrom(s, buf, BUFLEN, 0, &si_other, &slen) == -1)
			die("recvfrom() failed");

		/* inspect heartbeat */
		if(buf[0] == '#')
		{
			/* build reference list */
			if(complete != 1)
			{
				/* first, search for it in the list */
				add = 1;
				for(i = 0; i < *count; i++)
				{
					/* is it there? */
					if(nodes[i] == si_other.sin_addr.s_addr)
					{
						/* the node is already in the list, round is complete */
						if(vflag)
							printf("[ round complete ]\n");
						complete++;
						add = 0;
						break;
					}
				}
			
				/* new node, add to list */
				if(add)
				{
					nodes[*count] = si_other.sin_addr.s_addr;
					*count = *count + 1;
				}
			}
			/* compare the lists */
			else
			{
				/* check dead nodes list */
				for(i = 0; i < count_dead; i++)
				{
					found = 0;
					for(n = 0; n < count_l1; n++)
					{
						if(dead_nodes[i] == nodes[n])
						{
							/* node's not dead! */
							found = 1;
							break;
						}
					}
					
					if(!found)
					{
						/* do something (run script) */
						if(fexists(SCRIPT_PATH))
						{
							pid_t pID = fork();
							if (pID == 0)
							{
								addr.s_addr = dead_nodes[i];
								char *ip_str = inet_ntoa(addr);
								printf("dead: %s\n", ip_str);
							
								execl(SCRIPT_PATH, SCRIPT_PATH, "rm", ip_str, (char *)0);
								exit(0);
							}
						}
					}
				}
				
				/* clear dead nodes list */
				count_dead = 0;
				
				/* compare the lists */
				if(count_l1 != count_l2) /* this if should be removed */
				{
					for(i = 0; i < count_l2; i++)
					{
						found = 0;
						for(n = 0; n < count_l1; n++)
						{
							if(nodes_b[i] == nodes[n])
							{
								found = 1;
								break;
							}
						}
					
						if(!found)
						{
							/* not found */
							/* add to dead nodes list */
							addr.s_addr = nodes_b[i];
							char *ip_str = inet_ntoa(addr);
							printf("missed: %s\n", ip_str);
							dead_nodes[count_dead++] = nodes_b[i];
						}
					}
					
					count_l2 = count_l1;
					
					if(nodes == &node_list1)
					{
						nodes = node_list2;
						nodes_b = node_list1;
					}
					else
					{
						nodes = node_list1;
						nodes_b = node_list2;
					}
				}
				
				count_l1 = 0;
				complete = 0;
			}
		}
		else
		{
			fprintf(stderr, "error: detected malformed heartbeat from %s\n", inet_ntoa(si_other.sin_addr));
		}
	}

	close(s);
	return 0;
}

static void pulse(void)
{
	struct sockaddr_in sock;
	int s, i, slen = sizeof(sock);
	char buf[BUFLEN] = "#";

	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		die("couldn't create socket");

	memset((char *) &sock, 0, sizeof(sock));
	sock.sin_family = AF_INET;
	sock.sin_port = htons(Pvalue);

	if (inet_aton(dvalue, &sock.sin_addr) == 0)
		die("inet_aton() failed");

	while (1)
	{
		if (sendto(s, buf, BUFLEN, 0, &sock, slen) == -1)
			die("failed to send");

		sleep(rvalue);
	}

	close(s);
}

