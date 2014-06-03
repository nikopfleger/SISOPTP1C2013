#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"
#include "so-commons-library/collections/queue.h"
#include "so-commons-library/sockets.h"
#include "so-commons-library//log.h"
#include "plataforma.h"
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

//para inotify
#include <unistd.h>
#include <errno.h>
#include <sys/inotify.h>
#include <limits.h>

#define FALSE 0
#define TRUE 1

//Colas de Personajes
//void * colaBloqueados;

//para inotify
#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )
#define BUF_LEN     ( 1024 * EVENT_SIZE )

t_list *listaNP;
t_list *personajesConectados;
int EntroPrimerPersonaje;
pthread_mutex_t mutexPlataforma = PTHREAD_MUTEX_INITIALIZER;

//Variable del Quantum
int Quantum = 1;

int32_t Retardo = 1000000;

//Fin Variables Globales
int serverOrquestador;


int main(int argc,char *argv[])
{
	t_log *log = log_create("Plataforma.log","Orquestador",true,LOG_LEVEL_INFO);
	listaNP = list_create();
	personajesConectados = list_create();
	nivelPlanificadorStruct *unNivel;
	pthread_t orquestador;
	pthread_t planificador;
	pthread_t verificadorQuantum;
	nivelPlanificadorStruct *nivelCerrado;
	int socketNivel;
	int estoSeaTrue=1;
	EntroPrimerPersonaje = FALSE;



	pthread_create(&orquestador, NULL, orquestar, NULL);
	pthread_create(&verificadorQuantum, NULL, comprobarQuantum, NULL);

	log_info(log, "Se lanzo el hilo orquestador");
	int i;
	while(estoSeaTrue)
	{
		pthread_mutex_lock(&mutexPlataforma);

		for(i=0 ; i<list_size(listaNP) ; i++)
		{
			unNivel = list_get(listaNP,i);
			if(unNivel->estado == 1) // Hay que levantar el planificador
			{
				unNivel->estado = 2; // Levantado
				pthread_create(&planificador, NULL, planificar, unNivel->nivel);
				unNivel->hilo = planificador;
				log_info(log, "Se lanzo el hilo planificador para el nivel %s",unNivel->nivel);
				list_replace(listaNP,i,(void*)unNivel);

			}
			if(unNivel->estado == 3) //Hay que cerrar el planificador.
			{
				nivelCerrado = list_get(listaNP, i);
				if(nivelCerrado->estado == 2 || nivelCerrado->estado == 3)
				{
					pthread_cancel(nivelCerrado->hilo);
					log_info(log,"Planificador de nivel %s cerrado",nivelCerrado->nivel);
					nivelCerrado = list_remove(listaNP, i);
					free(nivelCerrado);
				}
				else
				{
					log_info(log,"El planificador estaba cerrado y su id es %d",nivelCerrado->estado);
					nivelCerrado = list_remove(listaNP, i);
					free(nivelCerrado);
				}
			}

			if(EntroPrimerPersonaje == TRUE)
				if(list_is_empty(personajesConectados))
				{
					log_info(log,"Todos los personajes terminaron su plan de nivel");
					estoSeaTrue = 0;
					while(!list_is_empty(listaNP))
					{
						nivelCerrado = list_remove(listaNP, 0);
						socketNivel = socketCreateClient(nivelCerrado->ipNivel, nivelCerrado->puertoNivel);
						identificadorStruct *identificador = malloc(sizeof(identificadorStruct));
						identificador->id=2;
						socketSend(socketNivel,identificador,sizeof(identificadorStruct));
						shutdown(socketNivel,2);
						log_info(log,"Planificador de nivel %s cerrado",nivelCerrado->nivel);
						pthread_cancel(nivelCerrado->hilo);
						free(identificador);
						free(nivelCerrado);
					}
				}
		}



	}

	pthread_cancel(orquestador);
	pthread_cancel(verificadorQuantum);

	shutdown(serverOrquestador,2);

	log_info(log,"Felicitaciones, vamos a ejecutar koopa!");
	log_destroy(log);
	sleep(5);

	char *arg[] = {"/home/utnso/Escritorio/tp-20131c-nintendo-c-nintendo-nada/Koopa/koopa","/home/utnso/Escritorio/tp-20131c-nintendo-c-nintendo-nada/Koopa/prueba.txt",NULL};
	execv("/home/utnso/Escritorio/tp-20131c-nintendo-c-nintendo-nada/Koopa/koopa",arg);



	printf("Esto no deberia ejecutarse nunca!");

	return 0;
}

