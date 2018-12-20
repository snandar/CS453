#define MSGKEY 46677

#define GET_PROCESSID 1 
#define GET_EPOCH 2
#define WIN 3
#define LOSE 4

//The structure of messages
typedef struct{
	long msg_type;
	int msg_number;
} st_message;