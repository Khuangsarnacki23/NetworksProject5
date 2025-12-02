#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ERROR 1
#define REQUIRED_ARGC 3
#define HOST_POS 1
#define PORT_POS 2
#define PROTOCOL "tcp"
#define BUFLEN 1024

#define ARG_HOST_POS 0x1
#define ARG_PORT_POS 0x2

#define MODE_NONE        0
#define MODE_ADDTEAM     1
#define MODE_ADDPLAYER   2
#define MODE_CREATEGAME  3
#define MODE_RECORDSTATS 4
#define MODE_LISTSTATS   5
#define MODE_DUMPJSON 6 
unsigned short host_flag = 0;
unsigned short port_flag = 0;

char *host_name   = NULL;
char *port_number = NULL;

int   mode        = MODE_NONE;

char *team_name   = NULL;
char *player_name = NULL;
char *game_date   = NULL;
char *game_time   = NULL;
char *location    = NULL;
char *home_team   = NULL;
char *away_team   = NULL;
int   game_id     = -1;  

int points  = 0;
int assists = 0;
int rebounds= 0;
int minutes = 0;

char message[BUFLEN];

void usage (char *progname)
{
    fprintf(stderr, "usage: %s -h hostname -p port\n", progname);
    exit (ERROR);
}

int errexit (char *format, char *arg)
{
    fprintf (stderr,format,arg);
    fprintf (stderr,"\n");
    exit (ERROR);
}


void parseargs(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv,"h:p:tbrlgj n:u:d:o:C:H:A:G:P:S:R:M:")) != -1)
    {
        switch (opt) {
            case 'h':
                host_flag |= ARG_HOST_POS;
                host_name  = optarg;
                break;

            case 'p':
                port_flag |= ARG_PORT_POS;
                port_number = optarg;
                break;

            case 't':
                mode = MODE_ADDTEAM;
                break;

            case 'b':
                mode = MODE_ADDPLAYER;
                break;

            case 'g':
                mode = MODE_CREATEGAME;
                break;

	    case 'j':
	        mode = MODE_DUMPJSON;
	        break;
		
            case 'r':
                mode = MODE_RECORDSTATS;
                break;

            case 'l':
                mode = MODE_LISTSTATS;
                break;
		
            case 'n':
                team_name = optarg;
                break;

            case 'u':
                player_name = optarg;
                break;

            case 'd':
                game_date = optarg;
                break;

            case 'o':
                game_time = optarg;
                break;

            case 'C':
                location = optarg;
                break;

            case 'H':
                home_team = optarg;
                break;

            case 'A':
                away_team = optarg;
                break;

            case 'G':
                game_id = atoi(optarg);
                break;

            case 'P':
                points = atoi(optarg);
                break;

            case 'S':
                assists = atoi(optarg);
                break;

            case 'R':
                rebounds = atoi(optarg);
                break;

            case 'M':
                minutes = atoi(optarg);
                break;

            case '?':
            default:
                usage(argv[0]);
        }
    }


    if (host_flag == 0) {
        fprintf(stderr, "error: no hostname given\n");
        usage(argv[0]);
    }
    if (port_flag == 0) {
        fprintf(stderr, "error: no port given\n");
        usage(argv[0]);
    }
    if (mode == MODE_NONE) {
        fprintf(stderr, "error: no mode given (-t|-b|-g|-r|-l)\n");
        usage(argv[0]);
    }


    switch (mode) {

        case MODE_ADDTEAM:
            if (team_name == NULL) {
                fprintf(stderr, "error: ADDTEAM (-t) requires -n team_name\n");
                usage(argv[0]);
            }
	    else{
		  snprintf(message, BUFLEN, "ADDTEAM %s\n", team_name);
	    }
            break;

        case MODE_ADDPLAYER:
            if (team_name == NULL || player_name == NULL) {
                fprintf(stderr,
                        "error: ADDPLAYER (-b) requires -n team_name and -u player_name\n");
                usage(argv[0]);
            }
	    else{
	      snprintf(message, BUFLEN, "ADDPLAYER %s %s\n", team_name, player_name);
	    }
            break;

        case MODE_CREATEGAME:
            if (!game_date || !game_time || !location || !home_team || !away_team) {
                fprintf(stderr,
                        "error: CREATEGAME (-g) requires:\n"
                        "   -d date -o time -C location -H home_team -A away_team\n");
                usage(argv[0]);
            }
	    else{
	      snprintf(message, BUFLEN, "CREATEGAME %s %s %s %s %s\n", game_date, game_time, location, home_team, away_team);
	    }
            break;

        case MODE_RECORDSTATS:
            if (game_id < 0 || !team_name || !player_name || !points || !assists || !rebounds || !minutes) {
                fprintf(stderr,
                        "error: RECORDSTATS (-r) requires:\n"
                        "   -G game_id -n team_name -u player_name\n");
                usage(argv[0]);
            }
	    else{
	      snprintf(message, BUFLEN, "RECORDSTATS %u %s %s %u %u %u %u\n", game_id, team_name, player_name, points, assists, rebounds, minutes);
	    }

            break;

        case MODE_LISTSTATS:
            if (!player_name && !team_name && game_id < 0) {
                fprintf(stderr,
                        "error: LISTSTATS (-l) requires one of:\n"
                        "   -u player_name OR -n team_name OR -G game_id\n");
                usage(argv[0]);
            }
	    if(player_name){
	      snprintf(message,BUFLEN,"LISTSTATS PLAYER %s\n",player_name);
	    }
	    else if(team_name){
	      snprintf(message,BUFLEN,"LISTSTATS TEAM %s\n",team_name);
	    }
	    else{
	      snprintf(message,BUFLEN,"LISTSTATS GAME %u\n", game_id);
	    }
            break;
	 case MODE_DUMPJSON:
            snprintf(message, BUFLEN, "DUMPJSON\n");
            break;
    }
}

int main (int argc, char *argv [])
{
    struct sockaddr_in sin;
    struct hostent *hinfo;
    struct protoent *protoinfo;
    int sd;
    parseargs(argc,argv);
    /* lookup the hostname */
    hinfo = gethostbyname (host_name);
    if (hinfo == NULL)
        errexit ("cannot find name: %s", host_name);

    /* set endpoint information */
    memset ((char *)&sin, 0x0, sizeof (sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons (atoi (port_number));
    memcpy ((char *)&sin.sin_addr,hinfo->h_addr,hinfo->h_length);

    if ((protoinfo = getprotobyname (PROTOCOL)) == NULL)
        errexit ("cannot find protocol information for %s", PROTOCOL);
    /* allocate a socket */
    /*   would be SOCK_DGRAM for UDP */
    sd = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
    if (sd < 0)
        errexit("cannot create socket",NULL);

    /* connect the socket */
    if (connect (sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit ("cannot connect", NULL);
    char rcv_buffer [BUFLEN];
    FILE *sp;
    sp = fdopen(sd,"r+");
    printf("%s\n",message);
    fputs(message,sp);
    fflush(sp);
    while (fgets(rcv_buffer, BUFLEN, sp) != NULL) {
      printf("%s", rcv_buffer);
    }
    close (sd);
    exit (0);
}
