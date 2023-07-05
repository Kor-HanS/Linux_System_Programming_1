#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include "ssu_backup_u.h"
#include "header.h"

int main(int argc, char *argv[]){

	// PATH_BACKUP_DIRECTORY : getenv("HOME")/backup 
	// 사용자의 홈 디렉토리에 따른 backup directory 경로 설정.
	char input[MAX_INPUT_LENGTH+1];
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY);

	pid_t pid;
	int status;
	DIR *backup_dirp;
	
	// 만약 ssu_backup 실행시 해시 함수 인자로 <md5 | sha1> 을 못 받았을경우, Usage 출력
	if(argc != 2 ){
		fprintf(stderr, "Usage: ssu_backup <md5 | sha1>\n");
		exit(1);
	}else if(strcmp(argv[1], "md5") != 0 && strcmp(argv[1],"sha1") != 0){
		fprintf(stderr, "Usage: ssu_backup <md5 | sha1>\n");
		exit(1);
	}else{
		// ssu_backup 실행시, 홈디렉토리/backup 디렉토리가 존재 하지 않을시, 새로 백업 디렉토리 생성
		if((backup_dirp = opendir(PATH_BACKUP_DIRECTORY)) == NULL && mkdir(PATH_BACKUP_DIRECTORY, 0775) == -1){
			fprintf(stderr, "ssu_backup.c : opendir() && mkdir() error\n");
			exit(1);
		}

		while(1){
			printf("20192392>");
			fgets(input, sizeof(input), stdin);
			input[strlen(input)-1] = '\0';	// 개행 문자 제거후 널 문자로 교체.

			char *token;
			char *args[MAX_ARGS] = {"./help", NULL}; // 기본 execv() 의 args 는 help -> 지정된 명령어 외에 입력은 모두 usage 출력 
			
			// strtok()을 활용하여, 입력 받은 input 문자열을 빈칸으로 잘라 args 2차원 배열에 저장.
			token = strtok(input, " ");
			int tokenIndex = 0;
			while(token != NULL && tokenIndex < MAX_ARGS-1){
				args[tokenIndex++] = token;
				token = strtok(NULL, " ");
			}
			
			// 명령어 처리.
			if(strcmp(args[0], "exit") == 0){
				// exit 
				break;
			}

			if((pid = fork()) < 0){
				fprintf(stderr, " ssu_backup.c : fork error\n"); // ssu_backup.c fork() 에러 처리.
				exit(1);
			}
			else if(pid == 0){ 
				// 공통적으로, 모든 입력에 마지막에 ssu_backup 에서 사용할 해시 함수를 exec() args 의 마지막 인자로 넘겨줌.
				if(strcmp(args[0], "add") == 0){
					// add
					args[tokenIndex] = strcmp(argv[1],"md5")? "sha1" : "md5";
					args[tokenIndex+1] = NULL; 
					execv("./add",args);
				}
				else if(strcmp(args[0], "remove") == 0){
					// remove
					args[tokenIndex] = strcmp(argv[1],"md5")? "sha1" : "md5";
					args[tokenIndex+1] = NULL;
					execv("./remove",args);
				}
				else if(strcmp(args[0], "recover") == 0){
					// recover
					args[tokenIndex] = strcmp(argv[1],"md5")? "sha1" : "md5";
					args[tokenIndex+1] = NULL;
					execv("./recover",args);
				}
				else if(strcmp(args[0], "ls") == 0){
					// ls
					execv("/bin/ls",args);
				}
				else if(strcmp(args[0], "vi") == 0){
					// vi
					execv("/usr/bin/vi",args);
				}
				else if(strcmp(args[0], "vim") == 0){
					// vim
					execv("/usr/bin/vim",args);
				}
				else{
					if(!tokenIndex){
						// 개행 문자 처리(엔터 입력))
						exit(0);
					}
					args[0] = "help"; args[1] = NULL;
					// help : 그 외, 모든 
					execv("./help", args);
				}
			}
			else{
				// 부모 프로세스가 자식 프로세스의 종료를 기다림.
				waitpid(pid, &status, 0);
			}
		}
	}
		
	exit(0);
}
