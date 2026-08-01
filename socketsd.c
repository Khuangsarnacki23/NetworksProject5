#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ERROR 1
#define QLEN  1
#define PROTOCOL "tcp"
#define BUFLEN 1024

#define MAX_TEAMS   31
#define MAX_PLAYERS 18
#define MAX_GAMES   64
#define MAX_STATS   512

#define RES_OK               0
#define RES_EXISTS           1
#define RES_TEAM_NOT_FOUND   2
#define RES_PLAYER_NOT_FOUND 3
#define RES_FULL             4
#define RES_GAME_NOT_FOUND   5
#define RES_STATS_FULL       6

#define TRUE 1

char *port_number = NULL;

char *team[MAX_TEAMS];
char *players[MAX_PLAYERS][MAX_TEAMS];

typedef struct {
    int game_id;
    char date[32];
    char time[32];
    char location[128];
    char home_team[64];
    char away_team[64];
} Game;

Game games[MAX_GAMES];
int game_count = 0;

typedef struct {
    int game_id;
    char team_name[64];
    char player_name[64];
    int points;
    int assists;
    int rebounds;
    int minutes;
} PlayerStats;

PlayerStats stats[MAX_STATS];
int stats_count = 0;

int usage(char *progname) {
    fprintf(stderr, "usage: %s -p port\n", progname);
    exit(ERROR);
}

int errexit(char *format, char *arg) {
    fprintf(stderr, format, arg);
    fprintf(stderr, "\n");
    exit(ERROR);
}

void send_line(int sd2, const char *line) {
    printf("S -> C: %s", line);
    write(sd2, line, strlen(line));
}

int Register_team(const char *team_name) {
    int i;
    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0)
            return RES_EXISTS;
    }
    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] == NULL) {
            team[i] = malloc(strlen(team_name) + 1);
            if (!team[i]) return RES_FULL;
            strcpy(team[i], team_name);
            return RES_OK;
        }
    }
    return RES_FULL;
}

int Register_player(const char *player_name, const char *team_name) {
    int i, j;
    int team_idx = -1;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0) {
            team_idx = i;
            break;
        }
    }
    if (team_idx == -1) return RES_TEAM_NOT_FOUND;

    for (j = 0; j < MAX_PLAYERS; j++) {
        if (players[j][team_idx] != NULL &&
            strcmp(players[j][team_idx], player_name) == 0)
            return RES_EXISTS;
    }

    for (j = 0; j < MAX_PLAYERS; j++) {
        if (players[j][team_idx] == NULL) {
            players[j][team_idx] = malloc(strlen(player_name) + 1);
            if (!players[j][team_idx]) return RES_FULL;
            strcpy(players[j][team_idx], player_name);
            return RES_OK;
        }
    }
    return RES_FULL;
}

int team_exists(const char *team_name) {
    int i;
    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0)
            return 1;
    }
    return 0;
}

int game_exists(int game_id) {
    return (game_id >= 0 && game_id < game_count);
}

int Create_game(const char *date_str, const char *time_str,
                const char *location, const char *home, const char *away)
{
    if (!team_exists(home) || !team_exists(away))
        return RES_TEAM_NOT_FOUND;

    if (game_count >= MAX_GAMES)
        return RES_FULL;

    games[game_count].game_id = game_count;
    strncpy(games[game_count].date, date_str, sizeof(games[game_count].date) - 1);
    strncpy(games[game_count].time, time_str, sizeof(games[game_count].time) - 1);
    strncpy(games[game_count].location, location, sizeof(games[game_count].location) - 1);
    strncpy(games[game_count].home_team, home, sizeof(games[game_count].home_team) - 1);
    strncpy(games[game_count].away_team, away, sizeof(games[game_count].away_team) - 1);

    games[game_count].date[sizeof(games[game_count].date) - 1] = '\0';
    games[game_count].time[sizeof(games[game_count].time) - 1] = '\0';
    games[game_count].location[sizeof(games[game_count].location) - 1] = '\0';
    games[game_count].home_team[sizeof(games[game_count].home_team) - 1] = '\0';
    games[game_count].away_team[sizeof(games[game_count].away_team) - 1] = '\0';

    game_count++;
    return RES_OK;
}

int player_on_team(const char *player_name, const char *team_name) {
    int i, j;
    int team_idx = -1;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0) {
            team_idx = i;
            break;
        }
    }
    if (team_idx == -1) return 0;

    for (j = 0; j < MAX_PLAYERS; j++) {
        if (players[j][team_idx] != NULL &&
            strcmp(players[j][team_idx], player_name) == 0)
            return 1;
    }
    return 0;
}

