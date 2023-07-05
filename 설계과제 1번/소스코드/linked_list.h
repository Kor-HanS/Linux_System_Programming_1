#ifndef LINKED_LIST_H
#define LINKED_LIST_H

// direcotry node 
typedef struct node {
	DIR *nodeDirp;
	char nodePath[4096];
	struct node *next;
}Node;

// file node 
typedef struct node_file{
	char nodePath[4096];
	struct stat nodeStat;
	struct node_file *next;
}Node_f;

// directory node
void push_front(Node **head, DIR *dirp, char *path);
void push_back(Node **head, DIR *dirp, char *path);
void pop_front(Node **head);
void pop_back(Node **head);
void insertNode(Node *prevNode, DIR *dirp, char *path);

// file node 
void push_back_f(Node_f **head_f, struct stat statbuf,  char path[]);
void pop_front_f(Node_f **head_f); 

// add
void backupFiles(Node **head, int flag_md5, int flag_sha1); // backup files(BFS)
void backupFile(char *filename, int flag_md5, int flag_sha1); // backup file

// remove
void removeAll(int flag_c); // remove /backup directory
void removeFiles(Node **head, int flag_c, int *deleteF_count, int *deleteD_count); // directory remove(DFS)
void removeFile(Node_f **head, int flag_c, int count); // file remove 

// recover
void recoverFiles(Node **head, int flag_md5, int flag_sha1, int flag_n, char* filename_new);
void recoverFile(Node_f **head_f, int flag_md5, int flag_sha1, char* filename_new); // file recover 
void compareAndCopyFile(char* source_path, char* dest_path, int flag_md5, int flag_sha1);

// hash function 
void getMd5Sum(char *filename, unsigned char* md5Hash);
void getSha1Sum(char *filename, unsigned char* sha1Hash);

#endif
