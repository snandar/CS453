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

//Copied from Blackjack
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
		{"FACEUP", 1, cmd_faceup},
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