int Record_stats(const char *player_name,
                 const char *team_name,
                 int game_id,
                 int points,
                 int assists,
                 int rebounds,
                 int minutes)
{
    if (!game_exists(game_id))                  return RES_GAME_NOT_FOUND;
    if (!team_exists(team_name))                return RES_TEAM_NOT_FOUND;
    if (!player_on_team(player_name, team_name)) return RES_PLAYER_NOT_FOUND;
    if (stats_count >= MAX_STATS)               return RES_STATS_FULL;

    strncpy(stats[stats_count].player_name, player_name, 63);
    strncpy(stats[stats_count].team_name,   team_name,   63);

    stats[stats_count].player_name[63] = '\0';
    stats[stats_count].team_name[63]   = '\0';

    stats[stats_count].game_id  = game_id;
    stats[stats_count].points   = points;
    stats[stats_count].assists  = assists;
    stats[stats_count].rebounds = rebounds;
    stats[stats_count].minutes  = minutes;

    stats_count++;
    return RES_OK;
}

void ListStats_player(int sd2, const char *player_name) {
    int i, found = 0;
    char line[BUFLEN];

    for (i = 0; i < stats_count; i++) {
        if (strcmp(stats[i].player_name, player_name) == 0) {
            snprintf(line, sizeof(line),
                     "STAT %d %s %s %d %d %d %d\n",
                     stats[i].game_id,
                     stats[i].team_name,
                     stats[i].player_name,
                     stats[i].points,
                     stats[i].assists,
                     stats[i].rebounds,
                     stats[i].minutes);
            send_line(sd2, line);
            found = 1;
        }
    }
    if (!found)
        send_line(sd2, "ERR NO_STATS_FOR_PLAYER\n");
}

void ListStats_team(int sd2, const char *team_name) {
    int i, found = 0;
    char line[BUFLEN];

    for (i = 0; i < stats_count; i++) {
        if (strcmp(stats[i].team_name, team_name) == 0) {
            snprintf(line, sizeof(line),
                     "STAT %d %s %s %d %d %d %d\n",
                     stats[i].game_id,
                     stats[i].team_name,
                     stats[i].player_name,
                     stats[i].points,
                     stats[i].assists,
                     stats[i].rebounds,
                     stats[i].minutes);
            send_line(sd2, line);
            found = 1;
        }
    }
    if (!found)
        send_line(sd2, "ERR NO_STATS_FOR_TEAM\n");
}

void ListStats_game(int sd2, int game_id) {
    int i, found = 0;
    char line[BUFLEN];

    for (i = 0; i < stats_count; i++) {
        if (stats[i].game_id == game_id) {
            snprintf(line, sizeof(line),
                     "STAT %d %s %s %d %d %d %d\n",
                     stats[i].game_id,
                     stats[i].team_name,
                     stats[i].player_name,
                     stats[i].points,
                     stats[i].assists,
                     stats[i].rebounds,
                     stats[i].minutes);
            send_line(sd2, line);
            found = 1;
        }
    }
    if (!found)
        send_line(sd2, "ERR NO_STATS_FOR_GAME\n");
}

/* ---- hardcoded 2025-26 NBA regular season seed data ---- */

static const char *seed_teams[] = {
    "Hawks","Celtics","Nets","Hornets","Bulls","Cavaliers","Mavericks","Nuggets",
    "Pistons","Warriors","Rockets","Pacers","Clippers","Lakers","Grizzlies","Heat",
    "Bucks","Timberwolves","Pelicans","Knicks","Thunder","Magic","76ers","Suns",
    "Blazers","Kings","Spurs","Raptors","Jazz","Wizards"
};

