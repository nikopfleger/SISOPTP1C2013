/* Biblio de sockets
*UTN SO
*/

#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "sockets.h"
#include <errno.h>


typedef struct {
		char nombre[25];
		char simbolo[2];
		char planDeNiveles[50];
		int16_t vidas;
		char orquestador[30];
	} personajeStruct;

typedef struct
{
	int8_t id;
	int16_t length;
} header_t;

//FUNCIONES DE SERVIDOR

int socketCreateServer(int puerto)
{
	int socketServidor;
	struct sockaddr_in socketInfo;
	/* Se abre el socket servidor, avisando por pantalla y saliendo si hay
	 * algún problema */
	if((socketServidor = socket(AF_INET, SOCK_STREAM, 0))<0){
		sleep(1);
		perror ("Error al abrir servidor");
		return EXIT_FAILURE;
	}

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	socketInfo.sin_port = htons(puerto);

	int yes = 1;
	if (setsockopt(socketServidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
	    perror("setsockopt");
	    exit(1);
	}


	// Vincular el socket con una direccion de red almacenada en 'socketInfo'.
	if (bind(socketServidor, (struct sockaddr*) &socketInfo, sizeof(socketInfo)) != 0)
	{
		perror("Error al bindear socket escucha");
		return EXIT_FAILURE;
	}
	return socketServidor;
}

int socketCreateServerAleatorio()
{
	int socketServidor;
	struct sockaddr_in socketInfo;
	/* Se abre el socket servidor, avisando por pantalla y saliendo si hay
	 * algún problema */
	if((socketServidor = socket(AF_INET, SOCK_STREAM, 0))<0){
		sleep(1);
		perror ("Error al abrir servidor");
		return EXIT_FAILURE;
	}

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	socketInfo.sin_port = htons(0);

	return socketServidor;
}



int socketListen(int unSocket)
{
	if (listen(unSocket, 10) != 0)
	{
		perror("Error al poner a escuchar socket");
		return -1;
	}
	return unSocket;
}

int socketRecv(int socketReceptor, void *buffer)
{	
	header_t header;
	int bytesRecibidos;

// Primero: Recibir el header para saber cuando ocupa el payload.
	if (recv(socketReceptor, &header, sizeof(header_t), MSG_WAITALL) <= 0)
		return -1;

// Segundo: Alocar memoria suficiente para el payload.	
	buffer = malloc (header.length);

// Tercero: Recibir el payload.
	if(( bytesRecibidos=  recv(socketReceptor, buffer, header.length , MSG_WAITALL)) < 0){
		free(buffer);
		return -1;
	}	
	return bytesRecibidos;
	
}

int socketAccept(int unSocket)
{
	return accept(unSocket, NULL, 0);
}


//FUNCIONES DE CLIENTE


// Crear un socket:

int socketCreateClient(char *ip,int puerto){
	struct sockaddr_in socketInfo;
	int unSocket;
	if ((unSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error al crear socket");
		return EXIT_FAILURE;
	}
	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = inet_addr(ip);
	socketInfo.sin_port = htons(puerto);

// Conectar el socket con la direccion 'socketInfo'.
	if (connect(unSocket, (struct sockaddr*) &socketInfo, sizeof(socketInfo))!= 0) 
	{
		perror("Error al conectar socket");
		return EXIT_FAILURE;
	}

	return unSocket;
	}
int socketSend(int unSocket,void *estructura ,int length){
	if (send(unSocket, estructura, length, 0) >= 0)
		return EXIT_SUCCESS;
	else
	{
		perror("Error al enviar ");
		return EXIT_FAILURE;
	}
}
void socketDestroyClient(int unSocket){
	close(unSocket);

}

/*
 * Función que devuelve el valor máximo en la tabla.
 * Supone que los valores válidos de la tabla son positivos y mayores que 0.
 * Devuelve 0 si n es 0 o la tabla es NULL */
int dameMaximo (int *tabla, int n)
{
	int i;
	int max;

	if ((tabla == NULL) || (n<1))
		return 0;

	max = tabla[0];
	for (i=0; i<n; i++)
		if (tabla[i] > max)
			max = tabla[i];

	return max;
}

/*
 * Busca en array todas las posiciones con -1 y las elimina, copiando encima
 * las posiciones siguientes.
 */
void compactaClaves (int *tabla, int *n)
{
	int i,j;

	if ((tabla == NULL) || ((*n) == 0))
		return;

	j=0;
	for (i=0; i<(*n); i++)
	{
		if (tabla[i] != -1)
		{
			tabla[j] = tabla[i];
			j++;
		}
	}

	*n = j;
}
