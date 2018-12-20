
#include <stdlib.h>
#include "deck.h"

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

