#include "common.h"

/* IPC ключ */
key_t IPC_key;

#define maxClients 40
#define sharedMemSize sizeof(clients)
int shmid; //айдишник для доступа к разделяемой памяти
client_t *clients; //адрес начала блока разделяемой памяти

/* IPC дескриптор для массива IPC семафоров
 * в массиве 2 семафора
 * 0 - для контроля количества экземпляров на одном объекте
 * 1 - для контроля доступа к разделяемой памяти
 */
int semid;
#define semCnt 2
#define semSharedMemoryNum 1

/* Структура для задания операции над семафором */
struct sembuf my_sembuf;
//идентификатор очереди
int serverQueueId; //id очереди серверных комманд
int myQueueId; //собственная очередь для передачи ввода вывода
pid_t serverPid; //pid сервера

char buffer[2048]; //буфер для чтения stdin

mess_data_t inputData;
mess_data_t outputData;

struct sembuf enterBuf = {-1,0,0};
void sharedEnter(int semid)
{
	if(semop(semid, &enterBuf, 1) < 0)
	{
		perror("Can't enter shared memory sem");
		exit(EXIT_FAILURE);
	}
}

struct sembuf leaveBuf = {1,0,0};
void sharedLeave(int semid)
{
	if(semop(semid, &leaveBuf, 1) < 0)
	{
		perror("Can't leave shared memory sem");
		exit(EXIT_FAILURE);
	}
}

/*создает ipc очередь и возвращает ipc id очереди */
int createQueue()
{
	srand(time(NULL));
	int c = rand() % 255;
	key_t key;
	key = ftok(".", c);
	int qid;
	if((qid = msgget(key, IPC_CREAT | 0666 | IPC_EXCL)) == -1)
	{
		perror("msgget");
		exit(EXIT_FAILURE);
	}
	return(qid);
}