static const char *seed_players[][2] = {
    {"Hawks","Trae_Young"},{"Hawks","Jalen_Johnson"},{"Hawks","Dyson_Daniels"},{"Hawks","Zaccharie_Risacher"},
    {"Hawks","Onyeka_Okongwu"},{"Hawks","Kristaps_Porzingis"},{"Hawks","Luke_Kennard"},{"Hawks","Vit_Krejci"},
    {"Celtics","Jayson_Tatum"},{"Celtics","Jaylen_Brown"},{"Celtics","Derrick_White"},{"Celtics","Payton_Pritchard"},
    {"Celtics","Anfernee_Simons"},{"Celtics","Sam_Hauser"},{"Celtics","Neemias_Queta"},{"Celtics","Josh_Minott"},
    {"Nets","Michael_Porter_Jr"},{"Nets","Nic_Claxton"},{"Nets","Cam_Thomas"},{"Nets","Egor_Demin"},
    {"Nets","Terance_Mann"},{"Nets","DayRon_Sharpe"},{"Nets","Ziaire_Williams"},{"Nets","Noah_Clowney"},
    {"Hornets","LaMelo_Ball"},{"Hornets","Brandon_Miller"},{"Hornets","Miles_Bridges"},{"Hornets","Kon_Knueppel"},
    {"Hornets","Collin_Sexton"},{"Hornets","Josh_Green"},{"Hornets","Tre_Mann"},{"Hornets","Moussa_Diabate"},
    {"Bulls","Josh_Giddey"},{"Bulls","Coby_White"},{"Bulls","Nikola_Vucevic"},{"Bulls","Matas_Buzelis"},
    {"Bulls","Patrick_Williams"},{"Bulls","Ayo_Dosunmu"},{"Bulls","Kevin_Huerter"},{"Bulls","Isaac_Okoro"},
    {"Cavaliers","Donovan_Mitchell"},{"Cavaliers","Darius_Garland"},{"Cavaliers","Evan_Mobley"},{"Cavaliers","Jarrett_Allen"},
    {"Cavaliers","DeAndre_Hunter"},{"Cavaliers","Max_Strus"},{"Cavaliers","Sam_Merrill"},{"Cavaliers","Lonzo_Ball"},
    {"Mavericks","Cooper_Flagg"},{"Mavericks","Anthony_Davis"},{"Mavericks","Kyrie_Irving"},{"Mavericks","Klay_Thompson"},
    {"Mavericks","PJ_Washington"},{"Mavericks","Daniel_Gafford"},{"Mavericks","Dereck_Lively"},{"Mavericks","DAngelo_Russell"},
    {"Nuggets","Nikola_Jokic"},{"Nuggets","Jamal_Murray"},{"Nuggets","Aaron_Gordon"},{"Nuggets","Christian_Braun"},
    {"Nuggets","Cameron_Johnson"},{"Nuggets","Tim_Hardaway_Jr"},{"Nuggets","Bruce_Brown"},{"Nuggets","Jonas_Valanciunas"},
    {"Pistons","Cade_Cunningham"},{"Pistons","Jalen_Duren"},{"Pistons","Jaden_Ivey"},{"Pistons","Ausar_Thompson"},
    {"Pistons","Tobias_Harris"},{"Pistons","Isaiah_Stewart"},{"Pistons","Duncan_Robinson"},{"Pistons","Caris_LeVert"},
    {"Warriors","Stephen_Curry"},{"Warriors","Jimmy_Butler"},{"Warriors","Draymond_Green"},{"Warriors","Brandin_Podziemski"},
    {"Warriors","Jonathan_Kuminga"},{"Warriors","Buddy_Hield"},{"Warriors","Moses_Moody"},{"Warriors","Al_Horford"},
    {"Rockets","Kevin_Durant"},{"Rockets","Alperen_Sengun"},{"Rockets","Amen_Thompson"},{"Rockets","Jabari_Smith"},
    {"Rockets","Reed_Sheppard"},{"Rockets","Fred_VanVleet"},{"Rockets","Steven_Adams"},{"Rockets","Dorian_Finney-Smith"},
    {"Pacers","Tyrese_Haliburton"},{"Pacers","Pascal_Siakam"},{"Pacers","Andrew_Nembhard"},{"Pacers","Bennedict_Mathurin"},
    {"Pacers","Aaron_Nesmith"},{"Pacers","TJ_McConnell"},{"Pacers","Obi_Toppin"},{"Pacers","Jarace_Walker"},
    {"Clippers","Kawhi_Leonard"},{"Clippers","James_Harden"},{"Clippers","Ivica_Zubac"},{"Clippers","Bradley_Beal"},
    {"Clippers","John_Collins"},{"Clippers","Derrick_Jones"},{"Clippers","Brook_Lopez"},{"Clippers","Chris_Paul"},
    {"Lakers","LeBron_James"},{"Lakers","Luka_Doncic"},{"Lakers","Austin_Reaves"},{"Lakers","Rui_Hachimura"},
    {"Lakers","Deandre_Ayton"},{"Lakers","Marcus_Smart"},{"Lakers","Jarred_Vanderbilt"},{"Lakers","Gabe_Vincent"},
    {"Grizzlies","Ja_Morant"},{"Grizzlies","Jaren_Jackson_Jr"},{"Grizzlies","Zach_Edey"},{"Grizzlies","Santi_Aldama"},
    {"Grizzlies","Jaylen_Wells"},{"Grizzlies","Cedric_Coward"},{"Grizzlies","Ty_Jerome"},{"Grizzlies","Brandon_Clarke"},
    {"Heat","Bam_Adebayo"},{"Heat","Tyler_Herro"},{"Heat","Norman_Powell"},{"Heat","Andrew_Wiggins"},
    {"Heat","Davion_Mitchell"},{"Heat","Kelel_Ware"},{"Heat","Nikola_Jovic"},{"Heat","Pelle_Larsson"},
    {"Bucks","Giannis_Antetokounmpo"},{"Bucks","Myles_Turner"},{"Bucks","Kyle_Kuzma"},{"Bucks","Bobby_Portis"},
    {"Bucks","Kevin_Porter_Jr"},{"Bucks","Gary_Trent_Jr"},{"Bucks","AJ_Green"},{"Bucks","Ryan_Rollins"},
    {"Timberwolves","Anthony_Edwards"},{"Timberwolves","Julius_Randle"},{"Timberwolves","Rudy_Gobert"},{"Timberwolves","Jaden_McDaniels"},
    {"Timberwolves","Naz_Reid"},{"Timberwolves","Donte_DiVincenzo"},{"Timberwolves","Mike_Conley"},{"Timberwolves","Terrence_Shannon_Jr"},
    {"Pelicans","Zion_Williamson"},{"Pelicans","Trey_Murphy"},{"Pelicans","Herbert_Jones"},{"Pelicans","Dejounte_Murray"},
    {"Pelicans","Jordan_Poole"},{"Pelicans","Derik_Queen"},{"Pelicans","Jeremiah_Fears"},{"Pelicans","Yves_Missi"},
    {"Knicks","Jalen_Brunson"},{"Knicks","Karl-Anthony_Towns"},{"Knicks","OG_Anunoby"},{"Knicks","Mikal_Bridges"},
    {"Knicks","Josh_Hart"},{"Knicks","Mitchell_Robinson"},{"Knicks","Miles_McBride"},{"Knicks","Guerschon_Yabusele"},
    {"Thunder","Shai_Gilgeous-Alexander"},{"Thunder","Jalen_Williams"},{"Thunder","Chet_Holmgren"},{"Thunder","Luguentz_Dort"},
    {"Thunder","Isaiah_Hartenstein"},{"Thunder","Alex_Caruso"},{"Thunder","Cason_Wallace"},{"Thunder","Aaron_Wiggins"},
    {"Magic","Paolo_Banchero"},{"Magic","Franz_Wagner"},{"Magic","Desmond_Bane"},{"Magic","Jalen_Suggs"},
    {"Magic","Wendell_Carter_Jr"},{"Magic","Anthony_Black"},{"Magic","Tyus_Jones"},{"Magic","Jonathan_Isaac"},
    {"76ers","Joel_Embiid"},{"76ers","Tyrese_Maxey"},{"76ers","Paul_George"},{"76ers","VJ_Edgecombe"},
    {"76ers","Jared_McCain"},{"76ers","Quentin_Grimes"},{"76ers","Andre_Drummond"},{"76ers","Kelly_Oubre"},
    {"Suns","Devin_Booker"},{"Suns","Jalen_Green"},{"Suns","Dillon_Brooks"},{"Suns","Grayson_Allen"},
    {"Suns","Mark_Williams"},{"Suns","Ryan_Dunn"},{"Suns","Royce_ONeale"},{"Suns","Khaman_Maluach"},
    {"Blazers","Damian_Lillard"},{"Blazers","Scoot_Henderson"},{"Blazers","Shaedon_Sharpe"},{"Blazers","Deni_Avdija"},
    {"Blazers","Toumani_Camara"},{"Blazers","Donovan_Clingan"},{"Blazers","Jerami_Grant"},{"Blazers","Jrue_Holiday"},
    {"Kings","Domantas_Sabonis"},{"Kings","Zach_LaVine"},{"Kings","DeMar_DeRozan"},{"Kings","Malik_Monk"},
    {"Kings","Keegan_Murray"},{"Kings","Dennis_Schroder"},{"Kings","Keon_Ellis"},{"Kings","Nique_Clifford"},
    {"Spurs","Victor_Wembanyama"},{"Spurs","DeAaron_Fox"},{"Spurs","Dylan_Harper"},{"Spurs","Stephon_Castle"},
    {"Spurs","Devin_Vassell"},{"Spurs","Jeremy_Sochan"},{"Spurs","Julian_Champagnie"},{"Spurs","Luke_Kornet"},
    {"Raptors","Scottie_Barnes"},{"Raptors","Brandon_Ingram"},{"Raptors","RJ_Barrett"},{"Raptors","Immanuel_Quickley"},
    {"Raptors","Jakob_Poeltl"},{"Raptors","Gradey_Dick"},{"Raptors","JaKobe_Walter"},{"Raptors","Collin_Murray-Boyles"},
    {"Jazz","Lauri_Markkanen"},{"Jazz","Walker_Kessler"},{"Jazz","Keyonte_George"},{"Jazz","Ace_Bailey"},
    {"Jazz","Isaiah_Collier"},{"Jazz","Taylor_Hendricks"},{"Jazz","Brice_Sensabaugh"},{"Jazz","Kyle_Filipowski"},
    {"Wizards","Alex_Sarr"},{"Wizards","Bilal_Coulibaly"},{"Wizards","Kyshawn_George"},{"Wizards","Tre_Johnson"},
    {"Wizards","CJ_McCollum"},{"Wizards","Khris_Middleton"},{"Wizards","Corey_Kispert"},{"Wizards","Bub_Carrington"}
};