void * orquestar(void* argumento)
{

	int socketNuevaConexion;
	int bytesRecibidos;		
	header_t header;
	nivelPlanificadorStruct *unNivel;
	payloadNivelPlanificadorStruct buffer;
	serverOrquestador = socketCreateServer(5001);
	personajeStruct *personaje;
	payloadNivelStruct *nivel;
	nivelCerrandoStruct *nivelCerrando;
	void *recursosLiberados;
	nivelPlanificadorStruct *nivelSolicitado;
	int error;
	nivelYPersonajeStruct *nivelYPersonaje;
	struct sockaddr_in info;
	socklen_t addr_size;

	t_log *log = log_create("Plataforma.log","Orquestador",true,LOG_LEVEL_INFO);

	while(1)
	{
		error = socketListen(serverOrquestador);

		if(error == -1)
			kill(getpid(),9);

		addr_size = sizeof(info);
		socketNuevaConexion //= socketAccept(serverOrquestador);
		= accept(serverOrquestador, (struct sockaddr *)&info, &addr_size);

		// Primero: Recibir el header para saber cuando ocupa el payload y saber que se está recibiendo.
		if (recv(socketNuevaConexion, &header, sizeof(header_t), MSG_WAITALL) <= 0)
		{
			//return -1;
		}
		// Segundo: Diferenciar si es un personaje o nivel
		if(header.id == 1) //Es un personaje			
		{
			personaje = malloc(header.length);
			if(( bytesRecibidos = recv(socketNuevaConexion, personaje, header.length , MSG_WAITALL)) < 0)
			{
				free(personaje);
				//return -1;
			}

			log_info(log, "Se conecto el personaje %s", personaje->nombre);

			bool verSiYaEsta(void* simbolo) {
				return (((simboloTerminoStruct*) simbolo)->simbolo == personaje->simbolo);
			}

			simboloTerminoStruct *simboloSolicitado = list_find(personajesConectados,verSiYaEsta);
			if(simboloSolicitado == NULL)
			{
				simboloTerminoStruct *simboloPersonaje = malloc(sizeof(simboloTerminoStruct));
				simboloPersonaje->simbolo = personaje->simbolo;
				list_add(personajesConectados,simboloPersonaje);
			}

			bool condicion(void* nivel) {
				return (strcmp(((nivelPlanificadorStruct*) nivel)->nivel , personaje->nivel) == 0 );
			}

			nivelPlanificadorStruct *nivelSolicitado = list_find(listaNP,condicion);
			if(nivelSolicitado != NULL)
			{
				llenarBuffer(&buffer, nivelSolicitado);
				log_info(log,"Nivel IP:%s Puerto:%d ",nivelSolicitado->ipNivel,nivelSolicitado->puertoNivel);
				log_info(log,"Puerto Planificador:%d",nivelSolicitado->puertoPlanificador);

				send(socketNuevaConexion, &buffer , sizeof(payloadNivelPlanificadorStruct), 0);
			}
			else
			{
				log_info(log,"%s me solicita el nivel %s, pero no esta levantado",personaje->nombre,personaje->nivel);
				strcpy(buffer.ipNivel,"0");
				buffer.puertoNivel=0;
				buffer.puertoPlanificador=0;
				send(socketNuevaConexion, &buffer , sizeof(payloadNivelPlanificadorStruct), 0);
			}
			free(personaje);
		}
		if (header.id == 2) //Es un nivel
		{
			nivel = malloc(header.length);
			if(recv(socketNuevaConexion, nivel, header.length, MSG_WAITALL) < 0)
			{
				free(nivel);
				//return -1;
			}
			log_info(log,"Socket IP:%s PORT:%d",inet_ntoa(info.sin_addr),ntohs(info.sin_port));
			log_info(log, "Se conecto el nivel %s",nivel->nombre);
			unNivel=malloc(sizeof(nivelPlanificadorStruct));
			llenarUnNivel(unNivel, nivel,info);
			log_info(log,"En lista: IP:%s Puerto %d",unNivel->ipNivel,unNivel->puertoNivel);
			unNivel->puertoPlanificador = -1;
			pthread_mutex_t creacionMutex = PTHREAD_MUTEX_INITIALIZER;
			unNivel->semColas = creacionMutex;


			list_add(listaNP,(void *) unNivel);
			free(nivel);

			pthread_mutex_unlock(&mutexPlataforma);
		}
		if (header.id == 3) //Nivel informando los recursos liberados porque un personaje termino el nivel
		{
			recursosLiberados = malloc(header.length);
			if(recv(socketNuevaConexion, recursosLiberados, header.length, 0) < 0)
			{
				free(recursosLiberados);
				//return -1;
			}
			nivelYPersonaje = malloc(sizeof(nivelYPersonajeStruct));

			recv(socketNuevaConexion, nivelYPersonaje, sizeof(nivelYPersonajeStruct), 0);
			liberadosNoSerializadoStruct *colaDeserializada=desserializador_liberados(recursosLiberados,header.length);
			asignarRecursosAPersonajesBloqueados(colaDeserializada->cola, nivelYPersonaje, socketNuevaConexion);
			log_info(log,"Recursos del nivel %s asignados",nivelYPersonaje->nivelNombre);
			free(recursosLiberados);
			free(nivelYPersonaje);
		}
		if(header.id == 4) //Nivel informando que se va a cerrar
		{
			nivelCerrando = malloc(header.length);
			recv(socketNuevaConexion, nivelCerrando, header.length, 0);

			bool condicion(void* nivel)
				{
					return (strcmp(((nivelPlanificadorStruct*) nivel)->nivel , nivelCerrando->nivel) == 0 );
				}


				nivelSolicitado = list_find(listaNP, condicion);
				if(nivelSolicitado != NULL)
				{
					log_info(log,"El nivel %s se cerro",nivelCerrando->nivel);
					nivelSolicitado->estado=3; //Para que el orquestador lo cierre.
					free(nivelCerrando);
				}
				pthread_mutex_unlock(&mutexPlataforma);
		}
		if (header.id == 5) //Nivel informando interbloqueo
		{

			void *data = malloc(header.length);
			char nombre[30];
			recv(socketNuevaConexion, data, header.length, 0);
			recv(socketNuevaConexion, &nombre, sizeof(char[30]), 0);
			colaNoSerializadaStruct *colaInterbloqueo = desserializador_interbloqueo(data);


			bool condicion(void* nivel)
			{
				return (strcmp(((nivelPlanificadorStruct*) nivel)->nivel , nombre) == 0 );
			}

			nivelSolicitado = list_find(listaNP, condicion);
			detectarYMatarPersonaje(colaInterbloqueo->cola,nivelSolicitado);


		}
		if (header.id == 6) // Personaje termino su plan
		{

			simboloTerminoStruct *simboloRecibido = malloc(header.length);
			recv(socketNuevaConexion, simboloRecibido, header.length, 0);

			bool buscarloParaEliminarlo(void* simbolo) {
				return (((simboloTerminoStruct*) simbolo)->simbolo == simboloRecibido->simbolo);
			}

			simboloTerminoStruct *simboloEliminar = list_remove_by_condition(personajesConectados,buscarloParaEliminarlo);
			free(simboloEliminar);

			log_info(log,"El personaje %c termino su plan de niveles :)",simboloRecibido->simbolo);

			 free(simboloRecibido);

			 pthread_mutex_unlock(&mutexPlataforma);

		}
		close(socketNuevaConexion);
	}
	return 0;
}

