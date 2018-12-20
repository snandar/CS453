#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include "misc.h"

time_t boot;
#define MAXARGS 32
#define DECK_SIZE 52
static const char CARDS[] = "A23456789TJQK";
int pid;
int doneround = -1;
int value = -1;

struct hand
{
	// cards[0..ncards-1] is the hand
	char cards[DECK_SIZE];
	int ncards;
};

struct deck
{
	// remaining cards: cards[top..DECK_SIZE-1]
	int top;
	char cards[DECK_SIZE];
};

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

//copy from deck.c
void deck_init(struct deck *p)
{
	int i, j;

	p->top = 0;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 13; j++)
		{
			int k = i * 13 + j;

			p->cards[k] = CARDS[j];
		}
	}
}

// shuffle deck using Knuth shuffle
void deck_shuffle(struct deck *p)
{
	int i, r;

	for (i = 1; i < 52; i++)
	{
		r = rand() % i;
		char tmp = p->cards[i];
		p->cards[i] = p->cards[r];
		p->cards[r] = tmp;
	}
}

char deck_draw(struct deck *p)
{
	if (p->top >= DECK_SIZE)
		return '?';

	return p->cards[p->top++];
}

void deck_deal(struct deck *p, struct hand *h)
{
	if (h->ncards >= DECK_SIZE)
		return;

	h->cards[h->ncards++] = deck_draw(p);
}

void hand_init(struct hand *h)
{
	h->ncards = 0;
}

char *hand_string(struct hand *h)
{
	static char buf[1024];
	int i;

	for (i = 0; i < h->ncards; i++)
		buf[i] = h->cards[i];

	buf[i] = 0;

	return buf;
}

//Copied from Blackjack
time_t boot;
#define MAXARGS 32

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
	while (aces && (sum + aces * 11) > 21)
	{
		sum++;
		aces--;
	}

	sum += 11 * aces;

	return sum;
}

enum
{
	STATE_IDLE,
	STATE_PLAYING
};

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

	doneround = 1;

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

	for (i = 0; i < game->player.ncards; i++)
		printf("%c", game->player.cards[i]);

	value = hand_value(&game->player);
	printf(" %d\n", hand_value(&game->player));
}

void cmd_logout(struct game *game, int argc, char *argv[])
{
	printf("+OK Done\n");
	game_finish(game);
}

struct command
{
	const char *cmd;
	int args;
	void (*fn)(struct game *game, int argc, char *argv[]);
};

static struct command commands[] =
	{
		{"BET", 2, cmd_bet},
		{"HIT", 1, cmd_hit},
		{"STAND", 1, cmd_stand},
		{"HAND", 1, cmd_hand},
		{"EXIT", 1, cmd_logout},
		{NULL, 0, NULL}};

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
	boot = epoch;

	printf("hello. What is the pid? ");
	fgets(cpid, 5, stdin);
	printf("You put %s\n", cpid);
	pid = atoi(cpid);
	fflush(stdin);

	char *cmoney;
	printf("How much u have? ");
	fgets(cmoney, sizeof(cmoney), stdin);
	printf("Initial money: %s", cmoney);
	fflush(stdin);

	//Starting local blackjack
	char *cp;
	pid_t pid;
	int argc;
	char *argv[MAXARGS];
	struct game game;

	srand(boot ^ pid);

	game.user = "acidburn";
	game.state = STATE_IDLE;

	printf("+OK Local BlackJack open\n");

	int money = atoi(cmoney);

	//BET money
	argv[0] = "BET";
	argv[1] = "1";
	command(&game, 2, argv);

	//check value
	argv[0] = "HAND";
	command(&game, 1, argv);
	// printf("%d\n", value);

	//HIT
	argv[0] = "HIT";
	command(&game, 1, argv);
	// printf("%d\n", value);

	//check value
	argv[0] = "HAND";
	command(&game, 1, argv);
	// printf("%d\n", value);

	//HIT
	argv[0] = "HIT";
	command(&game, 1, argv);
	// printf("%d\n", value);

	//check value
	argv[0] = "HAND";
	command(&game, 1, argv);
	// printf("%d\n", value);

	//BET money
	argv[0] = "BET";
	argv[1] = "1";
	command(&game, 2, argv);

	return 0;
}
