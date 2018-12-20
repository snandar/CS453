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

//copy from deck.c

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