typedef struct { const char *date, *time, *loc, *home, *away; } SeedGame;
static const SeedGame seed_games[] = {
    {"2025-10-21","19:30","Paycom_Center","Thunder","Rockets"},
    {"2025-10-21","22:00","Crypto_Arena","Lakers","Warriors"},
    {"2025-10-22","19:00","Madison_Square_Garden","Knicks","Cavaliers"},
    {"2025-10-22","19:30","Frost_Bank_Center","Spurs","Mavericks"},
    {"2025-10-24","19:00","TD_Garden","Celtics","76ers"},
    {"2025-10-25","20:00","Fiserv_Forum","Bucks","Raptors"},
    {"2025-11-01","19:00","Kia_Center","Magic","Hawks"},
    {"2025-11-05","20:00","United_Center","Bulls","Pistons"},
    {"2025-11-08","19:00","Barclays_Center","Nets","Hornets"},
    {"2025-11-12","20:00","Target_Center","Timberwolves","Kings"},
    {"2025-11-15","19:00","Smoothie_King_Center","Pelicans","Grizzlies"},
    {"2025-11-19","22:00","Moda_Center","Blazers","Jazz"},
    {"2025-11-22","19:00","Capital_One_Arena","Wizards","Heat"},
    {"2025-11-26","21:00","Ball_Arena","Nuggets","Clippers"},
    {"2025-11-29","19:00","Gainbridge_Fieldhouse","Pacers","Bulls"},
    {"2025-12-05","19:30","Toyota_Center","Rockets","Suns"},
    {"2025-12-25","12:00","Madison_Square_Garden","Knicks","Cavaliers"},
    {"2025-12-25","14:30","Paycom_Center","Thunder","Spurs"},
    {"2025-12-25","17:00","Chase_Center","Warriors","Mavericks"},
    {"2025-12-25","20:00","Crypto_Arena","Lakers","Rockets"},
    {"2025-12-25","22:30","Target_Center","Timberwolves","Nuggets"},
    {"2026-01-03","19:00","Intuit_Dome","Clippers","Kings"},
    {"2026-01-10","19:00","State_Farm_Arena","Hawks","Hornets"},
    {"2026-01-15","19:00","Scotiabank_Arena","Raptors","Nets"},
    {"2026-01-19","15:00","FedEx_Forum","Grizzlies","Jazz"},
    {"2026-01-24","19:30","Wells_Fargo_Center","76ers","Wizards"}
};

