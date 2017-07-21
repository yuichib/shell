#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#define NOREDIRECT 'a'
#define PIPE_WRITE 0x00000001
#define PIPE_READ  0x00000010
#define PIPE_INIT  0x00000000
#define PIPE_MAX 10
#define TRUE 1
#define FALSE 0

void setArgs(char *com,char *ppargv[],int *argc,char *redirect,
		char **filename,int *pipe_locate,int *pipe_num);

void mywait(int sig);

//global var
int bg_flag;
int now_wait;

int main(int argc,char *argv[],char *envp[]){

	pid_t pid;
	int status;
	char *myargv[BUFSIZ];
	char command[BUFSIZ];
	char redirect;
	char *filename;
	int pipe_locate[PIPE_MAX];
	int pipe_num;
	int pipe_fd[PIPE_MAX * 2];
	int pipe_stat;
	int myargc;


	printf("/********author:yuichi bando*********/\n");
	printf("/********number:60618761*************/\n");
	printf("/********version:1.0*****************/\n");
	printf("/***if you want to end this system,please input [exit].***/\n");
	printf("\n");

	//signal
	signal(SIGINT,SIG_IGN);//Ctrl-C
	signal(SIGTSTP,SIG_IGN);//Ctl-Z
	signal(SIGCHLD,mywait);

	int i;//common "for loop"
	while(1){//while_start

		now_wait = FALSE;
		fprintf(stderr," $");//prompt
		if(fgets(command,BUFSIZ,stdin) == NULL){
			if(feof(stdin)){
				fprintf(stderr,"EOF pushed\n");
			}
			else{
				fprintf(stderr,"fgets() error!\n");
			}
			continue;
		}

		setArgs(command,myargv,&myargc,&redirect,&filename,pipe_locate,&pipe_num);
		char *envpath = getenv("PATH");//for execve

		//error
		if(bg_flag == TRUE && (pipe_num !=0 || redirect != NOREDIRECT)){
			fprintf(stderr,"back ground erorr!\n");
			continue;
		}

		//only parent (cd,exit)
		if(myargv[0] != NULL){
			if(myargc == 1 && strcmp(myargv[0],"exit") == 0){
				free(myargv[0]);
				break;//end
			}
			if(strcmp(myargv[0],"cd") == 0){
				if(myargc != 1 && myargc != 2){
					fprintf(stderr,"format:$cd dir\n");
				}
				else{
					if(myargc == 1){
						if(chdir(getenv("HOME")) == -1){
							fprintf(stderr,"cd error!\n");
						}
					}
					else{
						if(strcmp(myargv[1],"~") == 0){
							myargv[1] = getenv("HOME");
						}
						if(chdir(myargv[1]) == -1){
							fprintf(stderr,"cd error!\n");
						}
					}
				}
				for(i=0; i<myargc; i++){
					free(myargv[i]);
				}
				continue;
			}
		}
		//only parent end


		//exist pipe(parent)
		int pipe_id;
		int start;int end;
		int write_pos;int read_pos;
		int stat[pipe_num + 1];//process_num == pipe_num + 1
		int pipe_pgid;int first_proc = TRUE;
		if(pipe_num != 0){//exist_pipe_start
			for(i=0; i<pipe_num; i++){
				if(pipe(&pipe_fd[i * 2]) < 0){//pipe_create
					fprintf(stderr,"pipe create error!\n");
					exit(-1);
				}
			}
			pipe_stat = PIPE_INIT;
			for(pipe_id=0; pipe_id<=pipe_num; pipe_id++){
				pid = fork();
				if(pid == 0)
					break;
				else if(first_proc == TRUE){
					pipe_pgid = pid;
					first_proc = FALSE;
				}
			}

			if(pid != 0){//parent
				for(i=0; i<pipe_num*2; i++)//close important!!
					close(pipe_fd[i]);
				for(i=0; i<=pipe_num; i++)//wait child proc
					wait(&stat[pipe_id]);
			}
			else{//child
				if(pipe_id == 0){//first
					pipe_stat = PIPE_WRITE;
					start = 0;
					end = pipe_locate[0];
					write_pos = pipe_fd[1];
					read_pos = pipe_fd[0];
					pipe_pgid = getpid();
				}
				else if(pipe_id == pipe_num){//last
					pipe_stat = PIPE_READ;
					start = pipe_locate[pipe_num-1];
					end = myargc;
					write_pos = pipe_fd[pipe_num*2-1];
					read_pos = pipe_fd[pipe_num*2-2];
				}
				else{//middle
					pipe_stat = (PIPE_READ | PIPE_WRITE);
					start = pipe_locate[pipe_id - 1];
					end = pipe_locate[pipe_id];
					write_pos = pipe_fd[pipe_id*2+1];
					read_pos = pipe_fd[pipe_id*2-2];
				}

			}
		}//exist_pipe_end

		//not_exist_pipe(parent)
		else{
			if((pid = fork()) != 0){//parent
				if(bg_flag == FALSE){
					now_wait = TRUE;
					waitpid(pid,&status,0);
				}
			}
		}//not_exist_pipe_end


		if(pid == 0){//child
			//signal
			signal(SIGINT,SIG_DFL);
			signal(SIGTSTP,SIG_DFL);
			//pgid(back ground)
			if(bg_flag == TRUE){
				if(setpgid(getpid(),getpid()) < 0)
					perror("setpgid");
			}
			//redirection
			if(redirect != NOREDIRECT){
				int fd;
				if(redirect == '>'){//write
					if((fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644)) < 0){
						perror("open");
						exit(-1);
					}
					close(1);//close stdout
					if(dup(fd) < 0){
						perror("dup");
						exit(-1);
					}
					close(fd);
				}
				else if(redirect == '<'){//read
					if((fd = open(filename,O_RDONLY)) < 0){
						perror("open");
						exit(-1);
					}
					close(0);//close stdin
					if(dup(fd) < 0){
						perror("dup");
						exit(-1);
					}
					close(fd);
				}
			}//redirect_end

			//if_pipe_exist(child)
			if(pipe_num != 0){
				if(setpgid(getpid(),pipe_pgid) < 0){//group id
					perror("setpgid");
					exit(-1);
				}
				if((pipe_stat & PIPE_WRITE) != 0){//write
					close(1);//stdout close
					if(dup(write_pos) < 0){
						perror("dup");
						exit(-1);
					}
				}
				if((pipe_stat & PIPE_READ) != 0){//read
					close(0);//stdin close
					if(dup(read_pos) < 0){//read
						perror("dup");
						exit(-1);
					}
				}
				/*close important!! */
				for(i=0; i<pipe_num*2; i++){
					close(pipe_fd[i]);
				}

				char *pipe_argv[BUFSIZ];
				int index = 0;
				for(i=start; i<end; i++)//copy
					pipe_argv[index++] = myargv[i];
				pipe_argv[index] = NULL;
				//execve
				if(*pipe_argv[0] == '/' || *pipe_argv[0] == '.'){
					if(execve(pipe_argv[0],pipe_argv,envp) < 0){
						fprintf(stderr,"unknwon such command %s\n",myargv[0]);
						exit(-1);
					}
				}
				while(1){
					char path[BUFSIZ];
					int n= 0;
					while(*envpath != ':' && *envpath != '\0'){
						path[n++] = *envpath++;
					}
					path[n++] = '/';
					path[n]   = '\0';
					strcat(path,pipe_argv[0]);
					execve(path,pipe_argv,envp);
					if(*envpath == '\0')
						break;
					envpath++;
				}
				fprintf(stderr,"unknwon such command %s\n",pipe_argv[0]);
				exit(-1);

			}//pipe_end

			//normal command(child)
			//execve
			if(*myargv[0] == '/' || *myargv[0] == '.'){
				if(execve(myargv[0],myargv,envp) < 0){
					fprintf(stderr,"unknwon such command %s\n",myargv[0]);
					exit(-1);
				}
			}
			while(1){
				char path[BUFSIZ];
				int n= 0;
				while(*envpath != ':' && *envpath != '\0'){
					path[n++] = *envpath++;
				}
				path[n++] = '/';
				path[n]   = '\0';
				strcat(path,myargv[0]);
				execve(path,myargv,envp);
				if(*envpath == '\0')
					break;
				envpath++;
			}
			fprintf(stderr,"unknwon such command %s\n",myargv[0]);
			exit(-1);

		}//child_end

		//only parent
		for(i=0; i<myargc; i++){//liberate
			free(myargv[i]);
		}
		if(filename != NULL){
			free(filename);
		}
		//only parent end

	}//whille_end

	return 0;
}





