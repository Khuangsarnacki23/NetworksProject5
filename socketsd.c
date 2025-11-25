
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define REQUIRED_ARGC 3
#define PORT_POS 1
#define MSG_POS 2
#define ERROR 1
#define QLEN 1
#define PROTOCOL "tcp"
#define BUFLEN 1024
#define MAX_TEAMS 31
#define MAX_PLAYERS 18
#define ARG_MSG_POS  0x1
#define ARG_PORT_POS 0x2
#define ARG_TEAM_POS 0x4
char *message_request = NULL;
char *port_number = NULL;
char *team[MAX_TEAMS];
char *players[MAX_PLAYERS][MAX_TEAMS];
unsigned short port_flag = 0;
unsigned short message_flag = 0;
unsigned short cmd_line_flags = 0;
char *team_name = NULL;
int usage (char *progname)
{
    fprintf (stderr,"usage: %s port msg\n", progname);
    exit (ERROR);
}

int errexit (char *format, char *arg)
{
    fprintf (stderr,format,arg);
    fprintf (stderr,"\n");
    exit (ERROR);
}


void Register_team(char *team_name){
    int found = 0;
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0) {
            fprintf(stderr, "Team '%s' already exists\n", team_name);
            return;
        }
    }

    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] == NULL) {
            team[i] = malloc(strlen(team_name) + 1);
            strcpy(team[i], team_name);
            found = 1;
            printf("Team '%s' successfully registered.\n", team_name);
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "No more team slots available.\n");
    }

    printf("Here are all the current teams\n");
    for(int i = 0; i< MAX_TEAMS; i++){
      if(team[i] != NULL){
	printf("%s\n",team[i]);
      }
    }
}

void Register_player(char *player_name, char *team_name){
    int found = 0;
    for (int i = 0; i < MAX_TEAMS; i++) {
      if (team[i] == team_name){
	for (int j = 0; j<MAX_PLAYERS; j++){
	  if(players[j][i] == player_name){
	    fprintf(stderr, "Player '%s' already exists\n", player_name);
            return;
	  }
	}
      }
    }

    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] == team_name) {
	  for(int j = 0; j<MAX_PLAYERS; j++){
	    if(players[j][i] == NULL){
	      players[j][i] = malloc(strlen(player_name) + 1);
	      strcpy(player[j][i], team_name);
	      found = 1;
	      printf("Player '%s' successfully registered.\n", player_name);
	      break;
	    }
	  }
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "No more player slots available.\n");
    }

    printf("Here are all the current players on the team\n");
    for (int i = 0; i < MAX_TEAMS; i++) {
      if (team[i] == team_name){
	for (int j = 0; j<MAX_PLAYERS; j++){
	  if(players[j][i] != NULL){
	    printf("%s\n",players[j][i]);
          }
        }
      }
    }
}

void parseargs (int argc, char *argv [])
{
    int opt;

    while ((opt = getopt (argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
            case 'p':
                port_flag |= ARG_PORT_POS;
                port_number = optarg;
                break;
            default:
                usage (argv [0]);
        }
    }
    if (port_flag == 0)
    {
        fprintf (stderr,"error: no port given\n");
        usage (argv [0]);
    }
}




void handle_client(int sd2) {
    char buf[BUFLEN];
    ssize_t n = read(sd2, buf, BUFLEN - 1);
    if (n <= 0) {
        return;
    }
    buf[n] = '\0';

    char cmd[32];
    char arg[BUFLEN];

    int num = sscanf(buf, "%31s %1023[^\n]", cmd, arg);
    if (num >= 1) {
        if (strcmp(cmd, "ADDTEAM") == 0 && num == 2) {
            Register_team(arg);
            const char *resp = "OK TEAM_ADDED\n";
            write(sd2, resp, strlen(resp));
	} else if (strcmp(cmd, "ADDPLAYER") == 0 && num == 2) {
            Register_player(arg);
            const char *resp = "OK PLAYER_ADDED\n";
            write(sd2, resp, strlen(resp));
        } else {
            const char *resp = "ERR UNKNOWN_COMMAND_OR_ARGS\n";
            write(sd2, resp, strlen(resp));
        }
    } else {
        const char *resp = "ERR BAD_REQUEST\n";
        write(sd2, resp, strlen(resp));
    }
}

int main (int argc, char *argv [])
{
    struct sockaddr_in sin;
    struct sockaddr addr;
    struct protoent *protoinfo;
    socklen_t addrlen;
    int sd, sd2;

    parseargs(argc, argv);

    if ((protoinfo = getprotobyname(PROTOCOL)) == NULL)
        errexit("cannot find protocol information for %s", PROTOCOL);

    memset((char *)&sin, 0x0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((u_short) atoi(port_number));

    sd = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
    if (sd < 0)
        errexit("cannot create socket", NULL);

    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("cannot bind to port %s", port_number);

    if (listen(sd, QLEN) < 0)
        errexit("cannot listen on port %s\n", port_number);
    
    printf("Server listening on port %s...\n", port_number);


    for (;;) {
        addrlen = sizeof(addr);
        sd2 = accept(sd, &addr, &addrlen);
        if (sd2 < 0) {
            perror("accept failed");
            continue;
        }

        handle_client(sd2);
        close(sd2);
    }


    close(sd);
    exit (0);
}
