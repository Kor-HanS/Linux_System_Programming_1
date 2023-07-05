#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include "header.h"
#include "ssu_backup_u.h"

void getBackupPath(char *absolute_path, char* backup_path ,char* PATH_BACKUP_DIRECTORY){
	char temp_path[MAX_FILE_PATH_LENGTH+1];
	strcpy(temp_path, PATH_BACKUP_DIRECTORY);
	strcat(temp_path, absolute_path + strlen(PATH_HOME_DIRECTORY));
	strcpy(backup_path, temp_path);
}

void getBackupDirectoryPath(char *PATH_BACKUP_DIRECTORY){
	strcpy(PATH_BACKUP_DIRECTORY, PATH_HOME_DIRECTORY);
	strcat(PATH_BACKUP_DIRECTORY, "/backup");
}

char* getProcessPath(char *path){
	char *token;
	char *path_token[MAX_ARGS];
	char *update_path = (char*)malloc(sizeof(char) * (MAX_FILE_PATH_LENGTH+1));
	int token_count = 0,update_path_index = 0;
	
	token = strtok(path, "/");
	while(token != NULL){
		path_token[token_count++] = token;
		token = strtok(NULL,"/");
	}
	for(int i = 0 ; i < token_count; i++){
		if(strcmp(path_token[i] , ".") == 0){
			continue;
		}else if(strcmp(path_token[i], "..") == 0){
			do{
				update_path_index--;
			}while( update_path_index >= 0 && update_path[update_path_index] != '/');

			if(update_path_index < 0){
				return "";
			}

		}else{
			if(i == 0 && strcmp(path_token[i], "~") == 0){
				// 맨 앞에 오는 '~' 처리.
				path_token[0] = PATH_HOME_DIRECTORY+1;
			}
			update_path[update_path_index++] = '/';
			strcpy(update_path+update_path_index, path_token[i]);
			update_path_index += (strlen(path_token[i]));
		}
	}
	
	update_path[update_path_index] = '\0';
	
	char *process_result = (char*)malloc(sizeof(char) * (MAX_FILE_PATH_LENGTH+1));
	strcpy(process_result, update_path);
	free(update_path);

	return process_result;
}