typedef struct { int game_id; const char *team, *player; int pts, ast, reb, min; } SeedStat;
static const SeedStat seed_stats[] = {
    {0,"Thunder","Shai_Gilgeous-Alexander",35,5,4,44}, {0,"Thunder","Chet_Holmgren",22,2,11,38}, {0,"Rockets","Alperen_Sengun",27,6,12,41}, {0,"Rockets","Kevin_Durant",23,4,7,39},
    {1,"Lakers","Austin_Reaves",26,9,5,37}, {1,"Warriors","Jimmy_Butler",31,4,5,36}, {1,"Warriors","Stephen_Curry",23,5,3,34},
    {2,"Knicks","Jalen_Brunson",34,7,3,38}, {2,"Knicks","Karl-Anthony_Towns",19,2,11,35}, {2,"Cavaliers","Donovan_Mitchell",31,5,4,37},
    {3,"Spurs","Victor_Wembanyama",40,2,15,36}, {3,"Mavericks","Anthony_Davis",22,3,13,35}, {3,"Mavericks","Cooper_Flagg",10,4,10,32},
    {4,"Celtics","Jaylen_Brown",25,6,7,36}, {4,"76ers","Tyrese_Maxey",40,6,4,40},
    {5,"Bucks","Giannis_Antetokounmpo",31,7,14,35}, {5,"Raptors","Scottie_Barnes",22,8,9,37},
    {6,"Magic","Paolo_Banchero",28,5,8,36}, {6,"Hawks","Trae_Young",24,12,3,35},
    {7,"Bulls","Coby_White",27,5,4,34}, {7,"Pistons","Cade_Cunningham",33,10,6,38},
    {8,"Nets","Michael_Porter_Jr",28,2,9,35}, {8,"Hornets","LaMelo_Ball",30,8,6,36},
    {9,"Timberwolves","Anthony_Edwards",38,4,6,37}, {9,"Kings","Zach_LaVine",27,3,4,35},
    {10,"Pelicans","Zion_Williamson",29,5,8,33}, {10,"Grizzlies","Ja_Morant",32,9,4,36},
    {11,"Blazers","Shaedon_Sharpe",30,3,5,36}, {11,"Blazers","Deni_Avdija",26,6,9,37}, {11,"Jazz","Lauri_Markkanen",31,2,8,36},
    {12,"Wizards","Alex_Sarr",24,3,11,34}, {12,"Heat","Tyler_Herro",29,6,5,36},
    {13,"Nuggets","Nikola_Jokic",36,12,16,38}, {13,"Clippers","James_Harden",26,10,5,37},
    {14,"Pacers","Pascal_Siakam",30,4,8,36}, {14,"Bulls","Josh_Giddey",25,10,8,37},
    {15,"Rockets","Kevin_Durant",34,5,6,38}, {15,"Suns","Devin_Booker",33,7,4,38},
    {16,"Knicks","Jalen_Brunson",37,6,4,39}, {16,"Cavaliers","Darius_Garland",25,8,3,36},
    {17,"Thunder","Shai_Gilgeous-Alexander",33,6,5,38}, {17,"Spurs","Victor_Wembanyama",28,3,12,37},
    {18,"Warriors","Stephen_Curry",38,6,5,37}, {18,"Mavericks","Cooper_Flagg",24,5,9,38},
    {19,"Lakers","Luka_Doncic",35,11,9,39}, {19,"Lakers","LeBron_James",27,8,7,36}, {19,"Rockets","Alperen_Sengun",24,6,10,38},
    {20,"Timberwolves","Anthony_Edwards",36,5,6,38}, {20,"Nuggets","Nikola_Jokic",31,10,14,37},
    {21,"Clippers","Kawhi_Leonard",30,4,6,35}, {21,"Kings","Domantas_Sabonis",22,7,14,36},
    {22,"Hawks","Trae_Young",32,11,3,36}, {22,"Hornets","Brandon_Miller",27,4,6,36},
    {23,"Raptors","Brandon_Ingram",29,5,6,36}, {23,"Nets","Cam_Thomas",33,4,3,35},
    {24,"Grizzlies","Ja_Morant",28,7,5,34}, {24,"Jazz","Keyonte_George",26,8,3,36},
    {25,"76ers","Joel_Embiid",39,4,12,36}, {25,"76ers","Tyrese_Maxey",31,7,4,38}, {25,"Wizards","Bilal_Coulibaly",19,5,6,35}
};

