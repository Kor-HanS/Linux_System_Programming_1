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

	int newName_index = 0;
	for(int i = 0 ; i < argc; i++){
		if(strcmp(argv[i], "-n") == 0){
			newName_index = i;
			if(newName_index == argc-2){
				// exception : -n flag 를 받고, <NEWNAME> 을 받지 못함.
				fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
				exit(1);
			}
		}	
	}

	char* input_path = argv[1]; // 입력 받은 <FILENAME>
	char* input_path_new = ""; // 입력받은 -n flag 바로 뒤에 오는 <NEWNAME>
	if(newName_index >= 2){input_path_new = argv[newName_index+1];}

	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1] = {0}; // 백업 디렉토리 주소
	char absolute_path[MAX_FILE_PATH_LENGTH+1] = {0}; // <FILENAME>  절대 주소.
	char absolute_path_new[MAX_FILE_PATH_LENGTH+1] = ""; // <NEWNAME> 절대 주소. 
	char backup_path[MAX_FILE_PATH_LENGTH+1] = {0}; //  <FILENAME> 백업  주소.

	// 플래그 값 관련
	char option_char;
	int flag_md5=0,flag_sha1=0; // md5,sha1 해쉬 함수 플래그.
	int flag_d=0,flag_n=0; // -d,-n 플래그.

	// exception : 첫번째 인자 입력이 없음. (recover / hashfunction)
	if(argc <= 2){
		fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
		exit(1);
	}

	// 사용자의 홈 디렉토리 경로에 따른 새로운 백업 디렉토리 경로 설정.
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.

	// ssu_backup 에서 받은 어떤 해쉬 함수를 쓸지 결정. 
	if(strcmp(argv[argc-1],"md5") == 0){
		flag_md5 = 1;
	}else if(strcmp(argv[argc-1],"sha1") == 0){
		flag_sha1 = 1;
	}

	// -옵션 들을 getopt() 를 통해 가공.
	while((option_char = getopt(argc, argv, "dn")) != -1){
		switch(option_char){
			case 'd':
				flag_d = 1;
				break;
			case 'n':
				flag_n = 1;
				break;
			case '?':
				// exception : 올바르지 않은 옵션이 들어옴. 
				fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
				exit(1);
		}
	}

	char *update_path, *update_path_new;

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
	strcpy(absolute_path, update_path); // '~' 와 "." 와 ".." 를 가공한 새로운 경로.

	// <NEWNAME> 에 대한 절대 경로
	if(flag_n){
		//  '/' 와 '~' 로 시작 안함.  -> 상대경로 처리. 
		if(input_path_new[0] != '/' && input_path_new[0] != '~'){
			// 상대 경로는 '/' 로 시작 안하는 특징.
			strcpy(absolute_path_new, PATH_NOW_DIRECTORY);
			strcat(absolute_path_new, "/");
			strcat(absolute_path_new, input_path_new);
		}else{
			// 절대 경로.
			strcpy(absolute_path_new, input_path_new);
		}
		update_path_new = getProcessPath(absolute_path_new);
		strcpy(absolute_path_new, update_path_new); // '~' 와 "." 와 ".." 를 가공한 새로운 경로.
	}

	// exception : <FILENAME> 경로(절대 경로)가 4096 바이트 길이제한을 넘어가는 경우(에러 처리.)
	if(strlen(absolute_path) >= sizeof(absolute_path)){
		fprintf(stderr, "recover.c : first argument <FILENAME> is over 4096 Bytes\n");
		exit(1);
	}

	// exception : <NEWNAME> 경로(절대 경로)가 4096 바이트 길이제한을 넘어가는 경우(에러 처리.)
	if(flag_n){
		if(strlen(absolute_path_new) >= sizeof(absolute_path_new)){
			fprintf(stderr, "recover.c : -n argument <NEWNAME> is over 4096 Bytes\n");
			exit(1);
		}
	}
	
	// exception : <FILENAME> 홈 디렉토리위에 없거나, /backup 디렉토리 또는 하위 백업 파일을 포함하였을경우(입력의 절대경로)
	if(strncmp(absolute_path, PATH_HOME_DIRECTORY, strlen(PATH_HOME_DIRECTORY)) != 0 || strstr(absolute_path, PATH_BACKUP_DIRECTORY) != NULL){
		printf("<FILENAME> : <%s> can't be backuped\n",input_path);
		exit(0);
	}

	// exception : <NEWNAME> 홈 디렉토리위에 없거나, /backup 디렉토리 또는 하위 백업 파일을 포함하였을경우(입력의 절대경로)
	if(flag_n){
		if(strncmp(absolute_path_new, PATH_HOME_DIRECTORY, strlen(PATH_HOME_DIRECTORY)) != 0 || strstr(absolute_path_new, PATH_BACKUP_DIRECTORY) != NULL){
			printf("<NEWNAME> : <%s> can't be backuped\n",input_path_new);
			exit(0);
		}
	}

	// 백업 파일 주소.
	getBackupPath(absolute_path, backup_path, PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 경로 반환 함수.

	// <FILENAME> 에 해당하는 백업 파일 찾기.
	char backup_fname[MAX_FILE_PATH_LENGTH+1];
	strcpy(backup_fname, strdup(strrchr(backup_path,'/')+1));
	char backup_dname[MAX_FILE_PATH_LENGTH+1];// 백업 주소의 디렉토리 주소.
	strncpy(backup_dname, backup_path, strrchr(backup_path,'/')+1 - backup_path);

	DIR *dirp1 = NULL, *dirp2 = NULL; // 입력주소 디렉토리면 dirp1 != NULL dirp2 를 쓴다는것은 일단, 파일이라는것.
	
	// dirp2 를 열기 전, 백업 패스 바로 위 디렉토리 경로 없다면 ->  invalid path -> 입력이 디렉토리도 아니고, 백업파일을 찾기 위해 상위 디렉토리를 realpath() 했는데 NULL이 뜨는것이므로.
	// 접근 권한도 이미 백업한 파일에대해 있는것이고, 경로도 정확한것.
	if( (dirp1 = opendir(backup_path)) != NULL){
		if(realpath(backup_path, backup_path) == NULL){
			fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
			exit(1);
		}
	}else{
		if(realpath(backup_dname, backup_dname) == NULL){
			fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
			exit(1);
		}
		strcpy(backup_path, backup_dname);
		strcat(backup_path, "/");
		strcat(backup_path, backup_fname);
	}
	
	//파일일경우 어차피, realpath() 에서 찾을수없음. -> 백업 될때 _시간 을 달고 있기 때문. 
	// 그러므로, 정확한 경로 일경우. 자신을 가진 상위 디렉토리를 통해 경로를 얻고, 그 경로를 통해 홈위에 있는지 백업 파일을 포함하는지 체크.

	// 만약 dirp2로 열었다면 일단 , 백업파일을 가질수도 있는 디렉토리가 열린것. 백업파일_시간 이므로, 상위 디렉토리에서 dirent 를 통해 찾기.
	// 후에 아래에서 지우려는 파일과 일치하는 파일이 있는지 확인한후 파일 링크드리스트를 만들고.
	// 링크드리스트에 아무것도 없다면, 복원하고자 하는 백업 파일이 없다는것을 의미.
	// exception : 올바르지 않은 경로.
	if(dirp1 == NULL && (dirp2 = opendir(backup_dname)) == NULL){
		//  dirp1 이 NULL 이 아닌것은, 확실히 백업된 디렉토리가 존재하는것. dirp2에 경우는 stat() 으로 해당하는 파일을 한번 찾아야함.
		fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
		exit(1);
	}

	// exception : 첫번째 인자가 디렉토리인 경우 '-d'옵션을 사용하지 않았다면 에러처리.
	if(dirp1 != NULL && flag_d == 0){
		fprintf(stderr, "%s is a directory file\nbut, no -d flag\n",absolute_path);
		exit(1);
	}

	Node *head = NULL;
	Node_f *head_f = NULL;

	// directory recover
	if(dirp1 != NULL){
		push_back(&head,dirp1,backup_path);
	}
	
	// file recover 
	// dirp2 가 NULL 이 아닌경우는, 파일을 recover 해야되는것이고, 해당 파일 리스트중 bakcup_fname 에 해당하는 파일이 없다면 에러 처리.
	int fileCount = 0; 
	struct stat statbuf_f;
	struct dirent *dentry; // 백업 경로 의 상위 디렉토리의 파일 리스트 정보.

	if(dirp2 != NULL){
		while((dentry = readdir(dirp2)) != NULL){
			if(dentry->d_name == "." || dentry->d_name == ".."){continue;}
			if(dentry->d_type != DT_REG ){continue;}

			if(strncmp(dentry->d_name, backup_fname, strlen(backup_fname)) == 0 && dentry->d_name[strlen(backup_fname)] == '_'){
				// 백업 파일이 있는 디렉토리 내에서 백업 파일 찾기.
				char temp_path[MAX_FILE_PATH_LENGTH+1];
				strcpy(temp_path, backup_dname);
				strcat(temp_path,"/");
				strcat(temp_path,dentry->d_name);
				if(stat(temp_path,&statbuf_f) < 0){
					fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
					exit(1);
				}
				push_back_f(&head_f, statbuf_f, temp_path);
				fileCount++;
			}
		}	
	}
	
	// head와 head_f 에 노드를 채우고, recover 연산 시작.
	if(head == NULL && head_f == NULL){
		// recover 할 파일이나 디렉토리가 없음.
		fprintf(stderr,"Usage : recover <FILENAME> [OPTION]\n  -d : recover directory recursive\n  -n <NEWNAME> : recover file with new name\n");
		exit(1);
	}else{
		// recover 할 파일이나 디렉토리가 있음.
		if(dirp1 != NULL){
			// 디렉토리 recover
			recoverFiles(&head, flag_md5, flag_sha1, flag_n, absolute_path_new);
		}else if(dirp1 == NULL && dirp2 != NULL){
			// 파일 recover
			recoverFile(&head_f, flag_md5, flag_sha1, absolute_path_new);
		}
	}

	exit(0);
}
