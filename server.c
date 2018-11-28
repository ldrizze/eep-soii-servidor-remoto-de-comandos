#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

#define NMB_OF_COMMANDS 12
#define BUFFER_SIZE 64
#define RESPONSE_TIMEOUT 5

// Enumerador de comandos para serem usados como ALIAS
enum commands{COMMAND_DATE, COMMAND_TIME, COMMAND_NODENAME, COMMAND_SYSNAME, COMMAND_RELEASE, COMMAND_VERSION, COMMAND_MACHINE,COMMAND_PROCCESS,COMMAND_FREERAM,COMMAND_TOTALRAM,COMMAND_UPTIME,COMMAND_PROCCESSORS};

// Sufixos de comandos
char command_suffix[NMB_OF_COMMANDS][20] = {
	"DATE",
	"TIME",
	"NODENAME",
	"SYSNAME",
	"RELEASE",
	"VERSION",
	"MACHINE",
	"PROCCESS",
	"FREERAM",
	"TOTALRAM",
	"UPTIME",
	"PROCCESSORS"
};

int get_pid_from_message(char *message); // Obtem o pid de uma mensagem
int get_command_id(char *command); // Obter o id do comando
void send_response(char *response, int client_pid, int shmid);
void strtoupper(char *command); // Altera a string para uppercase
void clear_trash_in_memory_segment(int shmid); // Limpa o segmento de memoria compartilhado antes de escutar comandos

// Principal
int main(){
	key_t key = ftok("/tmp", 'a');
	int shmid, msgid, id_comando;
	char *resposta;
	char comando[BUFFER_SIZE], response[BUFFER_SIZE];
	int exit = 0, tem_mensagem = 0, client_pid, command_id;

	// UTS Name
	struct utsname uts;
    uname(&uts);

    // Systinfo
	struct sysinfo info;
    int days, hours, mins;
    sysinfo(&info);


	// Time
	time_t t;
	struct tm *tm;

	// Initialize
	printf("Iniciando o servidor...\n");

	shmid=shmget(key,0,0);
	if(shmid != -1){
		printf("Segmento de memória ja existe, apagando... %d\n", shmctl(shmid,IPC_RMID,0));
	}

  	msgid=msgget(key,0600|IPC_CREAT); // Cria a fila de mensagens
  	shmid=shmget(key, BUFFER_SIZE, 0600|IPC_CREAT); // Cria o segmento de memória compartilhado

  	if(msgid == -1){
  		printf("Nao foi possivel criar a fila de mensagem.\n");
  		return 0;
  	}

  	if(shmid == -1){
  		printf("Nao foi possivel criar o segmento de memoria compartilhado.\n");;
  		return 0;
  	}


  	printf("Segmento de memoria e fila de mensagens criada. Servidor inicializado.\n");
  	clear_trash_in_memory_segment(shmid);
  	printf("Aguardando mensagens.\n");

  	do{
    	tem_mensagem = msgrcv(msgid,comando,sizeof(comando),0,0); // Recebe o comando na fila de mensagens
    	if(tem_mensagem > 0){
    		printf("Comando recebido: %s\n", comando);
    		client_pid = get_pid_from_message(comando);
    		command_id = get_command_id(comando);
    		printf("CLIENT PID: %d\n", client_pid);
    		printf("COMMAND ID: %d\n", command_id);

    		switch(command_id){ // Verifica qual comando é
    			case COMMAND_DATE:
					t = time(NULL);
					tm = localtime(&t);

					sprintf(response, "%d/%d/%d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
    			break;

    			case COMMAND_TIME:
					t = time(NULL);
					tm = localtime(&t);
					sprintf(response, "%d:%d:%d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    			break;

    			case COMMAND_NODENAME:
    				sprintf(response, "%s", uts.nodename);
    			break;

				case COMMAND_SYSNAME:
    				sprintf(response, "%s", uts.sysname);
				break;

				case COMMAND_RELEASE:
    				sprintf(response, "%s", uts.release);
				break;

				case COMMAND_VERSION:
					sprintf(response, "%s", uts.version);
				break;

				case COMMAND_MACHINE:
					sprintf(response, "%s", uts.machine);
				break;

				case COMMAND_PROCCESS:
					sprintf(response, "%d", info.procs);
				break;

				case COMMAND_FREERAM:
					sprintf(response, "%d -> %d Kb", info.freeram, info.freeram * info.mem_unit / 1024);
				break;

				case COMMAND_TOTALRAM:
					sprintf(response, "%d -> %d Kb", info.totalram, info.totalram * info.mem_unit / 1024);
				break;

				case COMMAND_UPTIME:
					days = info.uptime / 86400;
					hours = (info.uptime / 3600) - (days * 24);
					mins = (info.uptime / 60) - (days * 1440) - (hours * 60);
					sprintf(response, "%d -> %d days, %d hours, %d minutes, %ld seconds", info.uptime, days, hours, mins, info.uptime % 60);
				break;

				case COMMAND_PROCCESSORS:
					sprintf(response, "%d", get_nprocs());
				break;

    			default:
    				sprintf(response, "Comando invalido.");
    			break;
    		}
			send_response(response, client_pid, shmid);
    	}else if(tem_mensagem == -1){
    		printf("Erro na fila de mensagens.\n");
    		break;
    	}

  	}while(exit == 0);

  	printf("Servidor finalizado\n");
  	printf("\n");
  	return 0;
}


int get_pid_from_message(char *message){ // Obtem o pid de uma resposta
	int i, pid, buffer_counter = 0;;
	char buffer[10];
	for(i=0; i < strlen(message); i++){
		if(buffer_counter > 5) return -1;
		buffer[buffer_counter++] = message[i];
		if(message[i] == '|'){
			buffer[i] = '\0';
			break;
		}
	}
	pid = atoi(buffer);
	return pid;
}

int get_command_id(char *command){ // Obtem qual comando é

	int i;
	strtoupper(command);
	printf("%s\n", command);
	for(i=0; i < NMB_OF_COMMANDS; i++){
		if(strstr(command, command_suffix[i]) != NULL) return i;
	}

	return -1;
}

void send_response(char *response, int client_pid, int shmid){
	char final_response[BUFFER_SIZE];
	sprintf(final_response, "%d|", client_pid);
	strcat(final_response, response);
	printf("Escrevendo no segmento de memoria a resposta: %s\n", final_response);

	char *shm = shmat(shmid,0,0);
	if(shm != (char*)-1){
	    strcpy(shm, final_response);
	    printf("Escrito com sucesso.\n");
	}else{
		printf("Erro ao escrever no segmento de memoria.\n");
	}
    shmdt(shm);
}

void strtoupper(char *command){ // Capitaliza string
	int f = strlen(command);
	int i;

	for(i=0; i<f; i++){
		command[i] = toupper(command[i]);
	}
}

void clear_trash_in_memory_segment(int shmid){
	char *shm = shmat(shmid,0,0);
	if(shm != (char*)-1){
	    memset(shm, 0, BUFFER_SIZE);
	    printf("Memoria limpa.\n");
	}else{
		printf("Erro ao limpar o segmento de memoria.\n");
	}
    shmdt(shm);
}
