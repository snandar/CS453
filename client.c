#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "fdutil.h"
#include "deck.h"
#include "misc.h"

time_t boot;
#define MAXARGS 32

ssize_t readn(int fd, char *buffer, size_t count)
{
	int offset, block;

	offset = 0;
	while (count > 0)
	{
		block = read(fd, &buffer[offset], count);

		if (block < 0)
			return block;
		if (!block)
			return offset;

		offset += block;
		count -= block;
	}

	return offset;
}

ssize_t writen(int fd, char *buffer, size_t count)
{
	int offset, block;

	offset = 0;
	while (count > 0)
	{
		block = write(fd, &buffer[offset], count);

		if (block < 0)
			return block;
		if (!block)
			return offset;

		offset += block;
		count -= block;
	}

	return offset;
}

int nprintf(int fd, char *format, ...)
{
	va_list args;
	char buf[1024];

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	return writen(fd, buf, strlen(buf));
}

int readline(int fd, char *buf, size_t maxlen)
{
	int offset;
	ssize_t n;
	char c;

	offset = 0;
	while (1)
	{
		n = read(fd, &c, 1);

		if (n < 0 && n == EINTR)
			continue;

		if (c == '\n')
		{
			buf[offset++] = 0;
			break;
		}

		buf[offset++] = c;

		if (offset >= maxlen)
			return 0;
	}

	return 1;
}

//Copied from Blackjack

int card_value(char c)
{
        switch (c)
        {
                case 'A':
                        return 1;
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                        return c - '0';
                case 'T':
                case 'J':
                case 'Q':
                case 'K':
                        return 10;
        }

        return 0;
}

int hand_value(struct hand *h)
{
        int i;
        int aces = 0;
        int sum = 0;

        for (i = 0; i < h->ncards; i++)
        {
                if (h->cards[i] == 'A')
                        aces++;
                else
                        sum += card_value(h->cards[i]);
        }

        if (aces == 0)
                return sum;

        /* Give 11 points for aces, unless it causes us to go bust, so treat those as 1 point */
        while (aces && (sum + aces*11) > 21)
        {
                sum++;
                aces--;
        }

        sum += 11 * aces;

        return sum;
}

enum { STATE_IDLE, STATE_PLAYING };

struct game
{
        struct deck deck;
        struct hand player;
        struct hand dealer;
        int state;
        char *user;
        int bet;
};

void game_finish(struct game *game)
{
        char *result;

        int dealer = hand_value(&game->dealer);
        int player = hand_value(&game->player);

        if (dealer > 21 || dealer < player)
        {
                result = "WIN";
           
        }
        else if (dealer > player)
        {
                result = "LOSE";
               
        }
        else
        {
                result = "PUSH";
        }

        printf("+OK %s HAND %s %d", result, hand_string(&game->player), player);
        printf(" DEALER %s %d\n", hand_string(&game->dealer), dealer);

        game->state = STATE_IDLE;
        game->bet = 0;
}


void cmd_bet(struct game *game, int argc, char *argv[])
{
        if (game->state == STATE_PLAYING)
        {
                printf("-ERR You are already playing a game\n");
                return;
        }

        if (strspn(argv[1], "1234567890") != strlen(argv[1]))
        {
                printf("-ERR Expecting integer for a bet\n");
                return;
        }

        int bet = atoi(argv[1]);

        deck_init(&game->deck);
        deck_shuffle(&game->deck);

        hand_init(&game->player);
        hand_init(&game->dealer);

        deck_deal(&game->deck, &game->player);
        deck_deal(&game->deck, &game->player);

        deck_deal(&game->deck, &game->dealer);
        deck_deal(&game->deck, &game->dealer);

        game->bet = atoi(argv[1]);
        game->state = STATE_PLAYING;

        char faceup = game->dealer.cards[0];

        printf("+OK BET %d HAND %s %d FACEUP %c %d\n", game->bet, hand_string(&game->player), hand_value(&game->player), faceup, card_value(faceup));
}

void cmd_hit(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                printf("-ERR You are not playing a game\n");
                return;
        }

        deck_deal(&game->deck, &game->player);

        if (hand_value(&game->player) > 21)
        {
                printf("+OK BUST %s %d", hand_string(&game->player), hand_value(&game->player));
                printf(" DEALER %s %d\n", hand_string(&game->dealer), hand_value(&game->dealer));
                game->state = STATE_IDLE;
                return;
        }

        printf("+OK GOT %s %d\n", hand_string(&game->player), hand_value(&game->player));
}

