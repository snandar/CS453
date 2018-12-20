#ifndef _DECK_H
#define _DECK_H

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

extern void deck_init(struct deck *p);
extern void deck_shuffle(struct deck *p);
extern char deck_draw(struct deck *p);
extern void deck_deal(struct deck *p, struct hand *h);
extern void hand_init(struct hand *h);
extern char *hand_string(struct hand *h);

#endif
