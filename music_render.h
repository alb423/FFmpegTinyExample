#ifdef  __cplusplus
extern  "C" {
#endif

// For msg queue
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define RET_SUCCESS		0
#define RET_FAIL   		(-1)
#define __TRACE__ printf("%s:%d\n", __func__, __LINE__);

#if 1
#define AUDIODBG printf
#else
void noprintf(char *format, ...)
{
    ;
}
#define AUDIODBG noprintf
#endif

// Message Queue
#define PERMS 0666
#define MUSIC_AUDIO_MSG_KEY           ((key_t) 88890L)
#define MUSIC_FILE_MSG_KEY            ((key_t) 88891L)

#define AUDIO_CMD_BASE			2000L
#define AUDIO_PLAY_REQ 			AUDIO_CMD_BASE
#define AUDIO_PLAY_CFM			AUDIO_CMD_BASE + 1
#define AUDIO_STOP_REQ 			AUDIO_CMD_BASE + 2
#define AUDIO_STOP_CFM			AUDIO_CMD_BASE + 3
#define AUDIO_FILE_OPEN_REQ     AUDIO_CMD_BASE + 4
#define AUDIO_FILE_OPEN_CFM		AUDIO_CMD_BASE + 5
#define AUDIO_FILE_CLOSE_REQ    AUDIO_CMD_BASE + 6
#define AUDIO_FILE_CLOSE_CFM	AUDIO_CMD_BASE + 7

#define MTEXT_LENGTH 256

typedef struct tMusicMsg {
    long mtype;
    char mtext[MTEXT_LENGTH];
} tMusicMsg;


#ifdef  __cplusplus
}
#endif
