#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define DEBUG 0
#define debug_print(args ...) if (DEBUG) fprintf(stderr, args)//usado para exibir mensagens de debug
#define LINE_QUA 10 // aceita ate 10 processos fixado previamente
#define CMD_QUA 50 // cada processo pode receber 50 argumentos
#define CMD_LEN 20 // os argumentos tem ate 20 caracteres
#define STD_BUFFER_SIZE 1024 //buffer padrao utilizado em diversas funcoes
//recebe o numero de processos, o vetor de comandos e operador correspondente, avalia cada caso e executa a acao correspondente
int execMatrix(int num_lines, char ***cmds, int *op);
//essa funcao e responsavel pela quebra do input em N processos e o retorno do numero do mesmo  
//recebe o input do teclado, um ponteiro para a matriz de comandos e outro para o array
int process_input(char *input, char ****commands_matrix, int **operator_array);
// executa a cadeia de pipes, mas com um buffer usado para passar a informacao adiante na cadeia de processos
int execPipeProcess(int NUM_PROCESS, char ***cmds, char* buffer);
//executa a cadeia de pipes 
void execPipe(int NUM_PROCESS, char***cmd2);
//responsavel pela execucao das funcoes &&, || e &
int execCmd(char **cmd, char *buffer);
pid_t typedef erro;
erro er=0;
int detectPso=0;


int main (int argc, char** argv) {

	char input[STD_BUFFER_SIZE];//input de teclado lê stdin 
	char ***cmds; //matriz de comandos a ser utilizada nos pipes
	char current_dir[50];// vetor que armazena posicao dentro da arvore de diretorios
	
	//vetor de operadores
	int *op, num_lines, wstatus;
	pid_t pid;

	while(1) {//loop infinito, para encerrar o processo ctrl+c
		getcwd(current_dir, 50); //pega o diretorio corrente
		printf("%s $ ", current_dir);//print do diretorio corrente
		fgets(input, STD_BUFFER_SIZE, stdin);//captura da string do teclado
		input[strcspn(input, "\n")] = 0;//aloca NULL na posicao em que o \n eh encontrado
		num_lines = process_input(input, &cmds, &op);
		switch ((pid=fork()))//cria um filho para averiguar o caso correspondente
		{
		case -1://se o valor retornado for negativo teve erro na criacao
			printf("Error creating process fork()");
			return -1;
		case 0: //estamos dentro do filho
			execMatrix(num_lines, cmds, op);//executa a matriz de comandos para averiguar
			return 0;
		default:
			if(op[num_lines - 2] != 4)
				waitpid(pid, &wstatus, 0); //controle sobre o caso &, se nao tiver & no final o processo pai nao vai esperar
			break;
		}
	}
}


int execMatrix(int num_lines, char ***cmds, int *op) {
	
	int status, pipe_count, *pipe_aux;
	char *out_buffer = malloc(sizeof(char) * STD_BUFFER_SIZE);
	// essa funcao eh responsavel pelo caso em que temos comandos simples
	status = execCmd(cmds[0], out_buffer);//comeca a execucao do primeiro comando
	for(int i = 1; i < num_lines; i++) {//
		debug_print("\n-----------start---------\n");
		debug_print("Analizing op[%d] = %d\n", i - 1, op[i - 1]);
		switch (op[i - 1])//recebe a operacao a ser realizada
		{
		case 1:  // pipe
			debug_print("Previous status = %d\n", status);
			debug_print("Pipe found on op[%d]\n", i - 1);
			pipe_count = 0; //inicia  contador do numero de pipes pegos
			pipe_aux = op + i;//percorre os pipes para executar os processos 
			for(pipe_aux = op + i - 1; *pipe_aux == 1; pipe_aux++) pipe_count++;//conta os pipes
			debug_print("pipes: %d\n", pipe_count);
			status = execPipeProcess(pipe_count, cmds + i, out_buffer);//tenta executar o comando vigente na linha de pipes
			debug_print("Pipe returned with %d status on %d operator.\n", status, i - 1);
			i += pipe_count - 1; //itera i para poder ir ao proximo pipe
			break;
			
		case 2:  // and avalia se o filho previo consegue executar
			debug_print("Previous status = %d\n", status);
			if(status != 0) {
				debug_print("And broken due to status %d\n", status); // erro no irmao anterior
				break;
			}
			printf("%s", out_buffer);
			status = execCmd(cmds[i], out_buffer);
			break;
		case 3:  // or 
			debug_print("Previous status = %d\n", status);
			if(status == 0) {
				printf("%s", out_buffer);
				memset(out_buffer, 0, strlen(out_buffer)); //zera o buffer para o proximo filho 
				break;
			}
			status = execCmd(cmds[i], out_buffer);
			break;
		case 4:  // bg
			break;
		default:
			execCmd(cmds[i], out_buffer); // por padrao tenta executar o comando 
			break;
		}
		debug_print("------------end----------\n");
	}
	printf("%s", out_buffer);

	return status;
}

