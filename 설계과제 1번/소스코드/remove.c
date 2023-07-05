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

	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1] = {0}; // 백업 디렉토리 주소 홈디렉토리/backup
	char* input_path = argv[1]; // 입력받은 <FILENAME>
	char relative_path[MAX_FILE_PATH_LENGTH+1] = {0}; // 입력의 상대주소
	char absolute_path[MAX_FILE_PATH_LENGTH+1] = {0}; // 입력의 절대주소
	char backup_path[MAX_FILE_PATH_LENGTH+1] = {0}; // 백업 파일 리스트 주소.

	// 플래그 값 관련
	char option_char;
	int flag_md5=0,flag_sha1=0; // md5,sha1 해쉬 함수 플래그.
	int flag_a=0,flag_c=0; // -a,-c 플래그.

	// exception : 첫번째 인자 입력이 없음. (remove / hashfunction)
	if(argc <= 2){
		fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
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
	while((option_char = getopt(argc, argv, "ac")) != -1){
		switch(option_char){
			case 'a':
				flag_a = 1;
				break;
			case 'c':
				flag_c = 1;
				break;
			case '?':
				// exception : 올바르지 않은 옵션이 들어옴. 
				fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
				exit(1);
		}
	}

	// exception : -a 옵션과 -c 옵션을 동시에 받음.
	if(flag_a && flag_c){
		fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
		exit(1);
	}
	
	// exception : -c 옵션 사용시 다른 인자를 입력.
	if(flag_c && argc != 3){
		// remove -c hashfunction(md5 or sha1) 
		fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
		exit(1);
	}

	// 만약 -c 옵션을 받았다면, backup 디렉토리 처리. 이때, 홈디렉토리/backup은 지우면 안됨.
	if(flag_c){
		removeAll(flag_c);
		exit(0);
	}

	// 상대경로와 절대 경로 처리. getenv("HOME") 이 앞에 안 붙은 문자열 -> 상대경로 처리. 
	// 만들어진 경로가 잘못되었다면, 1차적으로 dirp1 NULL , dirp2 NULL 이 아닐수있으나, 같은이름의 백업 파일이나 디렉토리 찾지 못하고 그대로 에러 처리 함.
	if(input_path[0] != '/' && input_path[0] != '~'){
		// 상대 경로는 '/' 로 시작 안하는 특징.
		strcpy(absolute_path, PATH_NOW_DIRECTORY);
		strcat(absolute_path, "/");
		strcat(absolute_path, input_path);
	}else{
		// 절대 경로.
		strcpy(absolute_path, input_path);
	}

	char *update_path = getProcessPath(absolute_path);
	strcpy(absolute_path, update_path);

	// exception : 첫 번째 입력받은 경로(절대 경로)가 4096 바이트 길이제한을 넘어가는 경우(에러 처리.) :
	if(strlen(absolute_path) >= sizeof(absolute_path)){
		fprintf(stderr, "remove.c : first argument(FILENAME) is over 4096 Bytes\n");
		exit(1);
	}
	
	// exception : 홈 디렉토리위에 없거나, /backup 디렉토리 또는 하위 백업 파일을 포함하였을경우(입력의 절대경로)
	if(strncmp(absolute_path, PATH_HOME_DIRECTORY, strlen(PATH_HOME_DIRECTORY)) != 0 || strstr(absolute_path, PATH_BACKUP_DIRECTORY) != NULL){
		printf("<%s> can't be backuped\n",input_path);
		exit(0);
	}

	// 백업 파일 주소.
	getBackupPath(absolute_path, backup_path, PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 경로 반환 함수.
	char backup_fname[MAX_FILE_PATH_LENGTH+1];
	strcpy(backup_fname, strdup(strrchr(backup_path,'/')+1));
	char backup_dname[MAX_FILE_PATH_LENGTH+1];// 백업 주소의 디렉토리 주소.
	strncpy(backup_dname, backup_path, strrchr(backup_path,'/')+1 - backup_path);

	DIR *dirp1 = NULL, *dirp2 = NULL; // 입력주소 디렉토리면 dirp1 != NULL ,
	
	// 백업 패스 바로 위 디렉토리 경로 없다면 ->  invalid path
	// 있다면 이 경로를 가지고, 다시 backup 경로를 재 설정함.
	// 이 경로를 가지고 , 다시 absolute_path 를 재 설정함.
	// 여기서 이미, 백업 디렉토리 및에 있는 디렉토리의 경로를 갖고 있는것이므로, 
	// 접근 권한도 이미 백업한 파일에대해 있는것이고, 경로도 정확한것.
	if( (dirp1 = opendir(backup_path)) != NULL){
		if(realpath(backup_path, backup_path) == NULL){
			fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
			exit(1);
		}
	}else{
		if(realpath(backup_dname, backup_dname) == NULL){
			fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
			exit(1);
		}
		strcpy(backup_path, backup_dname);
		strcat(backup_path, "/");
		strcat(backup_path, backup_fname);
	}


	//파일일경우 어차피, realpath() 에서 찾을수없음. -> 백업 될때 _시간 을 달고 있기 때문. 
	// 그러므로, 정확한 경로 일경우. 자신을 가진 상위 디렉토리를 통해 경로를 얻고, 그 경로를 통해 홈위에 있는지 백업 파일을 포함하는지 체크.
	// 위에 연산을 통해 얻은 backup_path 를 통해 만든 absolute_path 는 원본 파일이 없어도, 만들어 낼수있음. -> 즉, 백업 파일은 존재하지만, 원본이 없어도 정확한 absolute_path 를 만들수있음.

	// 만약 dirp2로 열었다면 일단 , 백업파일을 가질수도 있는 디렉토리가 열린것.
	// 후에 아래에서 지우려는 파일과 일치하는 파일이 있는지 확인한후 파일 링크드리스트를 만들고.
	// 링크드리스트에 아무것도 없다면, 삭제할 백업파일이 없음을 의미.
	if(dirp1 == NULL && (dirp2 = opendir(backup_dname)) == NULL){
		//  exception : 올바르지 않은 경로. 후에, stat()으로 더블 체킹 할것임.
		//  dirp1 이 NULL 이 아닌것은, 확실히 백업된 디렉토리가 존재하는것. dirp2에 경우는 stat() 으로 해당하는 파일을 한번 찾아야함.
		// 없다면 Usage 출력.
		fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
		exit(1);
	}

	// exception : 첫번째 인자가 디렉토리인 경우 '-a'옵션을 사용하지 않았다면 에러처리.
	if(dirp1 != NULL && flag_a == 0){
		fprintf(stderr, "%s is a directory file\nbut, no -a flag\n",absolute_path);
		exit(1);
	}

	Node *head = NULL;
	Node_f *head_f = NULL;

	// directory remove
	if(dirp1 != NULL){
		push_back(&head,dirp1,backup_path);
	}
	
	// file remove 
	// dirp2 가 NULL 이 아닌경우는, 파일을 remove 해야되는것이고, 해당 파일 리스트중 bakcup_fname 에 해당하는 파일이 없다면 에러 처리.
	int fileCount = 0,removeNum = 0; 
	struct stat statbuf_f;
	struct dirent *dentry; // 백업 경로 의 상위 디렉토리의 파일 리스트 정보.

	if(dirp2 != NULL){
		while((dentry = readdir(dirp2)) != NULL){
			if(dentry->d_name == "." || dentry->d_name == ".."){continue;}
			if(dentry->d_type != DT_REG ){continue;}

			if(strncmp(dentry->d_name, backup_fname, strlen(backup_fname)) == 0 && strlen(dentry->d_name + strlen(backup_fname)) == 13){
				// 백업 파일이 있는 디렉토리 내에서 백업 파일 찾기.
				char temp_path[MAX_FILE_PATH_LENGTH+1];
				strcpy(temp_path, backup_dname);
				strcat(temp_path,"/");
				strcat(temp_path,dentry->d_name);
				if(stat(temp_path,&statbuf_f) < 0){
					fprintf(stderr,"remove.c : stat() error\n");
					exit(1);
				}
				push_back_f(&head_f, statbuf_f, temp_path);
				fileCount++;
			}
		}	
	}
	
	// head와 head_f 에 노드를 채우고, 삭제 연산 시작.
	if(head == NULL && head_f == NULL){
		// 삭제 할 파일이나 디렉토리가 없음.
		// 경로가 잘못 됐을경우 포함.
		fprintf(stderr,"Usage : remove <FILENAME> [OPTION]\n  -a : remove all file(recursive)\n  -c : clear backup directory\n");
		exit(1);
	}else{
		// 삭제 할 파일이나 디렉토리가 있음.
		if(dirp1 != NULL){
			// 디렉토리 삭제.
			int num1=0,num2=0;
			removeFiles(&head, flag_c, &num1, &num2);
		}else if(dirp1 == NULL && dirp2 != NULL){
			// 파일 삭제.
			if(fileCount == 1){
				// 삭제 할 백업 파일 1개
				removeFile(&head_f,flag_c,1);
			}else{
				// 삭제 할 백업 파일 2개이상.
				if(flag_a){
					// 백업 리스트에 모든 파일 지우기.
					for(int i = 1; i <= fileCount; i++){
						removeFile(&head_f,flag_c ,i);
					}
				}else{
					// flag_a 없이 백업 리스트 중 1개만 지울경우.
					printf("backup file list of \"%s\"\n",absolute_path);
					printf("0. exit\n");
					Node_f *temp_head = head_f;
					int printNum = 1;
					char *last_slash; 
					off_t fileSize[10];
					off_t fileS;
					int numCount;
					while(temp_head != NULL){
						numCount = 0;
						fileS = temp_head->nodeStat.st_size;
						printf("%d. %s",printNum++,strrchr(temp_head->nodePath,'_')+1);
						printf("\t\t");
					
						while(fileS != (off_t)0){
							fileSize[numCount++] = fileS % (off_t)1000;
							fileS /= (off_t)1000;
						}
	
						for(int i = numCount-1; i >= 0; i--){
							if(i == 0){
								printf("%03ld",fileSize[i]);
							}else if(i == numCount-1){
								printf("%ld,",fileSize[i]);
							}else{
								printf("%03ld,",fileSize[i]);
							}
						}
						printf("bytes\n");
						temp_head = temp_head->next;
					}
					char inputNum[MAX_INPUT_LENGTH+1];
					do{
						// 적절한 수가 들어올때까지 루프.
						printf("Choose file to remove\n>>");
						scanf("%d",&removeNum);
					
						char c; scanf("%c",&c); // 버퍼 제거.
					}while(removeNum < 0 || removeNum > fileCount);
					if(removeNum){
						removeFile(&head_f,flag_c,removeNum);
					}
				}
			}
		}
	}

	exit(0);
}
