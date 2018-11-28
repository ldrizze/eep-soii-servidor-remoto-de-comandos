#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <ctype.h>

#define RETRY_DELAY 1
#define NMB_OF_COMMANDS 12
#define BUFFER_SIZE 64

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

// Protótipos
int get_command_id(char *command); // Obter o id do comando
int send_command(unsigned short int command, int pid, int msgid); // Envia um comando para o servidor
void wait_for_response(int pid, int shmid, char *save_string); // Espera pela resposta no Segmento de memória compartilhada
int get_pid_from_response(char *response); // Obtem o pid de uma resposta
void get_message_from_response(char* response, char* save_string); // Obtem a mensagem de uma resposta
void strtoupper(char *command); // Altera a string para uppercase
int is_local_command(unsigned short int command);
void clear_trash_in_memory_segment(int shmid);


// Principal
int main(){
	pid_t pid = getpid();
	key_t key = ftok("/tmp", 'a');
	int shmid, msgid, id_comando;
	char comando[BUFFER_SIZE], resposta[BUFFER_SIZE];

	printf("Conectado ao servidor...\n");

	// Segmento de memória compartilhada
	do{
		shmid=shmget(key, BUFFER_SIZE, 0);
		if(shmid == -1){
			printf("Nao foi possivel estabelecer uma conexao com o segmento de memoria compartilhada. Postergando %ds.\n", RETRY_DELAY);
			sleep(RETRY_DELAY);
		}else{
			printf("SHMID: %d\n", shmid);
		}
	}
	while(shmid == -1);
	printf("Segmento de memoria encontrado.\n");

	// Fila de mensagens
	do{
  		msgid=msgget(key,0);

  		if(msgid == -1){
	  		printf("Nao foi possivel estabelecer uma conexao com a fila de mensagens. Postergando %ds.\n", RETRY_DELAY);
			sleep(RETRY_DELAY);
		}else{
			printf("MSGID: %d\n", msgid);
		}

	}while(msgid == -1);

	printf("Conexao estabelecida.\n");
	printf("\n");

	do{
		printf("$ ");
		fflush(stdin);
		gets(comando);
		id_comando = get_command_id(comando);

		if(id_comando == -1){
			printf("Comando invalido\n");
			continue;
		}

		if(is_local_command(id_comando) == -1){ // Se for comando remoto

			send_command(id_comando, pid, msgid); // Envia um comando
			wait_for_response(pid, shmid, resposta); // Espera pela resposta

			if(strstr(resposta, "SHUTDOWN") != NULL){ // Se for o comando shutdown
				printf("Executando o comando SHUTDOWN\n");
				break;

			}else{
				printf("%s\n", resposta);
			}


		}else{ // Se for comando local
			printf("Comando local\n");
		}

	}while(id_comando != 99);

	printf("Desconectado do servidor\n");
	printf("\n");
	return 0;
}


int get_command_id(char *command){ // Obtem qual comando é

	int i;
	strtoupper(command);
	for(i=0; i < NMB_OF_COMMANDS; i++){
		if(strstr(command, command_suffix[i]) != NULL) return i;
	}

	return -1;
}

int is_local_command(unsigned short int command){ // Retorna 0 se for comando local, senão -1

	if(command > 20){
		return 0;
	}

	return -1;

}

int send_command(unsigned short int command, int pid, int msgid){ // Envia um comando para o servidor
	char final_command[20];
	sprintf(final_command, "%d|", pid); // Salva o PID
	strcat(final_command, command_suffix[command]); // Concatena o comando
	return msgsnd(msgid, final_command, sizeof(final_command), 0); // Envia a mensagem
}

void wait_for_response(int pid, int shmid, char *save_string){ // Espera pela resposta no Segmento de memória compartilhada
	int received_pid = 0;
	char *rtn;
	do{
		char *shm = shmat(shmid,0,0);
		if(shm != (char*)-1){
			received_pid = get_pid_from_response(shm);
			if(received_pid == pid){
				get_message_from_response(shm, save_string);
				clear_trash_in_memory_segment(shmid);
    			shmdt(shm);
			}
		}else{
			printf("Nao ha resposta.\n");
		}

	}while(received_pid == 0);

}

int get_pid_from_response(char *response){ // Obtem o pid de uma resposta
	int i, pid, buffer_counter = 0;;
	char buffer[10];
	for(i=0; i < strlen(response); i++){
		if(buffer_counter > 5) return -1;
		buffer[buffer_counter++] = response[i];
		if(response[i] == '|'){
			buffer[i] = '\0';
			break;
		}
	}
	pid = atoi(buffer);
	return pid;
}

void get_message_from_response(char* response, char* save_string){ // Obtem a mensagem de uma resposta
	int i, counter = 0, f = 0;
	for(i=0; i < strlen(response); i++){
		if(f == 1){
			save_string[counter++] = response[i];
		}else if(response[i] == '|'){
			f = 1;
		}
	}
	save_string[counter] = '\0';
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
	}else{
		printf("Erro ao limpar o segmento de memoria.\n");
	}
    shmdt(shm);
}
