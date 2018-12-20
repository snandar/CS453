
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

//COPY FROM DECK.C
#define DECK_SIZE	52

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

void deck_init(struct deck *p);
void deck_shuffle(struct deck *p);
char deck_draw(struct deck *p);
void deck_deal(struct deck *p, struct hand *h);
void hand_init(struct hand *h);
char *hand_string(struct hand *h);

static const char CARDS[] = "A23456789TJQK";

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

//COPY FROM BJ.c

ssize_t readn(int fd, char *buffer, size_t count)
{
        int offset, block;

        offset = 0;
        while (count > 0)
        {
                block = read(fd, &buffer[offset], count);

                if (block < 0) return block;
                if (!block) return offset;

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

                if (block < 0) return block;
                if (!block) return offset;

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

int main(void)
{
        char host[] = "127.0.0.1";
        int port = 5555;
        int fd = -1;
        struct sockaddr_in sa;
        char buf[4096];
        int epoch;

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

        return 0;
}