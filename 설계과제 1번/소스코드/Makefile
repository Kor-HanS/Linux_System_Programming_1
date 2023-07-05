ssu_backup : ssu_backup.c help.c add.c remove.c recover.c linked_list.c
	gcc help.c -o help
	gcc -c add.c -o add.o
	gcc -c remove.c -o remove.o
	gcc -c recover.c -o recover.o
	gcc -c ssu_backup_u.c -o ssu_backup_u.o
	gcc -c -lcrypto -lssl -DOPENSSL_API_COMPAT=10101 linked_list.c -o linked_list.o
	gcc -DOPENSSL_API_COMPAT=10101 add.o linked_list.o ssu_backup_u.o -lcrypto -lssl -o add
	gcc -DOPENSSL_API_COMPAT=10101 remove.o linked_list.o ssu_backup_u.o -lcrypto -lssl  -o remove
	gcc -DOPENSSL_API_COMPAT=10101 recover.o linked_list.o ssu_backup_u.o -lcrypto -lssl -o recover
	gcc ssu_backup.c ssu_backup_u.o -o ssu_backup

clean : 
	rm add.o
	rm remove.o
	rm recover.o
	rm linked_list.o
	rm ssu_backup_u.o
