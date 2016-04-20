#ifndef common
#define common

#define _GNU_SOURCE
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#include <sys/types.h> //типы (key_t, f.e.)
#include <sys/ipc.h> //ipc
#include <sys/sem.h> //семафоры
#include <sys/msg.h> //очереди сообщений
#include <sys/shm.h> //разделяемая память

#define ProjId 0
#define msgCommandType 1
#define msgSrvAnswerType 2
#define msgOutDataType 3
#define msgInDataType 4

typedef enum regStatus
{
	statusOK = 0,
	statusRefused = 1
}regStatus_t;

typedef enum commands
{
	registerCmd = 0,
	statusCmd = 1,
	unregisterCmd = 2
} cmd_t;

/* переопределить структуру msgbuf(макс. размер - 4080(56) байт) */
/* структура для передачи команд */
typedef struct cmdmsgbuf
{
  long mtype;
  pid_t pid;
  int qid;
  cmd_t cmd;  
} mess_command_t;

/* структура для передачи данных*/
typedef struct datamsgbuf
{
  long mtype;
  pid_t pid;
  int stream;
  char data[2048]; 
} mess_data_t;

/* структура для ответа сервера на регистрацию */
typedef struct ansmsgbuf
{
  long mtype;
  pid_t pid; //pid сервера
  regStatus_t status; //ответ сервера
} mess_srvAnswer_t;

typedef struct client
{
	pid_t pid; //pid клиента
	int qid; //id очереди	
	int msgInCnt;
	int msgOutCnt;
	int msgErrCnt;
}client_t;

/* длина сообщения в байтах, без учета поля типа сообщения */
#define msgCmdLength (sizeof(mess_command_t) - sizeof(long))
#define msgAnswerLength (sizeof(mess_srvAnswer_t) - sizeof(long))
#define msgDataLength (sizeof(mess_data_t) - sizeof(long))

typedef struct
{
	char *ftokFilePath;
}params;

void proc_manager(params *cmd);
void deinit();

#endif
