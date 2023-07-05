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
#include "ssu_backup_u.h" // utility header
#include "linked_list.h" // linked_list header
#include "header.h" // macro header

int main(int argc, char *argv[]){
	
	char *input_path = argv[1];
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	char relative_path[MAX_FILE_PATH_LENGTH+1] = {0}; // 상대 경로 
	char absolute_path[MAX_FILE_PATH_LENGTH+1] = {0}; // 절대 경로
	char backup_path[MAX_FILE_PATH_LENGTH+1] = {0}; // 백업 경로

	DIR *dirp;
	struct dirent *dentry;
	struct stat statbuf;

	int option_char; // 옵션 관리
	int flag_md5 = 0; // md5 flag
	int flag_sha1 = 0; // sha1 flag
	int flag_d = 0; // option -d flag

	// 사용자의 홈 디렉토리 경로에 따른 새로운 백업 디렉토리 경로 설정.
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수

	// ssu_backup 에서 받은 어떤 해쉬 함수를 쓸지 결정. 
	if(strcmp(argv[argc-1],"md5") == 0){
		flag_md5 = 1;
	}else{
		flag_sha1 = 1;
	}

	// -옵션 들을 getopt() 를 통해 가공.
	while((option_char = getopt(argc, argv, "d")) != -1){
		switch(option_char){
			case 'd':
				flag_d = 1;
				break;
			case '?':
				// invalid options 
				fprintf(stderr, "Usage : add <FILENAME> [OPTION]\n   -d : add directory recursive\n");
				exit(1);
		}
	}

	// exception : 인자를 1개만 받았을경우. argc 가 2인 것은, ssu_backup 을 통해 어떤 해쉬 함수를 사용할지 마지막 인자로 받있기 때문. 
	if(argc == 2){
		fprintf(stderr, "Usage : add <FILENAME> [OPTION]\n   -d : add directory recursive\n");
		exit(1);
	}

	char *update_path;
	// <FILENAME> 에 대한 절대 경로.
	//  '/' 와 '~' 로 시작 안함.  -> 상대경로 처리. 
	
	if(input_path[0] != '/' && input_path[0] != '~'){
		// 상대 경로는 '/' 로 시작 안하는 특징.
		strcpy(absolute_path, PATH_NOW_DIRECTORY);
		strcat(absolute_path, "/");
		strcat(absolute_path, input_path);
	}else{
		// 절대 경로.
		strcpy(absolute_path, input_path);
	}

	update_path = getProcessPath(absolute_path);
	memset(absolute_path,0,sizeof(absolute_path));
	strcpy(absolute_path, update_path); // '~' 와 "." 와 ".." 를 가공한 새로운 경로.


	// exception : 절대 경로로 변환후, stat() 을 통해 해당 경로의 정보를 받지 못함. stat() 에러.
	// stat() 에 경우는 심볼링 링크 파일의 경우 가리키는 원본 파일의 정보를 가져오고, 
	// realpath() 전에 놓음으로써, 심볼릭 링크 파일 path 를 realpath() 에 넣어, 원본에 절대 경로를 얻기전, 심볼릭 링크 경로 그 자체를 통해 파일 정보를 가져옴.
	// 그외, 파일의 경우는 자신의 파일 정보를 가져옴.
	if(lstat(absolute_path, &statbuf) < 0){
		fprintf(stderr, "Usage : add <FILENAME> [OPTION]\n   -d : add directory recursive\n");
		exit(1);
	}

	// realpath() 를 통해 절대 경로가 정확한 경로인지, 접근 권한 이 있는지 처리.
	if(realpath(absolute_path, absolute_path ) == NULL){
		if(errno == ENOENT){
			// invalid path(no file or directory)
			fprintf(stderr, "Usage : add <FILENAME> [OPTION]\n   -d : add directory recursive\n");
		}
		else if(errno == EACCES){
			// no access
			fprintf(stderr, "add.c : no access for the path of <%s>\n",input_path);
		}
		exit(1);
	}

	// exception : 첫 번째 입력받은 경로(절대 경로)가 4096 바이트 길이제한을 넘어가는 경우(에러 처리.)
	if(strlen(absolute_path) >= sizeof(absolute_path)){
		fprintf(stderr, "add.c : first argument(FILENAME) is over 4096 Bytes\n");
		exit(1);
	}
	
	// exception : 만약, 현재 경로가 일반 파일이나 디렉토리가 아닌 경우(에러 처리.)
	if((!S_ISREG(statbuf.st_mode) && !S_ISDIR(statbuf.st_mode))){
		fprintf(stderr,"%s is not a regular file or directory\n",input_path);
		exit(1);
	}

	// exception : 만약, 현재 경로가 디렉토리 이나, -d 옵션을 받지 못한 경우.(에러 처리)
	if(S_ISDIR(statbuf.st_mode) && !flag_d){
		fprintf(stderr,"\"%s\" is a directory file\n",input_path);
		exit(1);
	}

	// exception : 만약, 첫번째 인자(절대경로) 가  ~ (홈디렉토리) 를 벗어나거나 백업디렉토리 주소(홈디렉토리/backup)를 포함하는 경우. 
	// 표준 출력 printf() 후, 프롬프트 재출력.
	if(strncmp(absolute_path, PATH_HOME_DIRECTORY, strlen(PATH_HOME_DIRECTORY)) != 0 || strstr(absolute_path, PATH_BACKUP_DIRECTORY) != NULL){
		printf("<%s> can't be backuped\n",input_path);
		exit(0);
	}

	// 위에 예외 처리에 해당하지 않다면, 백업 진행.
	Node* head = NULL;
	pid_t pid;
	int status;
	DIR *backupDirp;

	if(S_ISDIR(statbuf.st_mode)){
		// 경로가 디렉토리 일경우, 재귀적으로 탐색하여 백업을 위해 linked_list를 통해 파일 리스트 관리.
		if((dirp = opendir(absolute_path)) == NULL){
			fprintf(stderr, "opendir error for %s\n",input_path);
			exit(1);
		}
		push_back(&head,dirp,absolute_path);
		backupFiles(&head, flag_md5, flag_sha1);
	}else{
		// 경로가 일반 파일 일경우, 일반 파일 백업.
		backupFile(absolute_path, flag_md5, flag_sha1);
	}

	exit(0);
}
