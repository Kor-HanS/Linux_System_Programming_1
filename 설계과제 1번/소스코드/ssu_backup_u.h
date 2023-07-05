#ifndef SSU_BACKUP_UTILITY_H
#define SSU_BACKUP_UTILITY_H

// 절대 경로를 /backup 디렉토리 밑에 경로로 변환.
void getBackupPath(char *absolute_path, char *backup_path, char* PATH_BACKUP_DIRECTORY); 

// 백업 디렉토리의 경로를 가져옴.
void getBackupDirectoryPath(char *PATH_BACKUP_DIREOCTRY);

// 경로를 가공해줌/
// ex) /home/hanseung/../././././ -> /home , ./ ../  ~ 처리.
char* getProcessPath(char *path);
#endif
