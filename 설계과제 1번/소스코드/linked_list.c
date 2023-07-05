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
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "ssu_backup_u.h" // utility header
#include "linked_list.h" // linked_list header
#include "header.h" // macro header

// add.c 에 사용할 backup function : 103 line
// remove.c 에 쓸 remove functions : 285 line
// recover.c 에 쓸 recover functions : 392 line

void push_front(Node **head, DIR *dirp, char path[]){
	Node *newNode = (Node*)malloc(sizeof(Node));
	newNode->nodeDirp = dirp;
	strcpy(newNode->nodePath,path);
	newNode->next = NULL;
	*head = newNode;
}

void push_back(Node **head, DIR *dirp, char path[]){
	Node *newNode = (Node*)malloc(sizeof(Node));
	newNode->nodeDirp = dirp;
	strcpy(newNode->nodePath,path);
	newNode->next = NULL;

	if(*head == NULL){
		*head = newNode;
	}else{
		Node *nowNode = *head;
		while(nowNode->next != NULL){
			nowNode = nowNode->next;
		}
		nowNode->next = newNode;
	}
}

void push_back_f(Node_f ** head, struct stat statbuf, char path[]){
	Node_f *newNode_f = (Node_f*)malloc(sizeof(Node_f));
	strcpy(newNode_f->nodePath,path);
	newNode_f->nodeStat = statbuf;
	newNode_f->next = NULL;

	if(*head ==NULL){
		*head = newNode_f;
	}else{
		Node_f *nowNode_f = *head;
		while(nowNode_f->next != NULL){
			nowNode_f = nowNode_f->next;
		}
		nowNode_f->next = newNode_f;
	}
}

void pop_front(Node **head){
	Node* prevHead = *head;
	if(*head != NULL){
		*head = prevHead->next;
		free(prevHead);
	}
}

void pop_front_f(Node_f **head){
	Node_f* prevHead = *head;
	if(*head != NULL){
		*head = prevHead->next;
		free(prevHead);
	}
}

void pop_back(Node **head){
	if(*head != NULL){
		Node *prev;
		Node *nowNode = *head;
		while(nowNode->next != NULL){
			prev = nowNode;
			nowNode = nowNode->next;
		}
		prev->next = NULL;
		free(nowNode);
	}
}

void insertNode(Node *prevNode, DIR *dirp, char path[]){
	Node* newNode = (Node*)malloc(sizeof(Node));
	newNode->nodeDirp = dirp;
	strcpy(newNode->nodePath,path);
	newNode->next = prevNode->next;
	prevNode->next = newNode;
}