int isSingleOnFile(int semid)
{
	/*
	Получает значение семафора
	*/
	int sem_value = semctl(semid, 0, GETVAL, 0);
    if(sem_value<0)
    {
		perror("semctl");
		exit(EXIT_FAILURE);
    }
 
	/*
	Если обнаруживаем, что значение семафора больше 0,
	т.е. уже запущена другая копия программы
	*/
	if(sem_value > 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/*Проверяет жив ли сервер раз в 100 мс и завершает работу клиента, если мертв*/
void *isServerAlive_fnc(void *ptr)
{
	while(1)
	{
		if(kill(serverPid, 0)==-1)
		{
			printf("Sever dead(\n");
			deinit();
			exit(EXIT_FAILURE);
		}
		usleep(100);
	}
}

void deinit()
{
	if(msgctl(myQueueId, IPC_RMID, 0) == -1)
	{
		perror("Error deleting queue!");
		exit(EXIT_FAILURE);
	}
	
	/*отцепить область разделяемой памяти*/
	if(shmdt(clients) == -1)
	{ 
		perror("Can't detach shared memory");
		exit(EXIT_FAILURE);
	}
}

void unregisterCmdFunc(int qid)
{
	mess_command_t sent;
	/* сформировать сообщение */
	sent.mtype = msgCommandType;
	sent.qid = myQueueId;
	sent.pid = getpid();
	sent.cmd = unregisterCmd;
	
	/* Отправить */
	msgsnd(serverQueueId, &sent, msgCmdLength, 0);
	printf("Unregister msg sent...\n");	
}

void registerCmdFunc(int qid)
{
	mess_command_t sent;
	mess_srvAnswer_t answer;
	/* сформировать сообщение */
	sent.mtype = msgCommandType;
	sent.qid = myQueueId;
	sent.pid = getpid();
	sent.cmd = registerCmd;
	
	/* Отправить */
	msgsnd(serverQueueId, &sent, msgCmdLength, 0);
	printf("Register msg sent...\n");
	if(msgrcv(myQueueId, &answer, msgAnswerLength, msgSrvAnswerType, 0) == -1)
	{
		perror("Error receiving data");
		exit(EXIT_FAILURE);
	}
	
	if(answer.status == statusOK)
	{
		printf("Answer received: OK!\n");
		serverPid = answer.pid;
	}
	else
	{
		printf("Connection refused!\n");
		exit(EXIT_FAILURE);
	}
}

void sendStdIn()
{
	memcpy(inputData.data, buffer, sizeof(buffer));
	inputData.mtype = msgInDataType;
	inputData.pid = getpid();
	inputData.stream = 0;
	msgsnd(myQueueId, &inputData, msgDataLength, 0);	
}

void *getStdOutErr_fnc(void *ptr)
{	
	while(1)
	{
		if(msgrcv(myQueueId, &outputData, msgDataLength, msgOutDataType, 0) == -1)
		{
			perror("Error receiving data");
			exit(EXIT_FAILURE);
		}		
		printf("<%d / %s",outputData.stream,outputData.data);
	}
}

void proc_manager(params *cmd)
{
	enterBuf.sem_op = -1;
	enterBuf.sem_flg = 0;
	enterBuf.sem_num = semSharedMemoryNum;
	leaveBuf.sem_op = 1;
	leaveBuf.sem_flg = 0;
	leaveBuf.sem_num = semSharedMemoryNum;
	
	/* IPC ключ */
	IPC_key = ftok(cmd->ftokFilePath, ProjId);
	if(IPC_key<0)
	{
		perror("ftok error");
		exit(EXIT_FAILURE);
	}
	
	/*
	Пытаемся получить доступ по ключу к массиву семафоров,
	если он существует
    */
	semid = semget(IPC_key, semCnt, 0);
	if(semid == -1)
	{
		perror("Can\'t get semaphore id. Make sure server is working");
		exit(EXIT_FAILURE);
	}
	
	if(isSingleOnFile(semid))
	{
		printf("That ftok object isn't in use! U should run server)\n");
		exit(EXIT_FAILURE);
	}
	
	/* получить идентификатор очереди сервера*/
	serverQueueId = msgget(IPC_key, 0);
	if(serverQueueId ==-1)
	{
		perror("proc_manager msgget()");
		exit(EXIT_FAILURE);
	}
	
	/* запросить общедоступную память */
	shmid = shmget(IPC_key, sharedMemSize,0);
	if (shmid == -1)
	{
		perror("shmget error");
		exit(EXIT_FAILURE);
	}
	
	/* получить указатель на начало области разделяемой памяти */
	clients = (client_t*)shmat(shmid, NULL, 0);
	if(clients==NULL)
	{
		perror("shmat error");
		exit(EXIT_FAILURE);
	}
	
	myQueueId = createQueue();
	
	/* зарегистрироваться на сервере */
	registerCmdFunc(serverQueueId);
	
	pthread_t isServerAlive_thr;
	pthread_create(&isServerAlive_thr, NULL, isServerAlive_fnc, NULL);
	pthread_detach(isServerAlive_thr);
	
	pthread_t getStdOutErr_thr;
	pthread_create(&getStdOutErr_thr, NULL, getStdOutErr_fnc, NULL);
	pthread_detach(getStdOutErr_thr);
	
	//////////////////////////////
	
	while(1)
	{
		scanf("%s",buffer);
		if(strcmp(buffer,"exit")==0)
		{
			/* отменить регистрацию перед завершением работы */
			unregisterCmdFunc(serverQueueId);
			deinit();
			exit(EXIT_SUCCESS);
		}
		
		if(strcmp(buffer,"status")==0)
		{
			printf("|  PID  | IN | OUT | ERR |\n");
			sharedEnter(semid);
			for(int i=0;i<maxClients;i++)
			{
				if(clients[i].pid != -1)
				{
					printf("|%7d|%4d|%5d|%5d|\n", clients[i].pid,clients[i].msgInCnt,clients[i].msgOutCnt,clients[i].msgErrCnt);
				}
			}
			sharedLeave(semid);
			continue;
		}
		
		sendStdIn();
	}
}
