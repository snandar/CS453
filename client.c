#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

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

	int ok = 1;
	char cinput[2];
	int input;

	printf("\n");
	printf("BET 0=1 1=1000, 2=10,000, 3 = 20,000, 4 = 50,000");
	printf(" 5 HIT");
	printf(" 6 STAND\n");

	/* Try */
	while (ok == 1)
	{
		printf("Choose an option: ");
		fgets(cinput, 2, stdin);
		input = atoi(cinput);

		switch (input)
		{
			case 0:
				nprintf(fd, "BET 1\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			case 1:
				nprintf(fd, "BET 1000\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			case 2:
				nprintf(fd, "BET 10000\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			case 3:
				nprintf(fd, "BET 20000\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			case 4:
				nprintf(fd, "BET 50000\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			case 5:
				nprintf(fd, "HIT\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			case 6:
				nprintf(fd, "STAND\n");
				if (!readline(fd, buf, sizeof(buf)))
					return 1;
				break; /* optional */
			default: 
				break;
			}

		printf("buf = %s\n", buf);
	}

	return 0;
}