void Seed_season(int sd2) {
    char line[BUFLEN];
    int i, t = 0, p = 0, g = 0, s = 0;

    /* refuse to seed a league that already has data - keeps repeat
       clicks (or curious users) from piling entries onto the server */
    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL) {
            send_line(sd2, "ERR LEAGUE_NOT_EMPTY\n");
            return;
        }
    }

    for (i = 0; i < (int)(sizeof(seed_teams) / sizeof(seed_teams[0])); i++)
        if (Register_team(seed_teams[i]) == RES_OK) t++;

    for (i = 0; i < (int)(sizeof(seed_players) / sizeof(seed_players[0])); i++)
        if (Register_player(seed_players[i][1], seed_players[i][0]) == RES_OK) p++;

    for (i = 0; i < (int)(sizeof(seed_games) / sizeof(seed_games[0])); i++)
        if (Create_game(seed_games[i].date, seed_games[i].time, seed_games[i].loc,
                        seed_games[i].home, seed_games[i].away) == RES_OK) g++;

    for (i = 0; i < (int)(sizeof(seed_stats) / sizeof(seed_stats[0])); i++)
        if (Record_stats(seed_stats[i].player, seed_stats[i].team, seed_stats[i].game_id,
                         seed_stats[i].pts, seed_stats[i].ast, seed_stats[i].reb,
                         seed_stats[i].min) == RES_OK) s++;

    snprintf(line, sizeof(line), "OK SEASON_SEEDED %d %d %d %d\n", t, p, g, s);
    send_line(sd2, line);
}