void* planificar(void* nivel)
{
	char nombreNivel[30];
	strcpy(nombreNivel, nivel);
	char namelog[40];
	snprintf(namelog,sizeof(namelog),"%s%s%s","Planificador_",nombreNivel,".log");
	int *HayAlguienMoviendose = malloc(sizeof(int));
	*HayAlguienMoviendose = 0;
	t_log *log = log_create(namelog,"Planificador",true,LOG_LEVEL_INFO);

	int socketServidor, socketNuevoPersonaje;
	fd_set descriptoresLectura;
	int numeroPersonajes = 0;
	int maximo;
	int i;
	t_list *listaPersonajes = list_create();
	int32_t ordenDeEntrada=0;
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	pthread_mutex_t creacionMutex = PTHREAD_MUTEX_INITIALIZER;


	bool condicion(void* nivel)
	{
		return (strcmp(((nivelPlanificadorStruct*) nivel)->nivel , nombreNivel) == 0 );
	}
	nivelPlanificadorStruct *nivelSolicitado = list_find(listaNP,condicion);
	nivelSolicitado->listaPersonajes = listaPersonajes;

	nivelSolicitado->log = log;

	t_queue *colaListos;
	t_queue *colaBloqueados;
	colaListos = nivelSolicitado->listos;
	colaBloqueados = nivelSolicitado->bloqueados;
	header_t header;


	socketServidor = socketCreateServerAleatorio();
		socketListen(socketServidor);


		if (getsockname(socketServidor, (struct sockaddr *)&sin, &len) == -1)
		    perror("getsockname");

		int variable =  ntohs(sin.sin_port);

	nivelSolicitado->puertoPlanificador = variable;
	log_info(log, "El puerto del planificador es el  %d", nivelSolicitado->puertoPlanificador);

	payloadPersonajeBloqueado *personajeBloqueado;
	listaPersonajesStruct *unPersonaje;
	listaPersonajesStruct *nuevoPersonaje;
	statusStruct *movimientoFinal;
	listaPersonajesStruct *personajeEjecutando;

	listaPersonajesStruct *elementoBorrado;

	socketListen(socketServidor);

	quantumActualStruct quantumActual;
	quantumActual.quantumActual = 0;
	
	log_info(log,"Planificador del nivel %s corriendo correctamente",nombreNivel);

	while (1)
	{
		/* Se inicializa descriptoresLectura */
		FD_ZERO (&descriptoresLectura);

		/* Se añade para select() el socket servidor */
		FD_SET (socketServidor, &descriptoresLectura);

		/* Se añaden para select() los sockets con los clientes ya conectados */
		for (i=0; i<numeroPersonajes; i++)
		{
			if (numeroPersonajes > 0)
			{
				unPersonaje = (listaPersonajesStruct *) list_get(listaPersonajes, i);
				FD_SET (unPersonaje->descriptorPersonaje, &descriptoresLectura);
			}
		}
		/* Se obtiene el valor del descriptor más grande. Si no hay ningún cliente,
		 * devolverá 0 */
		maximo = dameMax(listaPersonajes);
		
		if (maximo < socketServidor)
		{
			maximo = socketServidor;
		}

		/* Espera indefinida hasta que alguno de los descriptores tenga algo
		 * que decir: un nuevo cliente o un cliente ya conectado que envía un
		 * mensaje */
		select(maximo + 1, &descriptoresLectura, NULL,  NULL, NULL);			
			
		/* Se comprueba si algún cliente ya conectado ha enviado algo */		
		for (i=0; i<numeroPersonajes; i++)
		{
			unPersonaje = (listaPersonajesStruct *) list_get(listaPersonajes, i);
			if (FD_ISSET (unPersonaje->descriptorPersonaje, &descriptoresLectura))
			{
				/* Se lee lo enviado por el cliente */				
				if (recv(unPersonaje->descriptorPersonaje, &header, sizeof(header_t), MSG_WAITALL))
				{
					if(header.id == 1)  //Mensaje de Turno Terminado
					{
						movimientoFinal=malloc(sizeof(header.length));
						recv(unPersonaje->descriptorPersonaje, movimientoFinal, header.length, MSG_WAITALL);

						if (quantumActual.quantumActual < Quantum)
						{
							if(movimientoFinal->idMensaje==TRUE) // No agota q y llega al recurso
							{
								quantumActual.quantumActual=Quantum;
								goto TerminoDeMoverse;
							}
							if(movimientoFinal->idMensaje==2) // No agota q y termina nivel
							{
								*HayAlguienMoviendose = FALSE;


								movimientoPermitido(colaListos,HayAlguienMoviendose,&quantumActual,log,&(nivelSolicitado->semColas));
								log_info(log,"Ejecutando: %s",quantumActual.personaje.nombre);
								logearCola(colaListos,log,'L');
								logearCola(colaBloqueados,log,'B');
							}
							if(movimientoFinal->idMensaje==0) // No agota q ni llega recurso ni termina nivel
							{
								movimientoPermitidoPorQuantum(&quantumActual,log);
								log_info(log,"Ejecutando: %s",quantumActual.personaje.nombre);
								logearCola(colaListos,log,'L');
								logearCola(colaBloqueados,log,'B');
							}

						}
						else
						if(quantumActual.quantumActual == Quantum) //Agota q
						{
							if(movimientoFinal->idMensaje==2) // Agota q y termina nivel
							{
								*HayAlguienMoviendose = FALSE;
								movimientoPermitido(colaListos,HayAlguienMoviendose,&quantumActual,log,&(nivelSolicitado->semColas));
								log_info(log,"Ejecutando: %s",quantumActual.personaje.nombre);
								logearCola(colaListos,log,'L');
								logearCola(colaBloqueados,log,'B');
							}
							else
							{
					TerminoDeMoverse:  //Agota q / agota q y llega al recurso
								*HayAlguienMoviendose = FALSE;
								pthread_mutex_lock(&(nivelSolicitado->semColas));
								queue_push(colaListos, (void *)unPersonaje);
								pthread_mutex_unlock(&(nivelSolicitado->semColas));
								movimientoPermitido(colaListos,HayAlguienMoviendose,&quantumActual,log,&(nivelSolicitado->semColas));
								log_info(log,"Ejecutando: %s",quantumActual.personaje.nombre);
								logearCola(colaListos,log,'L');
								logearCola(colaBloqueados,log,'B');
							}
						}

						free(movimientoFinal);

						}
					if(header.id == 2)	 //Mensaje de Bloqueado
					{
						personajeBloqueado = malloc(header.length);
						recv(unPersonaje->descriptorPersonaje, personajeBloqueado, header.length, MSG_WAITALL);
						unPersonaje->recursoQueLoBloquea= (char) personajeBloqueado->recursoQueLoBloquea;
						log_info(log,"%s bloqueado en el Nivel %s Recurso:%c",unPersonaje->nombre,unPersonaje->nivelDondeEsta,unPersonaje->recursoQueLoBloquea);

						*HayAlguienMoviendose= FALSE;
						pthread_mutex_lock(&(nivelSolicitado->semColas));
						queue_push(colaBloqueados, (void *)unPersonaje);
						pthread_mutex_unlock(&(nivelSolicitado->semColas));

						logearCola(colaListos,log,'L');
						logearCola(colaBloqueados,log,'B');

						if(!queue_is_empty(colaListos))
						{
							personajeEjecutando = queue_peek(colaListos);
							movimientoPermitido(colaListos,HayAlguienMoviendose,&quantumActual,log,&(nivelSolicitado->semColas));
							log_info(log,"Ejecutando: %s",personajeEjecutando->nombre);
							logearCola(colaListos,log,'L');
							logearCola(colaBloqueados,log,'B');

						}

						free(personajeBloqueado);
					}
				}
				else
				{
					/*Un Cliente cerro la conexion*/
					log_info(log,"%s ha cerrado la conexion",unPersonaje->nombre);
					shutdown(unPersonaje->descriptorPersonaje,2);
					sacarDeCola(colaListos,unPersonaje,'L',log,&(nivelSolicitado->semColas));
					sacarDeCola(colaBloqueados,unPersonaje,'B',log,&(nivelSolicitado->semColas));

					pthread_mutex_lock(&(unPersonaje->recursosLiberados));
					elementoBorrado =list_remove(listaPersonajes, i);

					numeroPersonajes = list_size(listaPersonajes);
					log_info(log,"Se desconecto un personaje");
					logearCola(colaListos,log,'L');
					logearCola(colaBloqueados,log,'B');

					if(!queue_is_empty(colaListos))
					{
						if(quantumActual.personaje.simbolo == elementoBorrado->simbolo)
						{
							*HayAlguienMoviendose=FALSE;
						}
						if(!(*HayAlguienMoviendose))
						{
							movimientoPermitido(colaListos,HayAlguienMoviendose,&quantumActual,log,&(nivelSolicitado->semColas));
						}
					}
					else
						if(quantumActual.personaje.simbolo == elementoBorrado->simbolo)
						{
							*HayAlguienMoviendose=FALSE;
						}
					free(elementoBorrado);
				}
			}

		}

		/* Se comprueba si algún cliente nuevo desea conectarse y se le
		 * admite */
		if (FD_ISSET (socketServidor, &descriptoresLectura))
		{
			//Nuevo Cliente
			socketNuevoPersonaje = socketAccept(socketServidor);
			nombreYSimboloStruct newPersonaje;

			recv(socketNuevoPersonaje, &newPersonaje, sizeof(nombreYSimboloStruct), MSG_WAITALL);

			bool existePersonaje(void* personaje)
					        {
					                return (((listaPersonajesStruct*) personaje)->simbolo == newPersonaje.simbolo );
					        }

					listaPersonajesStruct *personajeDuplicado = list_find((nivelSolicitado->listaPersonajes), existePersonaje);

			if(personajeDuplicado != NULL)
			{

				if(quantumActual.personaje.simbolo == personajeDuplicado->simbolo)
				{
					*HayAlguienMoviendose=FALSE;
				}
				log_info(log,"%c ha cerrado la conexion.",personajeDuplicado->simbolo);
				shutdown(personajeDuplicado->descriptorPersonaje,2);
				sacarDeCola(colaListos,personajeDuplicado,'L',log,&(nivelSolicitado->semColas));
				sacarDeCola(colaBloqueados,personajeDuplicado,'B',log,&(nivelSolicitado->semColas));

				pthread_mutex_lock(&(personajeDuplicado->recursosLiberados));
				elementoBorrado =list_remove_by_condition(listaPersonajes, existePersonaje);

				numeroPersonajes = list_size(listaPersonajes);
				log_info(log,"Cerro uno, logeo colas..");
				logearCola(colaListos,log,'L');
				logearCola(colaBloqueados,log,'B');

				free(elementoBorrado);
			}

			nuevoPersonaje =malloc(sizeof(listaPersonajesStruct));
			llenarUnPersonaje(nuevoPersonaje, &newPersonaje, nombreNivel, socketNuevoPersonaje);
			nuevoPersonaje->ordenDeEntrada=ordenDeEntrada++;
			nuevoPersonaje->recursosLiberados= creacionMutex;

			pthread_mutex_lock(&nuevoPersonaje->recursosLiberados);

			log_info(log,"El char es: %c",nuevoPersonaje->simbolo);

			list_add(listaPersonajes, nuevoPersonaje);
			numeroPersonajes = list_size(listaPersonajes);
			if (!(*HayAlguienMoviendose))
			{
				pthread_mutex_lock(&(nivelSolicitado->semColas));
				queue_push(colaListos, (void *)nuevoPersonaje);
				pthread_mutex_unlock(&(nivelSolicitado->semColas));
				log_info(log, "Se conecto el personaje %s y se le autorizo un movimiento por ser el primero de la lista",nuevoPersonaje->nombre);
				movimientoPermitido(colaListos,HayAlguienMoviendose,&quantumActual,log,&(nivelSolicitado->semColas));
				log_info(log,"Ejecutando: %s", nuevoPersonaje->nombre);
				logearCola(colaListos,log,'L');
				logearCola(colaBloqueados,log,'B');
			}
			else
			{
				pthread_mutex_lock(&(nivelSolicitado->semColas));
				queue_push(colaListos, (void *)nuevoPersonaje);
				pthread_mutex_unlock(&(nivelSolicitado->semColas));
				log_info(log,"Se conecto el personaje %s y se agrego a la cola de listos",nuevoPersonaje->nombre);
				logearCola(colaListos,log,'L');
				logearCola(colaBloqueados,log,'B');
			}

				EntroPrimerPersonaje = TRUE;


		}
	}

	return 0;
}