//responsavel por executar comandos simples e usando tanto nos casos 1, 3 e 4
int execCmd(char **cmd, char *buffer) {
	int exec_status, fd[2];

	pipe(fd);
	switch (fork())
	{
	case -1:
		return -1;
	case 0:
		debug_print("EU SOU O FILHO PARA O COMANDO %s\n", cmd[0]);
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		debug_print("Executando %s...\n", cmd[0]);
		debug_print("Morrendo %s...\n", cmd[0]);
		exec_status = execvp(cmd[0], cmd);
		printf("o comando >%s< nao eh reconhecido\n",cmd[0]);//alerta um erro no comando especifico
		exit(exec_status);
	default:
		close(fd[1]);//fecha o descritor de escrita do processo usado 
		waitpid(-1, &exec_status, 0);//pai espera o filho
		memset(buffer, 0, strlen(buffer));//zera a variavel de buffer compartilhado
		ssize_t sz = read(fd[0], buffer, STD_BUFFER_SIZE);//realiza a leitura do buffer anterior (processo ant)
		debug_print("Lidos %li bytes do comando \"%s\"\n", sz, cmd[0]);
		close(fd[0]);//fecha descritor
		return exec_status;
	}
	return 0;
}


int execPipeProcess(int NUM_PROCESS, char ***cmds, char* buffer) {
	int fd[2][2], status;
	pid_t pid;


	pipe(fd[0]);
	pipe(fd[1]);
	switch ((pid=fork()))
	{
	case -1:
		return -1;
	case 0:
		char in_buffer[STD_BUFFER_SIZE];
		ssize_t n;
		debug_print("FILHO PIPE-PROCESS CRIADO\n");

        //lendo o pai
		close(fd[0][1]);
		dup2(fd[0][0], STDIN_FILENO);
		close(fd[0][0]);
        // memset(in_buffer, 0, strlen(in_buffer));
		// n = read(STDIN_FILENO, in_buffer, STD_BUFFER_SIZE);
		// debug_print("O filho leu %li bytes: \n%s\n", n, in_buffer);

        //abrindo escrita no pai
        close(fd[1][0]);
        dup2(fd[1][1], STDOUT_FILENO);
        close(fd[1][1]);

        // char *cc[] = {"grep", "or", NULL};
        // debug_print("Executando %s %s %s\n", cc[0], cc[1], cc[2]);
        // execvp(cc[0], cc);
		// printf("---Pipe Exec Simulation---\n");
        execPipe(NUM_PROCESS, cmds);
		if(er!=0){
			printf("erro no comando do filho:%d\no comando:>%s< nao eh reconhecido\n", er, cmds[detectPso][0]);
			return 0;
		}
		debug_print("nao chega aqui------------------------------------");
		exit(1);
	default:
		debug_print("SOU O PAI DO PIPE-PROCESS\n");
        //escrevendo no filho
		close(fd[0][0]);
		write(fd[0][1], buffer, strlen(buffer));
        close(fd[0][1]);

        //lendo no filho
        close(fd[1][1]);
        waitpid(pid, &status, 0);
        memset(buffer, 0, strlen(buffer));
        read(fd[1][0], buffer, STD_BUFFER_SIZE);
        close(fd[1][0]);
        debug_print("Final Pipe exit code: %d\n", status);
		return(status);
        // printf("o filho retornou:\n%s--------\n", buffer);
		break;
	}
    return 0;
}