void List_teams(int sd2) {
    int i, found = 0;
    char line[BUFLEN];

    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL) {
            snprintf(line, sizeof(line), "TEAMNAME %s\n", team[i]);
            send_line(sd2, line);
            found = 1;
        }
    }
    if (!found)
        send_line(sd2, "ERR NO_TEAMS\n");
}

void List_players(int sd2, const char *team_name) {
    int i, j, team_idx = -1, found = 0;
    char line[BUFLEN];

    for (i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL && strcmp(team[i], team_name) == 0) {
            team_idx = i;
            break;
        }
    }
    if (team_idx == -1) {
        send_line(sd2, "ERR TEAM_NOT_FOUND\n");
        return;
    }

    for (j = 0; j < MAX_PLAYERS; j++) {
        if (players[j][team_idx] != NULL) {
            snprintf(line, sizeof(line), "ROSTER %s %s\n",
                     team_name, players[j][team_idx]);
            send_line(sd2, line);
            found = 1;
        }
    }
    if (!found)
        send_line(sd2, "ERR NO_PLAYERS_ON_TEAM\n");
}

void List_games(int sd2) {
    int i;
    char line[BUFLEN];

    if (game_count == 0) {
        send_line(sd2, "ERR NO_GAMES\n");
        return;
    }
    for (i = 0; i < game_count; i++) {
        snprintf(line, sizeof(line), "GAME %d %s %s %s %s %s\n",
                 games[i].game_id,
                 games[i].date,
                 games[i].time,
                 games[i].location,
                 games[i].home_team,
                 games[i].away_team);
        send_line(sd2, line);
    }
}

int write_json_files() {
    FILE *f = fopen("league_dump.json", "w");
    if (!f) return RES_STATS_FULL;

    fprintf(f, "{\n");

    fprintf(f, "  \"teams\": [\n");
    int first = 1;
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (team[i] != NULL) {
            if (!first) fprintf(f, ",\n");
            fprintf(f, "    \"%s\"", team[i]);
            first = 0;
        }
    }
    fprintf(f, "\n  ],\n");

    fprintf(f, "  \"players\": [\n");
    first = 1;
    for (int t = 0; t < MAX_TEAMS; t++) {
        if (team[t] == NULL) continue;
        for (int p = 0; p < MAX_PLAYERS; p++) {
            if (players[p][t] != NULL) {
                if (!first) fprintf(f, ",\n");
                fprintf(f,
                        "    { \"team\": \"%s\", \"name\": \"%s\" }",
                        team[t], players[p][t]);
                first = 0;
            }
        }
    }
    fprintf(f, "\n  ],\n");

    fprintf(f, "  \"games\": [\n");
    for (int i = 0; i < game_count; i++) {
        fprintf(f,
                "    { \"id\": %d, \"date\": \"%s\", \"time\": \"%s\", "
                "\"location\": \"%s\", \"home_team\": \"%s\", \"away_team\": \"%s\" }",
                games[i].game_id,
                games[i].date,
                games[i].time,
                games[i].location,
                games[i].home_team,
                games[i].away_team);

        if (i != game_count - 1) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"stats\": [\n");
    for (int i = 0; i < stats_count; i++) {
        fprintf(f,
                "    { \"game_id\": %d, \"team\": \"%s\", \"player\": \"%s\", "
                "\"points\": %d, \"assists\": %d, \"rebounds\": %d, \"minutes\": %d }",
                stats[i].game_id,
                stats[i].team_name,
                stats[i].player_name,
                stats[i].points,
                stats[i].assists,
                stats[i].rebounds,
                stats[i].minutes);

        if (i != stats_count - 1) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");

    fclose(f);
    return RES_OK;
}

