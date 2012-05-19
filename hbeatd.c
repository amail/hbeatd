#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
//#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFLEN 1
#define PORT 5300
#define SRV_IP "127.0.0.1"

static void pulse(void);
static void die(char *s)
{
	fprintf(stderr, "error: %s\n", s);
	exit(1);
}
static void usage(void)
{
	(void)fprintf(stderr, "usage: hbeatd [ -v [-s | -p] ]\n");
	exit(1);
}


int main(int argc, char *argv[])
{
	int c;
	int pflag, sflag, vflag;
	
	pflag = sflag = vflag = 0;
	
	if (argc < 2)
		usage();
	
	while ((c = getopt(argc, argv, "spvh")) != -1)
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
			case '?':
			default:
				usage();
		}
	}
	

	if(!sflag)
	{
		printf("PULSE MODE\n");
		(void)pulse();
		exit(0);
	}
	
	printf("SENSOR MODE\n");
	
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
	sock.sin_port = htons(PORT);
	sock.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(s, &sock, sizeof(sock)) == -1)
		die("bind");

	/* analyser
	   ========
	  - collect heartbeats under 4 seconds
	  - put away those in an array
	  - continue collecting for another 4 seconds
	  - compare the two lists for changes
	  - ALERT
	*/
	int i, n;
	unsigned long int *nodes = NULL;
	unsigned long int *nodes_b = NULL;
	unsigned long int node_list1[50];
	unsigned long int node_list2[50];
	
	int *count = NULL;
	int count_l1 = 0;
	int count_l2 = 0;
	
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
			/* build reference lists */
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
						printf("[ round complete ]\n");
						complete++;
						add = 0;
						break;
					}
				}
			
				/* new node, add to list */
				if(add)
				{
					nodes[*count] = si_other.sin_addr.s_addr; /* inet_ntoa(addr) */
					*count = *count + 1;
				}
			}
			/* compare the lists */
			else
			{
				if(count_l1 != count_l2)
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
							addr.s_addr = nodes_b[i];
							printf("Dead: %s\n", inet_ntoa(addr));
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
			fprintf(stderr, "Detected malformed heartbeat from %s\n", inet_ntoa(si_other.sin_addr));
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
	sock.sin_port = htons(PORT);

	if (inet_aton(SRV_IP, &sock.sin_addr) == 0)
		die("inet_aton() failed");

	while (1)
	{
		if (sendto(s, buf, BUFLEN, 0, &sock, slen) == -1)
			die("failed to send");

		sleep(1);
	}

	close(s);
}