void execPipe(int NUM_PROCESS, char***cmd2) {
	int pipes[NUM_PROCESS][2];//FD dos PIPES que serao herdados
	int status,i,j; 
	//printf("argv [1]:%s\n", argv[1]);
	for(j=0;j<NUM_PROCESS-1;j++){ // cmd1 | cmd2 | cmd3
		if(pipe(pipes[j])<0){ //abre o numero de pipes para NUM process
			printf("deu merda na criacao dos pipes");
		}
	}
	pid_t pid[NUM_PROCESS];//aloca em vetor de pid
	for(i=0;i<NUM_PROCESS;i++){//PERCORRE os processos 
		
		if((pid[i]=fork())<0){
			printf("Erro criacao PROCESSO\n");
			exit(-1);
		}
	
		 // processo1 ==> processo2 ==> processo3
		if(pid[i]==0){//checa o filho atual
			
			if(i==0 && NUM_PROCESS > 1) dup2(pipes[i][1], STDOUT_FILENO);//muda descritor de saida para o pipe
				
			if(i>0 && i<NUM_PROCESS-1){//processo intermediarios sempre vao ler no pipe anterior e escrever no posterior
				dup2(pipes[i][1], STDOUT_FILENO);//muda descritor de saida para o pipe
				dup2(pipes[i-1][0], STDIN_FILENO);//aloca o descritor padrao do processo stdin, inicialmente teclado para ler do pipe
			}

			if(i==NUM_PROCESS-1) dup2(pipes[i-1][0], STDIN_FILENO); //ultimo processo somente le
			//fecha todos os descritores, visto que nao tem mais nenhuma comunicacao pendente
			for(j=0;j<NUM_PROCESS-1;j++){
				close(pipes[j][0]);
				close(pipes[j][1]);
			}

			debug_print("PipeExecutando: %s\n", cmd2[i][0]);
			int sts = execvp(cmd2[i][0],cmd2[i]);
			er=getpid();//pega o ID do processo que gerou erro
			detectPso=i;//pega a posicao no indice do processo
			printf("erro no comando do filho:%d\no comando:>%s< nao eh reconhecido\n", er, cmd2[detectPso][0]);
			exit(sts);//encerra o filho com o processo se der problema
		}
	}
	//fecha todos os descritores que o processo de chamada criou para seus filhos usarem
	for(j=0;j<NUM_PROCESS-1;j++){
		close(pipes[j][0]);
		close(pipes[j][1]);
	}	
	
	int aux = 0;
	//manda o processo que criou os filhos esperar pelas execuções dele, se nao pode dar problema na criacao dos processos
	for(i=0;i<NUM_PROCESS;i++) {
		aux = waitpid(pid[i], &status, 0);
		debug_print("Pipe exit code: %d\n", status);
		if(status != 0) exit(-1);
	}
	exit(0);
}


int process_input(char *input, char ****commands_matrix, int **operator_array) {
    int cmd_count=0; //contador strings do comando
    char **aux_vector = malloc(sizeof(char*) * CMD_QUA);//recebe ate 50 argumentos
    char *aux; //ponteiro auxiliar strtok
    char ***cmds = malloc(sizeof(char**) * LINE_QUA); //aloca uma matriz para receber ate 10 processos
    int *op = malloc(sizeof(int) * LINE_QUA);//vetor de operacoes para receber a operacao de ate 10 processos
	//--- processo 0, operador[0]= 0 / ---processo 1, operador[1]=0 ... 
    for(int i = 0; i < LINE_QUA; i++) {
        cmds[i] = malloc(sizeof(char*) * CMD_QUA); //inicializa os 10 processos 
        op[i] = 0;//cada processo recebe um operar a sua posicao, eh usado para definir o tipo de operacao
    }
	//nosso token eh o espaco em branco
    aux = strtok(input, " ");
    for(int i = 0; aux != NULL; i++) {
        aux_vector[i] = malloc(sizeof(char) * CMD_LEN);//aloca 20 caracteres como vetor auxiliar
        strcpy(aux_vector[i], aux);// para cada posicao do vetor auxiliar, ele aloca o uma parte da string de input
        aux = strtok(NULL, " ");//executa o proximo token
        cmd_count++; //contabiliza mais um comando a ser feito
    }


    int i, lin, col;
    for(i = 0, lin = 0, col = 0; i < cmd_count; i++){
        if(strcmp(aux_vector[i], "|") == 0) {
            cmds[lin][col] = NULL;//marcamos como null a posicao do operador para ser utilizada como uma nova matriz de comandos
            op[lin] = 1; //marcamos que foi visto um | 
            lin++; //incrementamso para poder acionar o novo processo
            col = 0; //zeramos a contagem do numero de comando + argumentos
        }else if(strcmp(aux_vector[i], "&&") == 0) {
            cmds[lin][col] = NULL;
            op[lin] = 2;
            lin++;
            col = 0;
        }else if(strcmp(aux_vector[i], "||") == 0) {
            cmds[lin][col] = NULL;
            op[lin] = 3;
            lin++;
            col = 0;
        }else if(strcmp(aux_vector[i], "&") == 0) {
            cmds[lin][col] = NULL;
            op[lin] = 4;
            lin++;
            col = 0;
        }else{//nesse caso nenhum operador foi visto e podemos adiconar os elementos 
            cmds[lin][col] = malloc(sizeof(char) * CMD_LEN);//criamos o espaco da string a ser recebida
            strcpy(cmds[lin][col], aux_vector[i]);
            col ++;
        }
    }
	//caso tenha operadores
    cmds[lin][col] = NULL;/// marcamos a ultima posicao da matriz de arumentos como null para usar em execvp

    // char *aux2;
    // for(int i = 0; i < lin + 1; i++){
    //     aux2 = cmds[i][0];
    //     for(int j = 1; aux2 != NULL; j++) {
    //         printf("%s ", aux2);
    //         aux2 = cmds[i][j];
    //     }
    //     printf("\n");
    // }
	//aponta para a posicao inicial do vetor de matriz de argumentos
    *commands_matrix = cmds;
    *operator_array = op; //aponta para a posicao inicial do vetor de operadores
    return lin + 1; //lin + 1, porque o numero de processos eh de operadores + 1
}
