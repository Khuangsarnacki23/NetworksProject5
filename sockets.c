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
#define ARG_TEAMNAME   0x0001  /* -n */
#define ARG_PLAYERNAME 0x0002  /* -u */
#define ARG_DATE       0x0004  /* -d */
#define ARG_TIME       0x0008  /* -o */
#define ARG_LOCATION   0x0010  /* -C */
#define ARG_HOME       0x0020  /* -H */
#define ARG_AWAY       0x0040  /* -A */
#define ARG_GAMEID     0x0080  /* -G */
#define ARG_POINTS     0x0100  /* -P */
#define ARG_ASSISTS    0x0200  /* -S */
#define ARG_REBOUNDS   0x0400  /* -R */
#define ARG_MINUTES    0x0800 
#define MODE_NONE        0
#define MODE_ADDTEAM     1
#define MODE_ADDPLAYER   2
#define MODE_CREATEGAME  3
#define MODE_RECORDSTATS 4
#define MODE_LISTSTATS   5
#define MODE_DUMPJSON 6 
unsigned int args_flag = 0;
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

    args_flag = 0;  /* reset for each run */

    while ((opt = getopt(argc, argv,
                         "h:p:tbrlgjn:u:d:o:C:H:A:G:P:S:R:M:")) != -1)
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

            /* modes */
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

            /* arguments with values */
            case 'n':   /* team name */
                team_name = optarg;
                args_flag |= ARG_TEAMNAME;
                break;

            case 'u':   /* player name */
                player_name = optarg;
                args_flag |= ARG_PLAYERNAME;
                break;

            case 'd':   /* date */
                game_date = optarg;
                args_flag |= ARG_DATE;
                break;

            case 'o':   /* time */
                game_time = optarg;
                args_flag |= ARG_TIME;
                break;

            case 'C':   /* location */
                location = optarg;
                args_flag |= ARG_LOCATION;
                break;

            case 'H':   /* home team */
                home_team = optarg;
                args_flag |= ARG_HOME;
                break;

            case 'A':   /* away team */
                away_team = optarg;
                args_flag |= ARG_AWAY;
                break;

            case 'G':   /* game id */
                game_id = atoi(optarg);
                args_flag |= ARG_GAMEID;
                break;

            case 'P':   /* points */
                points = atoi(optarg);
                args_flag |= ARG_POINTS;
                break;

            case 'S':   /* assists */
                assists = atoi(optarg);
                args_flag |= ARG_ASSISTS;
                break;

            case 'R':   /* rebounds */
                rebounds = atoi(optarg);
                args_flag |= ARG_REBOUNDS;
                break;

            case 'M':   /* minutes */
                minutes = atoi(optarg);
                args_flag |= ARG_MINUTES;
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
        fprintf(stderr, "error: no mode given (-t|-b|-g|-r|-l|-j)\n");
        usage(argv[0]);
    }

    unsigned int required;
    unsigned int allowed  = 0;

    switch (mode) {

        case MODE_ADDTEAM:
            required = ARG_TEAMNAME;
            allowed  = ARG_TEAMNAME;
            if ((args_flag & required) != required && args_flag & ~allowed) {
                fprintf(stderr, "error: ADDTEAM (-t) requires -n team_name\n");
                usage(argv[0]);
            }
            snprintf(message, BUFLEN, "ADDTEAM %s\n", team_name);
            break;

        case MODE_ADDPLAYER:
            required = ARG_TEAMNAME | ARG_PLAYERNAME;
            allowed  = required;
            if ((args_flag & required) != required && args_flag & ~allowed) {
                fprintf(stderr,
                        "error: ADDPLAYER (-b) requires -n team_name and -u player_name\n");
                usage(argv[0]);
            }
            snprintf(message, BUFLEN,
                     "ADDPLAYER %s %s\n", team_name, player_name);
            break;

        case MODE_CREATEGAME:
            required = ARG_DATE | ARG_TIME | ARG_LOCATION | ARG_HOME | ARG_AWAY;
            allowed  = required;
            if ((args_flag & required) != required && args_flag & ~allowed) {
                fprintf(stderr,
                        "error: CREATEGAME (-g) requires:\n"
                        "   -d date -o time -C location -H home_team -A away_team\n");
                usage(argv[0]);
            }
            snprintf(message, BUFLEN,
                     "CREATEGAME %s %s %s %s %s\n",
                     game_date, game_time, location, home_team, away_team);
            break;

        case MODE_RECORDSTATS:
            required = ARG_GAMEID | ARG_TEAMNAME | ARG_PLAYERNAME |
                       ARG_POINTS | ARG_ASSISTS | ARG_REBOUNDS | ARG_MINUTES;
            allowed  = required;
            if ((args_flag & required) != required && args_flag & ~allowed) {
                fprintf(stderr,
                        "error: RECORDSTATS (-r) requires:\n"
                        "   -G game_id -n team_name -u player_name "
                        "-P points -S assists -R rebounds -M minutes\n");
                usage(argv[0]);
            }
            snprintf(message, BUFLEN,
                     "RECORDSTATS %u %s %s %u %u %u %u\n",
                     game_id, team_name, player_name,
                     points, assists, rebounds, minutes);
            break;

        case MODE_LISTSTATS:
            allowed = ARG_PLAYERNAME | ARG_TEAMNAME | ARG_GAMEID;
            if (args_flag & ~allowed) {
                fprintf(stderr,
                        "error: LISTSTATS (-l) only accepts -u, -n, or -G (one of them)\n");
                usage(argv[0]);
            }

            if (args_flag & ARG_PLAYERNAME) {
                snprintf(message, BUFLEN,
                         "LISTSTATS PLAYER %s\n", player_name);
            } else if (args_flag & ARG_TEAMNAME) {
                snprintf(message, BUFLEN,
                         "LISTSTATS TEAM %s\n", team_name);
            } else {
                snprintf(message, BUFLEN,
                         "LISTSTATS GAME %u\n", game_id);
            }
            break;

        case MODE_DUMPJSON:
            if (args_flag != 0) {
                fprintf(stderr,
                        "error: DUMPJSON (-j) does not take any additional arguments\n");
                usage(argv[0]);
            }
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
