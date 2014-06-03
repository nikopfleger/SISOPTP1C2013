#ifndef SOCKETS_H_
#define SOCKETS_H_

//PROTOTIPOS FUNCIONES SERVER
int socketCreateServer(int puerto);
int socketListen(int unSocket);
int socketRecv(int socketReceptor, void *buffer);
int socketAccept(int unSocket);
int socketCreateServerAleatorio();

//PROTOTIPOS FUNCIONES CLIENTE
int socketCreateClient(char *ip, int puerto);
int socketSend(int unSocket, void *estructura, int length);
void socketDestroyClient(int unSocket);
void compactaClaves (int *tabla, int *n);
int dameMaximo (int *tabla, int n);
#endif /* SOCKETS_H_*/