void parseargs(int argc, char *argv[]) {
    int opt, port_flag = 0;

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
    char cmd[32];
    char arg1[128], arg2[128], arg3[128], arg4[128], arg5[128], arg6[128], arg7[128];
    ssize_t n;

    memset(buf, 0, sizeof(buf));
    memset(cmd, 0, sizeof(cmd));
    memset(arg1, 0, sizeof(arg1));
    memset(arg2, 0, sizeof(arg2));
    memset(arg3, 0, sizeof(arg3));
    memset(arg4, 0, sizeof(arg4));
    memset(arg5, 0, sizeof(arg5));
    memset(arg6, 0, sizeof(arg6));
    memset(arg7, 0, sizeof(arg7));

    n = read(sd2, buf, BUFLEN - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    printf("C -> S: %s", buf);

    int parsed = sscanf(buf, "%31s %127s %127s %127s %127s %127s %127s %127s",
                        cmd, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    if (parsed < 1) {
        send_line(sd2, "ERR UNKNOWN_COMMAND_OR_ARGS\n");
        return;
    }

    if (strcmp(cmd, "ADDTEAM") == 0) {
        int s = Register_team(arg1);
        if (s == RES_OK)          send_line(sd2, "OK TEAM_ADDED\n");
        else if (s == RES_EXISTS) send_line(sd2, "ERR TEAM_EXISTS\n");
        else                      send_line(sd2, "ERR TEAM_FULL\n");
    }
    else if (strcmp(cmd, "ADDPLAYER") == 0) {
        int s = Register_player(arg2, arg1);
        if (s == RES_OK)               send_line(sd2, "OK PLAYER_ADDED\n");
        else if (s == RES_EXISTS)      send_line(sd2, "ERR PLAYER_EXISTS\n");
        else if (s == RES_TEAM_NOT_FOUND) send_line(sd2, "ERR TEAM_NOT_FOUND\n");
        else                            send_line(sd2, "ERR PLAYER_FULL\n");
    }
    else if (strcmp(cmd, "CREATEGAME") == 0) {
        int s = Create_game(arg1, arg2, arg3, arg4, arg5);
        if (s == RES_OK)             send_line(sd2, "OK GAME_CREATED\n");
        else if (s == RES_TEAM_NOT_FOUND) send_line(sd2, "ERR TEAM_NOT_FOUND\n");
        else                          send_line(sd2, "ERR GAME_FULL\n");
    }
    else if (strcmp(cmd, "RECORDSTATS") == 0) {
        int game_id  = atoi(arg1);
        int points   = atoi(arg4);
        int assists  = atoi(arg5);
        int rebounds = atoi(arg6);
        int minutes  = atoi(arg7);

        int s = Record_stats(arg3, arg2, game_id, points, assists, rebounds, minutes);

        if (s == RES_OK)                 send_line(sd2, "OK STATS_RECORDED\n");
        else if (s == RES_GAME_NOT_FOUND)  send_line(sd2, "ERR GAME_NOT_FOUND\n");
        else if (s == RES_TEAM_NOT_FOUND)  send_line(sd2, "ERR TEAM_NOT_FOUND\n");
        else if (s == RES_PLAYER_NOT_FOUND) send_line(sd2, "ERR PLAYER_NOT_FOUND\n");
        else                               send_line(sd2, "ERR STATS_FULL\n");
    }
    else if (strcmp(cmd, "LISTSTATS") == 0) {
        if      (strcmp(arg1, "PLAYER") == 0) ListStats_player(sd2, arg2);
        else if (strcmp(arg1, "TEAM")   == 0) ListStats_team(sd2, arg2);
        else if (strcmp(arg1, "GAME")   == 0) ListStats_game(sd2, atoi(arg2));
        else send_line(sd2, "ERR LISTSTATS_MODE\n");
    }
    else if (strcmp(cmd, "SEEDSEASON") == 0) {
        Seed_season(sd2);
    }
    else if (strcmp(cmd, "LISTTEAMS") == 0) {
        List_teams(sd2);
    }
    else if (strcmp(cmd, "LISTPLAYERS") == 0) {
        List_players(sd2, arg1);
    }
    else if (strcmp(cmd, "LISTGAMES") == 0) {
        List_games(sd2);
    }
    else if (strcmp(cmd, "DUMPJSON") == 0) {
        int s = write_json_files();
        if (s == RES_OK) {
            /* stream the JSON back to the client, then confirm */
            FILE *jf = fopen("league_dump.json", "r");
            char jline[BUFLEN];
            if (jf) {
                while (fgets(jline, sizeof(jline), jf) != NULL)
                    send_line(sd2, jline);
                fclose(jf);
            }
            send_line(sd2, "OK JSON_WRITTEN\n");
        }
        else send_line(sd2, "ERR JSON_WRITE_FAILED\n");
    }
    else {
        send_line(sd2, "ERR UNKNOWN_COMMAND_OR_ARGS\n");
    }
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
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port        = htons((u_short) atoi(port_number));

    sd = socket(PF_INET, SOCK_STREAM, protoinfo->p_proto);
    if (sd < 0) errexit("cannot create socket", NULL);

    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("cannot bind to port %s", port_number);

    if (listen(sd, QLEN) < 0)
        errexit("cannot listen on port %s", port_number);

    printf("Server listening on port %s...\n", port_number);

    while (TRUE) {
        addrlen = sizeof(addr);
        sd2 = accept(sd, &addr, &addrlen);
        if (sd2 < 0) continue;
        handle_client(sd2);
        close(sd2);
    }

    exit(0);
}