void backupFiles(Node **head, int flag_md5, int flag_sha1){
	struct dirent *dentry; // 현재 노드의 파일 리스트를 얻기위한 포인터 구조체.
	struct stat statbuf; // stat() 을 통해 파일의 정보를 갖는 구조체.
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1]; // 사용자의 홈 디렉토리의 따른 backup directory 경로 설정.
	char filename[MAX_FILE_PATH_LENGTH+1]; // 백업하고 싶은 파일의 경로

	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.
	
	Node* nowNode = *head; // 입력으로 받은 이중 포인터 헤드를 노드 포인터에 nowNode로 선언.

	while(nowNode != NULL){
		// 현재 디렉토리의 하위 파일중 이름이 '.' 과 '..' 은 무시.
		while((dentry = readdir(nowNode->nodeDirp)) != NULL){
			if(strcmp(dentry->d_name, ".") == 0 || strcmp(dentry->d_name, "..") == 0){
				continue;
			}

			// 현재 디렉토리의 경로 + "/하위 파일 이름(dentry->d_name)" : 현재 탐색중인 파일의 원래 경로 
			memset(filename,0,sizeof(filename));
			strcpy(filename, nowNode->nodePath);
			filename[strlen(filename)] = '/';
			strcat(filename, dentry->d_name);
			
			// lstat() 을 통해 현재 탐색중인 파일의 정보 받기. - > stat() 이 아닌, lstat() 을 씀으로써, 링크 파일을 제대로 거를수있음.
			if(lstat(filename, &statbuf) < 0){
				// fprintf(stderr," backupFiles() : stat() error for %s\n",filename);
				continue;
			}
			
			// 현재 경로 접근 권한 여부 확인.
			char absolute_path[MAX_FILE_PATH_LENGTH+1];
			if(realpath(filename, absolute_path) == NULL){
				if(errno == EACCES){
					//fprintf(stderr, "backupFiles() : no access for the path of <%s>\n",filename);
				}else{
					//fprintf(stderr, "backupFiles() : realpath() error for %s errorno : %d\n",filename,errno);
				}
				continue;
			}

			if(!S_ISDIR(statbuf.st_mode) && !S_ISREG(statbuf.st_mode)){
				// fprintf(stderr, "%s is not a regular file or directory",filename);
				continue;
			}

			if(S_ISDIR(statbuf.st_mode)){
				// 만약 현재 탐색중인 파일이 디렉토리이다. -> 다음 탐색을 위해 링크드 리스트에 추가(BFS)
				// 이때 해당하는 디렉토리의 경로가 "홈디렉토리/backup" 일 경우는 패스 ,strstr() 을 통해 검사.
				if(strstr(filename, PATH_BACKUP_DIRECTORY) == NULL){
					push_back(head, opendir(filename),filename);
				}
			}else if(S_ISREG(statbuf.st_mode)){
				// 현재 탐색중인 파일이 일반 파일이다 -> 백업을 진행.
				backupFile(filename, flag_md5, flag_sha1);
			}
		}
		pop_front(head); // 현재 탐색이 완료된 Node 의 경우는 링크드 리스트에서 삭제.
		nowNode = nowNode->next; // 다음 노드로 이동.
	}
}

