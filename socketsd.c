#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ERROR 1
#define QLEN 1
#define PROTOCOL "tcp"
#define BUFLEN 1024
#define MAX_TEAMS 31
#define MAX_PLAYERS 18
#define MAX_GAMES 64
#define MAX_STR 31
#define MAX_STATS 512

char *port_number = NULL;
char *team[MAX_TEAMS];
char *players[MAX_PLAYERS][MAX_TEAMS];
int game_count = 0;


typedef struct {
    int game_id;
    char date[32];
    char time[16];
    char location[MAX_STR];
    char home_team[MAX_STR];
    char away_team[MAX_STR];
} Game;

typedef struct {
    char player_name[64];
    char team_name[64];
    int game_id;

    int points;
    int assists;
    int rebounds;
    int minutes_played;
} PlayerStats;



Game games[MAX_GAMES];

PlayerStats stats[MAX_STATS];
int stats_count = 0;


int usage (char *progname) {
    fprintf(stderr, "usage: %s -p port\n", progname);
    exit(ERROR);
}

int errexit(char *format, char *arg) {
    fprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    exit(ERROR);
}


int Register_team(const char *team_name) {
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0)
            return 1;
    }
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] == NULL) {
            team[i] = malloc(strlen(team_name) + 1);
            strcpy(team[i], team_name);
            return 0;
        }
    }
    return 2;
}


int Register_player(const char *player_name, const char *team_name) {
    int idx = -1;

    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return 2;

    for (int j = 0; j < MAX_PLAYERS; j++) {
        if (players[j][idx] != NULL &&
            strcmp(players[j][idx], player_name) == 0)
            return 1;
    }

    for (int j = 0; j < MAX_PLAYERS; j++) {
        if (players[j][idx] == NULL) {
            players[j][idx] = malloc(strlen(player_name) + 1);
            strcpy(players[j][idx], player_name);
            return 0;
        }
    }
    return 3;
}

int Create_game(const char *date, const char *time,
                const char *location, const char *home,
                const char *away)
{
    if (game_count >= MAX_GAMES)
        return 1;

    Game *g = &games[game_count];

    g->game_id = game_count;
    strcpy(g->date, date);
    strcpy(g->time, time);
    strcpy(g->location, location);
    strcpy(g->home_team, home);
    strcpy(g->away_team, away);

    game_count++;
    return 0;
}


void parseargs(int argc, char *argv[]) {
    int opt;
    int port_flag = 0;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port_number = optarg;
                port_flag = 1;
                break;
            default:
                usage(argv[0]);
        }
    }
    if (!port_flag) usage(argv[0]);
}

void handle_client(int sd2) {
    char buf[BUFLEN];
    ssize_t n = read(sd2, buf, BUFLEN - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    printf("C -> S: %s", buf);

    char cmd[32];
    char arg1[128], arg2[128], arg3[128], arg4[128], arg5[128];
    int num = sscanf(buf, "%31s %127s %127s %127s %127s %127s",
                     cmd, arg1, arg2, arg3, arg4, arg5);

    const char *resp;

    if (num == 2 && strcmp(cmd, "ADDTEAM") == 0) {
        int s = Register_team(arg1);
        if (s == 0) resp = "OK TEAM_ADDED\n";
        else if (s == 1) resp = "ERR TEAM_EXISTS\n";
        else resp = "ERR TEAM_FULL\n";
    }
    else if (num == 3 && strcmp(cmd, "ADDPLAYER") == 0) {
        int s = Register_player(arg2, arg1);
        if (s == 0) resp = "OK PLAYER_ADDED\n";
        else if (s == 1) resp = "ERR PLAYER_EXISTS\n";
        else if (s == 2) resp = "ERR TEAM_NOT_FOUND\n";
        else resp = "ERR PLAYER_FULL\n";
    }
    else if (num == 6 && strcmp(cmd, "CREATEGAME") == 0) {
        int s = Create_game(arg1, arg2, arg3, arg4, arg5);
        if (s == 0) resp = "OK GAME_CREATED\n";
        else if (s == 1) resp = "ERR INVALID_DATETIME\n";
        else if (s == 2) resp = "ERR TEAM_NOT_FOUND\n";
        else resp = "ERR GAME_FULL\n";
    }
    else {
        resp = "ERR UNKNOWN_COMMAND_OR_ARGS\n";
    }

    printf("S -> C: %s", resp);
    write(sd2, resp, strlen(resp));
}

int main(int argc, char *argv[]) {
    struct sockaddr_in sin;
    struct sockaddr addr;
    struct protoent *protoinfo;
    socklen_t addrlen;
    int sd, sd2;

    parseargs(argc, argv);

    if ((protoinfo = getprotobyname(PROTOCOL)) == NULL)
        errexit("cannot find protocol information for %s", PROTOCOL);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((u_short) atoi(port_number));

    sd = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
    if (sd < 0) errexit("cannot create socket", NULL);

    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("cannot bind to port %s", port_number);

    if (listen(sd, QLEN) < 0)
        errexit("cannot listen on port %s", port_number);

    printf("Server listening on port %s...\n", port_number);

    for (;;) {
        addrlen = sizeof(addr);
        sd2 = accept(sd, &addr, &addrlen);
        if (sd2 < 0) continue;
        handle_client(sd2);
        close(sd2);
    }

    exit(0);
}