void setArgs(char *com,char **ppargv,int *argc,char *redirect,char **filename,
		int *pipe_locate,int *pipe_num)
{
	int i = 0;
	*argc = 0;
	*redirect = NOREDIRECT;//default
	*filename = NULL;
	*pipe_num = 0;
	bg_flag = FALSE;
	char *pmal;//for malloc

	while(com[i] != '\n'){

		while(com[i] == ' ' || com[i] == '\t'){//blank
			i++;
		}
		if(com[i] == '\n')
			break;//end

		else if(com[i] == '>' || com[i] == '<'){//redirect_start
			*redirect = com[i];
			i++;
			char tmp[BUFSIZ];
			int num = 0;
			while(com[i] == ' ' || com[i] == '\t'){
				i++;
			}
			while(com[i] != ' ' && com[i] != '\t' && com[i] != '\n'){
				tmp[num++] = com[i++];
			}
			tmp[num] = '\0';
			if((pmal = (char *)malloc(sizeof(char) * num + 1)) == NULL){
				fprintf(stderr,"Can't allocate memory!!\n");
				exit(-1);
			}
			*filename = pmal;
			int j;
			for(j=0; j<=num; j++){
				*pmal++ = tmp[j];
			}
			break;
		}//redirect_end

		else if(com[i] == '|'){//pipe
			if(*argc == 0){
				fprintf(stderr,"pipe locate incorrect place\n");
				break;
			}
			*pipe_locate++ = *argc;
			(*pipe_num)++;
			i++;
			continue;
		}
		else if(com[i] == '&'){//bg
			bg_flag = TRUE;
			i++;
			continue;
		}
		else{//command_start
			char tmp[BUFSIZ];
			int num = 0;
			while(com[i] != ' ' && com[i] != '\t' && com[i] != '\n'
				&& com[i] != '<' && com[i] != '>' &&com[i] != '|' && com[i] != '&'){//command
				tmp[num++] = com[i++];
			}

			tmp[num] = '\0';
			if((pmal = (char *)malloc(sizeof(char) * num + 1)) == NULL){
				fprintf(stderr,"Can't allocate memory!!\n");
				exit(-1);
			}
			*ppargv++ = pmal;
			(*argc)++;
			int j;
			for(j=0; j<=num; j++){
				*pmal++ = tmp[j];
			}
		}//command_end

	}//while_end

	*ppargv = NULL;

}//setArgs_end


void
mywait(int sig)
{
	if(now_wait == FALSE){
		int status;
		wait(&status);
	}

}