void backupFile(char *filepath, int flag_md5, int flag_sha1){
	pid_t pid;
	int status;
	DIR *backupDirp; // 백업하려는 파일의 바로 위 디렉토리 경로의 포인터 
	struct dirent *dentry; // 백업하려는 곳의 디렉토리의 파일리스트 -> 백업하려는 파일과 같은 이름의 파일이 같은 해쉬 값을 같는지 보기위해서.
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	char backup_path[MAX_FILE_PATH_LENGTH+1] = {0}; // filepath 의 파일을 백업할시, 사용할 백업 경로.

	time_t nowTime = time(NULL);
	struct tm *pLocal = localtime(&nowTime); // 백업시 활용할 _YYMMDDHHMMSS 에 쓸 구조체 선언.

	// 사용자의 환경 에 맞는 홈디렉토리/backup 경로를 백업폴더의 경로로 지정.
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.
	getBackupPath(filepath, backup_path, PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 경로 반환 함수.

	char *fnameptr = strrchr(backup_path, '/'); // 현재 파일명은 마지막 / 기준 뒤에 해당. 마지막 / 위치 포인터. 
	char *fname = fnameptr+1;  // 마지막 / 위치 포인터+1 의 문자열은 파일이름.
	char dname[MAX_FILE_PATH_LENGTH+1] = {0}; // filepath 에서 마지막 슬래시 기준으로 "/파일이름" 을 잘라 만든 백업 파일 바로 위 디렉토리 경로
	memset(dname, 0, sizeof(dname));
	strncpy(dname, backup_path, fnameptr - backup_path);

	// ~/backup 하위
	// 백업 하기 위한 파일을 넣을 상위 디렉토리가 없을시 access(현재 만들려는 디렉토리 주소, F_OK) 를 통해 검사 후,
	// 없다면 mkdir() 을 통해 디렉토리 생성.
	char* temp_dname = strdup(dname);
	strcpy(temp_dname, temp_dname);
	char* p = strtok(temp_dname, "/");
	char token_buf[MAX_FILE_PATH_LENGTH+1] = "";

	while(p){
		strcat(token_buf, "/");
		strcat(token_buf, p);

		if((access(token_buf, F_OK) != 0)){
			if(mkdir(token_buf, 0775) < 0){
				return;
			}
		}
		p = strtok(NULL, "/");
	}

	// 백업 하려는 곳의 디렉토리를 열어, 같은 파일이 있는지 검사.
	if((backupDirp = opendir(dname)) == NULL){
		return;
	}

	// 같은 해쉬 값을 가진 백업 파일이 존재한다면, is alreay backuped
	// 같은 해쉬 값을 가진 백업 파일이 존재하지 않다면, 그대로 백업 진행.
	int isSameBackupExist = 0;
	char compareFileName[MAX_FILE_PATH_LENGTH+1] = {0}; // 만약 백업 파일명이 백업하려는파일명_xxxxxxxxxxxx 이런 형태인지 검사.
	char sameBackupFileName[MAX_FILE_PATH_LENGTH+1] = {0}; // 만약 같은 해쉬값을 가진 백업파일이 존재시, 해당 문자열에 저장.
	unsigned char hash_SHA1[SHA_DIGEST_LENGTH] = {0}; // 백업 하려는 파일의 SHA1 해쉬값
	unsigned char hash_MD5[MD5_DIGEST_LENGTH] = {0}; // 백업 하려는 파일의 MD5 해쉬값
	getMd5Sum(filepath, hash_MD5);
	getSha1Sum(filepath, hash_SHA1);

	while(backupDirp != NULL && (dentry = readdir(backupDirp)) != NULL){
		// 같은 이름을 가진 백업파일 찾기 
		if(dentry->d_type != DT_REG){continue;} 
		if(strlen(dentry->d_name) < strlen(fname)){continue;}
		if(!strncmp(dentry->d_name, fname, strlen(fname))){
			strcpy(compareFileName, dname);
			strcat(compareFileName, "/");
			strcat(compareFileName, dentry->d_name);
			unsigned char temp_sha1Hash[SHA_DIGEST_LENGTH] = {0}; // 같은 이름을 가진 백업 파일의 SHA1 해쉬값
			unsigned char temp_md5Hash[MD5_DIGEST_LENGTH] = {0}; // 같은 이름을 가진 백업 파일의 MD5 해쉬값

			if(flag_md5 && !isSameBackupExist){ 
				// 백업하려는 파일과 같은 백업 파일을 찾지 못했고, ssu_backup 이 md5 를 사용할경우.
				getMd5Sum(compareFileName, temp_md5Hash);
				if(!memcmp(hash_MD5,temp_md5Hash,MD5_DIGEST_LENGTH)){
					isSameBackupExist = 1;
					strcpy(sameBackupFileName,compareFileName);
				}
			}else if(flag_sha1 && !isSameBackupExist){
				// 백업하려는 파일과 같은 백업 파일을 찾지 못했고, ssu_backup 이 sha1 을 사용할경우.
				getSha1Sum(compareFileName, temp_sha1Hash);
				if(!memcmp(hash_SHA1,temp_sha1Hash,SHA_DIGEST_LENGTH)){
					isSameBackupExist = 1;
					strcpy(sameBackupFileName,compareFileName);
				}
			}
		}
	}

	// 만약 현재 백업하려는 파일의 백업본이 있다면 백업 중지.
	if(isSameBackupExist){
		printf("\"%s\" is already backupded\n",sameBackupFileName);
		return;
	}
	
	// 만약 현재 백업하려는 파일의 백업본이 없다면 백업.
	// 백업 하기 위한 파일 명칭 새로 만들기.
	strcat(dname,"/");
	strcat(dname, fname);
	strcat(dname, "_");
	char timeString[13];
	sprintf(timeString, "%02d%02d%02d%02d%02d%02d",(pLocal->tm_year+1900)%100, pLocal->tm_mon+1, pLocal->tm_mday, pLocal->tm_hour, pLocal->tm_min, pLocal->tm_sec);
	strcat(dname, timeString);
	
	// 하위 프로세스를 통해 현재 파일을 -> 백업 경로에 파일명_현재시각 으로 백업.
	if((pid = fork()) < 0){
		fprintf(stderr, "backupFile : fork() error\n");
		return;
	}else if(pid == 0){
		// copy file to backup path 
		char *cpArgs[] = {"cp", filepath, dname, NULL};
		execvp(cpArgs[0], cpArgs);
		exit(0);
	}else{
		waitpid(pid,&status,0);
	}
	printf("\"%s\" backuped\n",dname);
}

void removeAll(int flag_c){

	if(!flag_c){
		return;
	}

	DIR *dirp; 
	struct dirent *dentry;
	int deleteF_count =0,deleteD_count = 0;	
	// directory node *head and file node *head_f
	Node *head = NULL;

	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.

	if((dirp = opendir(PATH_BACKUP_DIRECTORY)) == NULL){
		fprintf(stderr,"removeAll() : opendir error\n");
		exit(1);
	}
	push_back(&head, dirp, PATH_BACKUP_DIRECTORY);
	removeFiles(&head,flag_c,&deleteF_count, &deleteD_count);
	if(deleteF_count == 0 && deleteD_count == 0){
		printf("no file(s) in the backup\n"); // 백업 디렉토리에 아무 파일도 없을 경우.
	}else{
		printf("backup directory cleared(%d  regular files and  %d subdirectories totally).\n",deleteF_count, deleteD_count);
	}
}

void removeFiles(Node **head, int flag_c, int *deleteF_count, int *deleteD_count){
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.

	Node *nowNode = *head;
	DIR *dirp = nowNode->nodeDirp;
	char *path = nowNode->nodePath;

	if(dirp == NULL){
		fprintf(stderr, "removeFiles : opendir() error\n");
		exit(1);
	}

	struct dirent *dentry;
	struct stat statbuf;
	char temp_path[MAX_FILE_PATH_LENGTH+1];
	Node_f *head_f = NULL;
	
	while((dentry = readdir(dirp)) != NULL){
		if(strcmp(dentry->d_name,".") == 0 || strcmp(dentry->d_name,"..") == 0){
			continue;
		}

		memset(temp_path,0,sizeof(temp_path));
		strcpy(temp_path, path);
		if(temp_path[strlen(path)-1] != '/'){strcat(temp_path, "/");}
		strcat(temp_path, dentry->d_name);
		
		if(stat(temp_path, &statbuf) < 0){
			fprintf(stderr, "removeFiles : stat() error\n");
			exit(1);
		}
		
		if(dentry->d_type == DT_REG){
			push_back_f(&head_f, statbuf, temp_path);
			removeFile(&head_f,flag_c,1);
			(*deleteF_count) += 1;
			pop_front_f(&head_f);
		}

		if(dentry->d_type == DT_DIR){
			push_front(head, opendir(temp_path), temp_path);
			removeFiles(head, flag_c, deleteF_count, deleteD_count); // 재귀 호출로 하위 디렉토리 파일 및 디렉토리 삭제.a
		}
	}

	if(strcmp(path, PATH_BACKUP_DIRECTORY) != 0){
		(*deleteD_count) += 1;
		remove(path);
	}

	pop_front(head);
}

void removeFile(Node_f **head, int flag_c, int count){
	pid_t pid;
	int status;
	int now = 1;
	if((pid = fork()) < 0){
		fprintf(stderr, "removeFile : fork() error\n");
		exit(1);
	}else if(pid == 0){
		Node_f *temp_head = *head;
		while(temp_head != NULL){
			if(now == count){
				char *args[] = {"rm", temp_head->nodePath, NULL};
			
				if(!flag_c){printf("%s backup file removed\n",temp_head->nodePath);}
				execvp(args[0],args);
				exit(0);
			}
			now++;
			temp_head = temp_head->next;
		}
	}else{
		waitpid(pid, &status, 0);
	}
}

void recoverFiles(Node **head, int flag_md5, int flag_sha1, int flag_n, char* filename_new){
	// node 에 들어있는 backup_path 를 가공-하여서, filename_new에 붙이고. 
	// ex) /home/hanseung/backup/P1 -> /home/hanseung/backup/P1/file이름  -> filename_new/file이름
	
	// 백업 디렉토리 경로 받기.
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.
	
	Node *nowNode = *head;
	DIR *dirp = nowNode->nodeDirp;
	char *backup_path = nowNode->nodePath;

	if(dirp == NULL){
		fprintf(stderr, "removeFiles : opendir() error\n");
		exit(1);
	}

	struct dirent *dentry;
	struct stat statbuf, statbuf_new;
	char temp_path[MAX_FILE_PATH_LENGTH+1]; // 노드에 넣을때 쓸 <FILENAME>의 백업 경로.
	char temp_path_new[MAX_FILE_PATH_LENGTH+1]; // 함수 에 넣을 때 쓸 <NEWNAME>
	
	int dentry_length = 0;
	// diretn 구조체 길이 측정.
	while((dentry = readdir(dirp)) != NULL){
		dentry_length++;	
	}

	int visit[dentry_length];
	memset(visit,0,sizeof(visit));
	int now_dirpnum; // 현재 링크드리스트에서 가리키고 있는 번호.
	
	for(int i = 0 ; i < dentry_length; i++){
		if(visit[i]){continue;}
		rewinddir(dirp);
		visit[i] = 1;
		dentry = readdir(dirp);
		now_dirpnum = 0;
			
		while(now_dirpnum < i && (dentry = readdir(dirp)) != NULL){
			// 현재 자신보다 작은 번호들은 이미 처리 햇으므로, 넘기기.
			now_dirpnum++;
		}
		Node_f *head_f = NULL;
			
		if(strcmp(dentry->d_name,".") == 0 || strcmp(dentry->d_name,"..") == 0){
			// 현재 dirent 구조체의 d_name 이 . 이나 .. 인 경우 넘기기.
			continue;
		}

		// 새로운 백업 <FILENAME> 만들기.
		memset(temp_path,0,sizeof(temp_path));
		strcpy(temp_path, backup_path);
		if(temp_path[strlen(temp_path)-1] != '/'){strcat(temp_path, "/");}
		strcat(temp_path, dentry->d_name);
		if(stat(temp_path, &statbuf) < 0){
			fprintf(stderr, "recoverFiles : stat() error\n");
			exit(1);
		}
		
		if(dentry->d_type == DT_REG){
			push_back_f(&head_f, statbuf, temp_path);
			// 백업 파일중 _ 를 제외하고, 본인과 같은 이름을 가진 파일을 묶어서 링크드 리스트에 넣는다.
			// 이때 이미 한번 링크드 리스트로 만든 파일의 경우는 visit[자신의순번] = 1 로 함으로써, 1번만 검사하게 한다.
			
			char now_file_name[MAX_FILE_PATH_LENGTH+1];
			char *fnameptr = strrchr(dentry->d_name, '_');
			memset(now_file_name,0,sizeof(now_file_name));
			strncpy(now_file_name, dentry->d_name, fnameptr - dentry->d_name);

			// 자신의 이름 과 같은 파일들 묶어서 recover list 로 만들어서 recoverFile() 함수 부르기.
			while((dentry = readdir(dirp)) != NULL){
				now_dirpnum++;
				if(visit[now_dirpnum]){continue;}
				if(strcmp(dentry->d_name,".") == 0 || strcmp(dentry->d_name,"..") == 0){continue;}
				if(dentry->d_type == DT_REG && strncmp(dentry->d_name, now_file_name ,strlen(now_file_name)) == 0 && strlen((dentry->d_name) + strlen(now_file_name)) == 13 ){
					// strlen()  마지막 비교 -> strlen(now_file_name)만큼 같고 그 뒤에 _시간이 붙어있다면 같은 파일의 백업본. 위에 백업할때 쓴 timeString이 13개의 문자.
					visit[now_dirpnum] = 1;
					memset(temp_path,0,sizeof(temp_path));
					strcpy(temp_path, backup_path);
					if(temp_path[strlen(temp_path)-1] != '/'){strcat(temp_path, "/");}
					strcat(temp_path, dentry->d_name);
					if(stat(temp_path, &statbuf) < 0){
						fprintf(stderr, "recoverFiles : stat() error\n");
						exit(1);
					}
					push_back_f(&head_f, statbuf, temp_path);
				}
			}

			if(strcmp(filename_new, "") != 0){
				memset(temp_path_new,0,sizeof(temp_path_new));
				strcpy(temp_path_new, filename_new);
				if(temp_path_new[strlen(temp_path_new)-1] != '/'){strcat(temp_path_new, "/");}
				strcat(temp_path_new, now_file_name);
			}
			recoverFile(&head_f, flag_md5, flag_sha1, temp_path_new);
			continue;
		}
		else if(dentry->d_type == DT_DIR){
			// 재귀 호출을 위해 recoverFiles() 현재 탐색중인 디렉토리로 부름.
			push_front(head, opendir(temp_path), temp_path);
			
			if(strcmp(filename_new,"") != 0){
				memset(temp_path_new,0,sizeof(temp_path_new));
				strcpy(temp_path_new, filename_new);
				if(temp_path_new[strlen(filename_new)-1] != '/'){strcat(temp_path_new, "/");}
				strcat(temp_path_new, dentry->d_name);
			}
			recoverFiles(head, flag_md5, flag_sha1, flag_n, temp_path_new); // 재귀 호출로 하위 디렉토리 파일 및 디렉
			continue;
		}

	}
	pop_front(head);
}

void recoverFile(Node_f **head_f, int flag_md5, int flag_sha1, char *filename_new){
	
	// 백업 디렉토리 경로 받기.
	char PATH_BACKUP_DIRECTORY[MAX_FILE_PATH_LENGTH+1];
	getBackupDirectoryPath(PATH_BACKUP_DIRECTORY); // ssu_backup_u.h 내 백업 디렉토리 경로 반환 함수.
	
	char *fnameptr = strrchr((*head_f)->nodePath, '_'); 
	char tempname[MAX_FILE_PATH_LENGTH+1] = {0}; 
	strncpy(tempname, (*head_f)->nodePath, fnameptr - (*head_f)->nodePath);

	char print_path[MAX_FILE_PATH_LENGTH+1] = {0};
	char absolute_path[MAX_FILE_PATH_LENGTH+1] = {0};
	strcpy(absolute_path, PATH_HOME_DIRECTORY);
	strcat(absolute_path, tempname + strlen(PATH_BACKUP_DIRECTORY));
	strcpy(print_path, absolute_path);

	if(strcmp(filename_new,"") != 0){
		// 새로운 recover 경로.
		memset(print_path,0,sizeof(print_path));
		strcpy(print_path,absolute_path); // 원래 넣으려던 경로.
		memset(absolute_path, 0, sizeof(absolute_path));
		strcpy(absolute_path, filename_new);
	}

	// 새로 디렉토리 만들어서 recover 할 공간 만들어줘야함.
	char dname[MAX_FILE_PATH_LENGTH+1];
	fnameptr = strrchr(absolute_path, '/'); 
	memset(dname, 0, sizeof(dname));
	strncpy(dname,absolute_path, fnameptr - absolute_path);
	
	char* temp_dname = strdup(dname);
	strcpy(temp_dname, temp_dname);
	
	char* p = strtok(temp_dname, "/");
	char token_buf[MAX_FILE_PATH_LENGTH+1] = "";

	while(p){
		strcat(token_buf, "/");
		strcat(token_buf, p);
		if((access(token_buf, F_OK) != 0)){
			if(mkdir(token_buf, 0775) < 0){
				return;
			}
		}
		p = strtok(NULL, "/");
	}

	if((*head_f)->next == NULL){
		// recover 하려고 하는 파일이 1개 존재시
		compareAndCopyFile((*head_f)->nodePath, absolute_path, flag_md5, flag_sha1);
		pop_front_f(head_f);
		return;
	}

	printf("backup file list of \"%s\"\n",print_path);
	printf("0. exit\n");
	Node_f *temp_head = *head_f;
	int printNum = 1,recoverNum = 0;
	char *last_slash; 
	off_t fileSize[10], fileS; // 파일을 천단위로 끊어서 출력하기 위한 배열 및 현재 파일 사이즈.
	int numCount;
	while(temp_head != NULL){
		numCount = 0;
		fileS = temp_head->nodeStat.st_size;
		printf("%d. %s",printNum++,strrchr(temp_head->nodePath,'_')+1);
		printf("\t\t");

		// 파일 사이즈가 0일경우 예외 처리.
		if(fileS == (off_t)0){
			numCount = 1;
			fileSize[0] = 0;
		}
		
		while(fileS != (off_t)0){
			fileSize[numCount++] = fileS % (off_t)1000;
			fileS /= (off_t)1000;
		}
	
		for(int i = numCount-1; i >= 0; i--){
			if(i == 0 && numCount == 1){
				printf("%ld",fileSize[i]);
				continue;
			}

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

	do{
		// 적절한 수가 들어올때까지 루프.
		printf("Choose file to recover\n>>");
		scanf("%d",&recoverNum);
					
		char c; scanf("%c",&c); // 버퍼 제거.
	}while(recoverNum < 0 || recoverNum > printNum-1);
	
	if(recoverNum){
		int list_num = 0;
		// 해당하는 번호의 파일을 지정된 경로 -> absolute_path로 저장.
		Node_f *nowNode = *head_f;
		while(list_num != recoverNum -1){
			nowNode = nowNode->next;
			list_num++;
		}
		compareAndCopyFile(nowNode->nodePath, absolute_path, flag_md5, flag_sha1);
		while(*head_f != NULL){
			pop_front_f(head_f);
		}
	}
}

void compareAndCopyFile(char* source_path, char* dest_path, int flag_md5, int flag_sha1){
	unsigned char source_SHA1[SHA_DIGEST_LENGTH], dest_SHA1[SHA_DIGEST_LENGTH];
	unsigned char source_MD5H[MD5_DIGEST_LENGTH], dest_MD5H[MD5_DIGEST_LENGTH];

	// dest_path 파일과 source_path 파일이 같음.
	if(access(dest_path, F_OK) == 0){
		// dest_path  파일 존재.
		if(flag_md5){
			getMd5Sum(source_path,source_MD5H);
			getMd5Sum(dest_path,dest_MD5H);
			if(memcmp(source_MD5H, dest_MD5H, MD5_DIGEST_LENGTH) == 0){
				// 같은 해쉬값을 가진 파일임.
				printf("\"%s\" and \"%s\" is same file\n", source_path, dest_path);
				return;
			}
		}else if(flag_md5){
			getSha1Sum(source_path,source_SHA1);
			getSha1Sum(dest_path,dest_SHA1);
			if(memcmp(source_SHA1, dest_SHA1, SHA_DIGEST_LENGTH) == 0){
				// 같은 해쉬값을 가진 파일임.
				printf("\"%s\" and \"%s\" is same file\n", source_path, dest_path);
				return;
			}
		}
	}
	
	pid_t pid;
	int status;
	// 하위 프로세스를 통해 현재 파일을 -> 백업 경로에 파일명_현재시각 으로 백업.
	if((pid = fork()) < 0){
		fprintf(stderr, "backupFile : fork() error\n");
		return;
	}else if(pid == 0){
		// copy file to backup path 
		char *cpArgs[] = {"cp", source_path, dest_path, NULL};
		printf("\"%s\" backup recover to \"%s\"\n", source_path, dest_path);
		execvp(cpArgs[0], cpArgs);
		exit(0);
	}else{
		waitpid(pid,&status,0); // 자식 프로세스가 일할때는 부모 프로세스 기다림. 이로써, 2개 이상의 프로세스가 일을 하지 않음.
	}
}

void getMd5Sum(char *filename, unsigned char *md5Hash){
	int fd,count;
	const int BUFSIZE = 1024;
	unsigned char buf[BUFSIZE];

	if((fd = open(filename, O_RDONLY)) < 0){
		fprintf(stderr, "getmd5Sum open error\n");
		exit(1);
	}

	MD5_CTX md5_ctx;
	MD5_Init(&md5_ctx);

	while((count = read(fd, buf, BUFSIZE )) != 0){
		MD5_Update(&md5_ctx, buf, count);
	}

	MD5_Final(md5Hash, &md5_ctx);
	close(fd);
}

void getSha1Sum(char *filename, unsigned char *sha1Hash){
	int fd,count;
	const int BUFSIZE = 1024;
	unsigned char buf[BUFSIZE];

	if((fd = open(filename, O_RDONLY)) < 0){
		fprintf(stderr, "getSha1Sum open error\n");
		exit(1);
	}
	
	SHA_CTX sha1_ctx;
	SHA1_Init(&sha1_ctx);
	
	while((count = read(fd, buf, BUFSIZE)) != 0){
		SHA1_Update(&sha1_ctx, buf, count);
	}

	SHA1_Final(sha1Hash, &sha1_ctx);
	close(fd);
}
