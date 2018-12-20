#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include "fdutil.h"
#include "deck.h"
#include "misc.h"
#include "account.h"

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
                account_update(game->user, game->bet);
        }
        else if (dealer > player)
        {
                result = "LOSE";
                account_update(game->user, -game->bet);
        }
        else
        {
                result = "PUSH";
        }

        nprintf("+OK %s HAND %s %d", result, hand_string(&game->player), player);
        nprintf(" DEALER %s %d\n", hand_string(&game->dealer), dealer);

        game->state = STATE_IDLE;
        game->bet = 0;
}


void cmd_bet(struct game *game, int argc, char *argv[])
{
        if (game->state == STATE_PLAYING)
        {
                nprintf("-ERR You are already playing a game\n");
                return;
        }

        if (strspn(argv[1], "1234567890") != strlen(argv[1]))
        {
                nprintf("-ERR Expecting integer for a bet\n");
                return;
        }

        int bet = atoi(argv[1]);
        int balance = account_balance(game->user);

        if (balance > 1000000000)
        {
                nprintf("-ERR You already have enough money\n");
                return;
        }

        if (balance < bet)
        {
                nprintf("-ERR Not enough money\n");
                return;
        }

        if (bet <= 0)
        {
                nprintf("-ERR Invalid bet\n");
                return;
        }

        if (bet > 50000)
        {
                nprintf("-ERR Maximum bet is 50000\n");
                return;
        }

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

        nprintf("+OK BET %d HAND %s %d FACEUP %c %d\n", game->bet, hand_string(&game->player), hand_value(&game->player), faceup, card_value(faceup));
}

void cmd_hit(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                nprintf("-ERR You are not playing a game\n");
                return;
        }

        deck_deal(&game->deck, &game->player);

        if (hand_value(&game->player) > 21)
        {
                account_update(game->user, -game->bet);
                nprintf("+OK BUST %s %d", hand_string(&game->player), hand_value(&game->player));
                nprintf(" DEALER %s %d\n", hand_string(&game->dealer), hand_value(&game->dealer));
                game->state = STATE_IDLE;
                return;
        }

        nprintf("+OK GOT %s %d\n", hand_string(&game->player), hand_value(&game->player));
}

void cmd_stand(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                nprintf("-ERR You are not playing a game\n");
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
                nprintf("-ERR You are not playing a game\n");
                return;
        }

        nprintf("+OK ");

        for (i = 0; i < game->player.ncards; i++)
                nprintf("%c", game->player.cards[i]);

        nprintf(" %d\n", hand_value(&game->player));
}

void cmd_double(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                nprintf("-ERR You are not playing a game\n");
                return;
        }

        game->bet *= 2;

        deck_deal(&game->deck, &game->player);

        if (hand_value(&game->player) > 21)
        {
                account_update(game->user, -game->bet);
                nprintf("+OK BUST %s %d\n", hand_string(&game->player), hand_value(&game->player));
                game->state = STATE_IDLE;
                return;
        }

        game_finish(game);
}

void cmd_faceup(struct game *game, int argc, char *argv[])
{
        if (game->state != STATE_PLAYING)
        {
                nprintf("-ERR You are not playing a game\n");
                return;
        }

        char faceup = game->dealer.cards[0];

        nprintf("+OK %c %d\n", faceup, card_value(faceup));
}

void cmd_status(struct game *game, int argc, char *argv[])
{
        nprintf("+OK Started at %d\n", boot);
}

void cmd_balance(struct game *game, int argc, char *argv[])
{
        int balance = account_balance(game->user);

        nprintf("+OK %d\n", balance);
}

void cmd_flag(struct game *game, int argc, char *argv[])
{
        int balance = account_balance(game->user);

        if (balance >= 1000000)
                nprintf("+OK %s\n", account_flag());
        else
                nprintf("-ERR You need to become a millionaire to see the flag!\n");
}

void cmd_logout(struct game *game, int argc, char *argv[])
{
        if (game && game->state == STATE_PLAYING)
        {
                account_update(game->user, -game->bet);
                nprintf("+OK Your bet was forfeit. Please come back soon!\n");
        } else {
                nprintf("+OK Come back soon!\n");
        }
        exit(0);
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
        { "BALANCE", 1, cmd_balance },
        { "LOGOUT",  1, cmd_logout },
        { "EXIT",    1, cmd_logout },
        { "QUIT",    1, cmd_logout },
        { "FLAG",    1, cmd_flag },
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
                nprintf("-ERR Unknown command\n");
                return;
        }

        if (argc < c->args)
        {
                nprintf("-ERR Not enough arguments\n");
                return;
        }

        c->fn(game, argc, argv);
}

int main(void)
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

        nprintf("+OK Welcome to the Blackjack server!\n");

        char line[1024];

        while (1)
        {
                if (!fgets(line, sizeof(line), stdin))
                        break;

                if ((cp = strrchr(line, '\n')))
                        *cp = 0;

                argc = split(line, argv, MAXARGS);

                command(&game, argc, argv);
        }

        return 0;
}