void * comprobarQuantum()
{
	char buffer[BUF_LEN];
	t_log* log;


	log = log_create("quantum.log","Quantum",true,LOG_LEVEL_INFO);
	int file_descriptor = inotify_init();
	if (file_descriptor <0){
		log_error(log,"inotify_init");
	}

	int quantumAnterior;
	int32_t retardoAnterior;

	t_configQuantum *configQuantum;
	configQuantum = malloc(sizeof(t_configQuantum));
	char *configName;
	configName = malloc(sizeof("./quantum/quantum.txt"));
	configName = "./quantum/quantum.txt";
	configQuantum = config_create(configName);
	configQuantum->quantum = config_get_int_value(configQuantum,"quantum");
	configQuantum->retardo = config_get_double_value(configQuantum,"retardo");
	Retardo = (configQuantum->retardo) * 1000000;
	Quantum=configQuantum->quantum;
	log_info(log,"Quantum: %d",Quantum);
	log_info(log,"Retardo %.2f", configQuantum->retardo);
	retardoAnterior = Retardo;
	quantumAnterior = Quantum;
	config_destroy(configQuantum);


		int watch_descriptor = inotify_add_watch(file_descriptor, "quantum", IN_MODIFY);
		while(1)
			{

		int length = read(file_descriptor, buffer, BUF_LEN);
		if (length < 0) {
				log_error(log,"ERROR EN QUANTUM");
			}

		int offset = 0;

		while (offset < length)
		{
			struct inotify_event *event = (struct inotify_event *) &buffer[offset];
			if (event->len)
			{
				if (event->mask & IN_CREATE)
				{
					if (event->mask & IN_ISDIR)
					{
						log_info(log,"The directory %s was created.", event->name);
					} else
					{
						log_info(log,"The file %s was created.", event->name);
					}
				} else if (event->mask & IN_DELETE)
				{
					if (event->mask & IN_ISDIR)
					{
						log_info(log,"The directory %s was deleted.", event->name);
					} else
					{
						log_info(log,"The file %s was deleted.", event->name);
					}
				} else if (event->mask & IN_MODIFY) {
				if (event->mask & IN_ISDIR)
				{
					log_info(log,"The directory %s was modified.", event->name);
				} else
				{
					configQuantum = malloc(sizeof(t_configQuantum));
					configQuantum = config_create(configName);
					if(config_has_property(configQuantum,"quantum"))
					{
						configQuantum->quantum = config_get_int_value(configQuantum,"quantum");
						Quantum=configQuantum->quantum;
						if(Quantum != quantumAnterior)
						{
							log_info(log,"Quantum: %d", Quantum);
							quantumAnterior = Quantum;
						}
					}

					if(config_has_property(configQuantum,"retardo"))
					{
						configQuantum->retardo = config_get_double_value(configQuantum,"retardo");
						Retardo = (configQuantum->retardo) * 1000000;
						if(Retardo != retardoAnterior)
						{
							log_info(log,"Retardo %.2f", configQuantum->retardo);
							retardoAnterior = Retardo;
						}
					}

					config_destroy(configQuantum);
				}
				}
			}
			offset += sizeof (struct inotify_event) + event->len;
		}



	}

		inotify_rm_watch(file_descriptor, watch_descriptor);
		close(file_descriptor);

		return 0;
}