void cmd_stand(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                printf("-ERR You are not playing a game\n");
                return;
        }

        while (hand_value(&game->dealer) < 17)
                deck_deal(&game->deck, &game->dealer);

        game_finish(game);
}

void cmd_hand(struct game *game, int argc, char *argv[])
{
        int i;

        if (game->state != STATE_PLAYING)
        {
                printf("-ERR You are not playing a game\n");
                return;
        }

        printf("+OK ");

        for (i = 0; i < game->player.ncards; i++)
                printf("%c", game->player.cards[i]);

        printf(" %d\n", hand_value(&game->player));
}

void cmd_double(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                printf("-ERR You are not playing a game\n");
                return;
        }

        game->bet *= 2;

        deck_deal(&game->deck, &game->player);

        if (hand_value(&game->player) > 21)
        {
                printf("+OK BUST %s %d\n", hand_string(&game->player), hand_value(&game->player));
                game->state = STATE_IDLE;
                return;
        }

        game_finish(game);
}

void cmd_faceup(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                printf("-ERR You are not playing a game\n");
                return;
        }

        char faceup = game->dealer.cards[0];

        printf("+OK %c %d\n", faceup, card_value(faceup));
}

void cmd_status(struct game *game, int argc, char *argv[])
{
        printf("+OK Started at %d\n", boot);
}

struct command
{
        const char *cmd;
        int args;
        void (*fn)(struct game *game, int argc, char *argv[]);
};

static struct command commands[] =
{
        { "BET",     2, cmd_bet },
        { "HIT",     1, cmd_hit },
        { "STAND",   1, cmd_stand },
        { "HAND",    1, cmd_hand },
        { "FACEUP",  1, cmd_faceup },
        { "DOUBLE",  1, cmd_double },
        { "STATUS",  1, cmd_status },
        { "EXIT",    1, cmd_logout },
        { "QUIT",    1, cmd_logout },
        { NULL, 0, NULL }
};

void command(struct game *game, int argc, char *argv[])
{
        struct command *c = commands;

        while (c->cmd != NULL)
        {
                if (!strcasecmp(c->cmd, argv[0]))
                        break;

                c++;
        }

        if (c->cmd == NULL)
        {
                printf("-ERR Unknown command\n");
                return;
        }

        if (argc < c->args)
        {
                printf("-ERR Not enough arguments\n");
                return;
        }

        c->fn(game, argc, argv);
}

void blackjack(char *line)
{
	char *cp;
	pid_t pid;
	int argc;
	char *argv[MAXARGS];
	struct game game;

	boot = time(NULL);
	pid = getpid();

	srand(boot ^ pid);

	game.user = "acidburn";
	game.state = STATE_IDLE;

	printf("+OK Welcome to the Blackjack server!\n");

	char line[1024];

	if ((cp = strrchr(line, '\n')))
		*cp = 0;

	argc = split(line, argv, MAXARGS);

	command(&game, argc, argv);
}

//Main function
int main(void)
{
	char host[] = "127.0.0.1";
	int port = 5555;
	int fd = -1;
	struct sockaddr_in sa;
	char buf[4096];
	int epoch;
	char cpid[5];

	if (!inet_aton(host, &sa.sin_addr))
	{
		perror("inet_aton");
		return 1;
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		perror("socket");
		return 1;
	}

	sa.sin_port = htons(port);
	sa.sin_family = AF_INET;

	if (connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0)
	{
		perror("inet_aton");
		return 1;
	}

	/* "Welcome" message */
	if (!readline(fd, buf, sizeof(buf)))
		return 1;
	printf("buf = %s\n", buf);

	/* Check status */
	nprintf(fd, "STATUS\n");
	if (!readline(fd, buf, sizeof(buf)))
		return 1;

	printf("buf = %s\n", buf);

	if (sscanf(buf, "+OK Started at %d", &epoch) != 1)
		return 1;

	printf("epoch = %d\n", epoch);

	printf("hello. What is the pid? ");
	fgets(cpid, 5, stdin);
	printf("You put %s\n", cpid);
	int pid = atoi(cpid);
	fflush(stdin);

	char *cmoney;
	printf("How much u have? ");
	fgets(cmoney, sizeof(cmoney), stdin);
	printf("Initial money: %s", cmoney);
	fflush(stdin);

	return 0;
}