int32_t asignarRecursosAPersonajesBloqueados(t_queue *recursosLiberados,nivelYPersonajeStruct *nivelYPersonaje, int socketNivel)
{
        t_queue *colaBloqueados;
        t_queue *colaListos;
        t_queue *colaAsignados = queue_create();
        personajeYRecursoAsignadoStruct *unRecurso;

        int32_t cantidadPersonajesDesbloqueados=0;

        bool condicion(void* nivel)
        {
                return (strcmp(((nivelPlanificadorStruct*) nivel)->nivel , nivelYPersonaje->nivelNombre) == 0 );
        }

        nivelPlanificadorStruct *nivelSolicitado = list_find(listaNP, condicion);

        colaBloqueados = nivelSolicitado->bloqueados;
        colaListos = nivelSolicitado->listos;

        t_log *log = log_create("Plataforma.log","Orquestador",true,LOG_LEVEL_INFO);
        log_info(log,"Entramos a asignar los recursos");


        t_queue *colaAuxiliar = queue_create();

        if(queue_is_empty(colaBloqueados))
                        log_info(log,"No hay personajes bloqueados");

        pthread_mutex_lock(&(nivelSolicitado->semColas));
        while (!queue_is_empty(recursosLiberados))
        {
        		queue_clean(colaAuxiliar);
                recursoStruct *recursoLiberado = (recursoStruct *)queue_pop(recursosLiberados);

                while(!queue_is_empty(colaBloqueados))
                {
                        listaPersonajesStruct *personaje = (listaPersonajesStruct *)queue_pop(colaBloqueados);

                        if (personaje->recursoQueLoBloquea != recursoLiberado->recurso)
                        {
                                queue_push(colaAuxiliar, (void *)personaje);
                        }
                        else
                        {

                                log_info(log,"Desbloqueo a %s en nivel %s por %c",personaje->nombre,nivelYPersonaje->nivelNombre,recursoLiberado->recurso);
                                cantidadPersonajesDesbloqueados++;
                                unRecurso= malloc(sizeof(personajeYRecursoAsignadoStruct));
                                unRecurso->simbolo=personaje->simbolo;
                                unRecurso->recurso=recursoLiberado->recurso;
                                queue_push(colaAsignados,(void*) unRecurso);
                                queue_push(colaListos, (void *)personaje);
                                break;
                        }
                }
                while(!queue_is_empty(colaBloqueados))
                        queue_push(colaAuxiliar,queue_pop(colaBloqueados));

                queue_copy(colaBloqueados,colaAuxiliar);

        }
        pthread_mutex_unlock(&(nivelSolicitado->semColas));

		int32_t size=0,length;
		colaNoSerializadaStruct colaAsig;
		colaAsig.cantidadPersonajes = cantidadPersonajesDesbloqueados;
		colaAsig.cola = colaAsignados;
		char *buffer = serializador_recursos(&colaAsig,&size);
		length = size + sizeof(int32_t);

		socketSend(socketNivel,buffer,length);


		cantidadPersonajesDesbloqueados=0;
		queue_destroy(colaAsignados);
		queue_destroy(colaAuxiliar);
		free(buffer);


		bool condicionPersonaje(void* personaje)
		        {
		                return (((listaPersonajesStruct*) personaje)->simbolo == nivelYPersonaje->personaje );
		        }

		listaPersonajesStruct *personajeSolicitado = list_find((nivelSolicitado->listaPersonajes), condicionPersonaje);

		if(personajeSolicitado != NULL)
		{
			pthread_mutex_unlock(&personajeSolicitado->recursosLiberados);
		}

		log_destroy(log);

        return 0;
}

