#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "so-commons-library/string.h"
#include "config.h"
#include "so-commons-library/sockets.h"
#include "nivel.h"
#include "so-commons-library/log.h"
#include "so-commons-library/collections/queue.h"
#include <signal.h>

#define TRUE 1
#define FALSE 0
#define ERROR -1


ITEM_NIVEL * ListaItems = NULL;
int32_t numeroPersonajes = 0;
nivelConfigStruct nivel;
pthread_mutex_t controlListaPersonajes= PTHREAD_MUTEX_INITIALIZER;
char logName[20];
t_list *listaPersonajes;
//Prueba
int32_t socketOrquestador;

int32_t main (int32_t argc,char *argv[]) {

	signal(SIGINT, rutinaSignal);
	payloadNivelStruct nivelPayload;
	nivelPaquetizadoStruct *nivelPaquetizado = malloc(sizeof(nivelPaquetizadoStruct));
	t_config *nivelConfig = malloc(sizeof(t_config));
	t_list *cajas;
	int32_t i;
	pthread_t chequeoInterbloqueo;


	cajas = list_create();

	//Levanto el archivo de configuracion
 	nivelConfig = config_create(argv[1]);
	int32_t numeroCajas = config_keys_amount(nivelConfig) - 4;
	int32_t lengthKey;

	nivelConfig->nombre  = config_get_string_value(nivelConfig,"nombre");

	for (i=1;i<=numeroCajas;i++)
	{

		lengthKey = get_int_len(i);
		char aux[lengthKey + 1];
		char *auxCaja = malloc(sizeof("caja") + lengthKey);
		strcpy(auxCaja,"caja");

		nivelConfig->caja = config_get_string_value(nivelConfig, strcat(auxCaja, itoa(i,aux) ) );
		list_add(cajas,(void *) getInfoCaja(nivelConfig->caja));


	}

	nivel.cajas = cajas;
	nivelConfig->orquestador = config_get_string_value(nivelConfig,"orquestador");
	nivelConfig->tiempoChequeoDeadlock = config_get_int_value(nivelConfig,"tiempoChequeoDeadlock");
	nivelConfig->recovery = config_get_int_value(nivelConfig,"recovery");

	snprintf(logName,sizeof(logName),"%s%s",nivelConfig->nombre,".log");


	//copio lo que levante del archivo de configuracion a la la variable nivel de la estructura
	//de tipo nivelStuct
	strcpy(nivel.nombre,nivelConfig->nombre);
	strcpy(nivel.orquestador,nivelConfig->orquestador);
	nivel.tiempoChequeoDeadlock = nivelConfig->tiempoChequeoDeadlock;
	nivel.recovery = nivelConfig->recovery;

	strcpy(nivelPayload.nombre , nivel.nombre);

	nivelPaquetizado->id=2; //para indicarle al servidor que soy un nivel
	nivelPaquetizado->length = sizeof(payloadNivelStruct);
	nivelPaquetizado->nivel = nivelPayload;

	ListaItems = crearNivel(nivel);
	nivel_gui_dibujar(ListaItems);
	pthread_create(&chequeoInterbloqueo, NULL, chequearInterbloqueo, NULL);
	servidorNivel(nivelPaquetizado);
	pthread_join(chequeoInterbloqueo,NULL);
	return 0;

}

void * chequearInterbloqueo(void *arg) {
	while (1) {

		sleep(nivel.tiempoChequeoDeadlock / 1000);
		pthread_mutex_lock(&controlListaPersonajes);
		algoritmoInterbloqueo(numeroPersonajes);
		pthread_mutex_unlock(&controlListaPersonajes);

	}
	return 0;
}

int32_t servidorNivel(nivelPaquetizadoStruct *nivelPaquetizado)
{

	char nombreLogRe[50];

	snprintf(nombreLogRe,sizeof(nombreLogRe),"%s%s","R",logName);

	t_log *log = log_create(logName,"Nivel",FALSE,LOG_LEVEL_INFO);
	t_log *log3 = log_create(nombreLogRe,"Recursos",FALSE,LOG_LEVEL_INFO);
	int32_t socketServidor, socketNuevoPersonaje;
	fd_set descriptoresLectura;
	int32_t maximo;
	int32_t i;
	int32_t noDisponible;
	int32_t descriptorActual;
	header_t header;
	char simbolo;
	int8_t identificador;
	payloadRecursoStruct *mensajeRecurso;
	posicionesEnNivel *posicion;
	payloadPosicionStruct *mensajePosicion;
	statusStruct *otorgado;
	statusStruct *sePudoMover;
	void  *elementoBorrado;
	recursosPersonajesStruct *unPersonaje;
	recursoStruct *r;
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	log_info(log3,"Recursos disponiibles al iniciar el nivel");
	mostrarDisponibles(log3);
	socketServidor = socketCreateServerAleatorio();
	socketListen(socketServidor);


	if (getsockname(socketServidor, (struct sockaddr *)&sin, &len) == -1)
	    perror("getsockname");
	else
	    log_info(log,"port number %d\n", ntohs(sin.sin_port));
	int32_t variable =  ntohs(sin.sin_port);
	log_info(log,"Variable :%d",variable);
	nivelPaquetizado->nivel.puerto = variable;
	log_info(log,"El puerto que se va a mandar:%d",nivelPaquetizado->nivel.puerto);

	socketOrquestador = socketCreateClient(getIp(nivel.orquestador), getPort(nivel.orquestador));
			if(socketOrquestador !=  0)
			{
				socketSend(socketOrquestador, nivelPaquetizado, sizeof(nivelPaquetizadoStruct));
				log_info(log, "Envie mis datos al orquestador");
			}
			socketDestroyClient(socketOrquestador);



	if (!listaPersonajes)
		listaPersonajes = list_create();

	while (1)
		{
			/* Cuando un cliente cierre la conexión, se pondrá un -1 en su descriptor
			 * de socket dentro del array socketCliente. La función compactaClaves()
			 * eliminará dichos -1 de la tabla, haciéndola más pequeña. */

			/* Se inicializa descriptoresLectura */
			FD_ZERO (&descriptoresLectura);

			/* Se añade para select() el socket servidor */
			FD_SET (socketServidor, &descriptoresLectura);
			/* Se añaden para select() los sockets con los clientes ya conectados */

			for (i=0; i < numeroPersonajes ; i++)
			{
				descriptorActual = -1;
							if (numeroPersonajes > 0)
							{
								unPersonaje = (recursosPersonajesStruct *) list_get(listaPersonajes,i);
								descriptorActual = unPersonaje->descriptor;
							}
				FD_SET (descriptorActual, &descriptoresLectura);
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
			for (i=0; i < numeroPersonajes ; i++)
			{
				descriptorActual = -1;
				if (numeroPersonajes > 0)
				{
					unPersonaje = (recursosPersonajesStruct *) list_get(listaPersonajes,i);
					descriptorActual = unPersonaje->descriptor;
				}

				if (FD_ISSET (descriptorActual, &descriptoresLectura))
				{
					/* Se lee lo enviado por el cliente */

					if (recv(unPersonaje->descriptor, &header, sizeof(header_t), MSG_WAITALL))
					{

						if(header.id == 1)  //Pedido de Posicion Recurso del Personaje
						{

							posicion = malloc(sizeof(posicionesEnNivel));
							mensajeRecurso = malloc(header.length);
							recv(unPersonaje->descriptor, mensajeRecurso, header.length, MSG_WAITALL);
							posicion = buscarRecurso((char) mensajeRecurso->recurso);

							log_info(log, "Recibi un pedido de busqueda de %c por %c",(char) mensajeRecurso->recurso,unPersonaje->simbolo);
							socketSend(unPersonaje->descriptor, posicion, sizeof(posicionesEnNivel));

							free(posicion);
							free(mensajeRecurso);
						}
						if(header.id == 2)  //Movimiento del personaje (para dibujar)
						{
							sePudoMover = malloc(sizeof(statusStruct));
							mensajePosicion = malloc(header.length);
							recv(unPersonaje->descriptor, mensajePosicion, header.length, MSG_WAITALL);

							pthread_mutex_lock(&controlListaPersonajes);
							sePudoMover->status = MoverPersonaje(ListaItems, (char) mensajePosicion->simbolo, mensajePosicion->x, mensajePosicion->y,rows,cols);
							unPersonaje->posicion.posX= mensajePosicion->x;
							unPersonaje->posicion.posY= mensajePosicion->y;
							if (sePudoMover->status == TRUE)
							{
								nivel_gui_dibujar(ListaItems);
								log_info(log, "Muevo personaje %c",unPersonaje->simbolo);
							}
							pthread_mutex_unlock(&controlListaPersonajes);


							socketSend(unPersonaje->descriptor, sePudoMover, sizeof(statusStruct));


							free(mensajePosicion);
							free(sePudoMover);
						}
						if(header.id == 3) //Pedido de asignacion del recurso
						{

							mensajeRecurso = malloc(header.length);
							recv(unPersonaje->descriptor, mensajeRecurso, header.length, MSG_WAITALL);

							//Elimino recurso (validar que este en la posicion, y luego elimiar de la cola)
							posicion = buscarRecurso((char) mensajeRecurso->recurso);
							log_info(log,"Recibi un pedido de otorgacion de %c a %c",(char) mensajeRecurso->recurso,unPersonaje->simbolo);

							//Confirmo que este en el casillero correspondiente
							pthread_mutex_lock(&controlListaPersonajes);
							if ((posicion->posX != unPersonaje->posicion.posX) || (posicion->posY != unPersonaje->posicion.posY))
							{
								log_info(log,"ERROR DE CASILLERO");
								noDisponible = TRUE;

							}
							else
							{
								noDisponible = eliminarRecurso(ListaItems,((char) mensajeRecurso->recurso));
							}

							pthread_mutex_unlock(&controlListaPersonajes);

							if(noDisponible == FALSE)
							{
								otorgado=malloc(sizeof(statusStruct));
								otorgado->status = 1;		//Confirmando otorgacion
								socketSend(unPersonaje->descriptor, otorgado, sizeof(statusStruct));
								log_info(log,"El recurso %c fue otorgado a  %c",(char) mensajeRecurso->recurso,unPersonaje->simbolo);
								log_info(log3,"El recurso %c fue otorgado a  %c",(char) mensajeRecurso->recurso,unPersonaje->simbolo);
								r = malloc(sizeof(recursoStruct));
								r->recurso = mensajeRecurso->recurso;
								queue_push(unPersonaje->recursos,r);
								free(otorgado);
							}
							else
							if(noDisponible == TRUE)
							{
								otorgado=malloc(sizeof(statusStruct));
								otorgado->status = 0;		//No hay recurso, se bloquea
								socketSend(unPersonaje->descriptor, otorgado, sizeof(statusStruct));
								log_info(log,"Rechazo otorgacion de recurso %c a %c",mensajeRecurso->recurso,unPersonaje->simbolo);
								log_info(log3,"%c se bloqueo esperando recurso %c que no esta disponible",unPersonaje->simbolo,mensajeRecurso->recurso);
								unPersonaje->recursoEnElQueSeBloqueo = mensajeRecurso->recurso;
								free(otorgado);

							}
							free(mensajeRecurso);
						}
						if (header.id == 4) //Recepcion del plan de nivel del personaje
						{
							queue_clean(unPersonaje->recursosMaximos);
							planDeNivelesStruct *plan;
							void *data;
							log_info(log, "Recibi el plan del nivel del personaje %c",unPersonaje->simbolo);
							data = malloc(header.length);
							recv(unPersonaje->descriptor, data, header.length, MSG_WAITALL);
							plan = desserializador_planDeNiveles(data,header.length);
							queue_copy(unPersonaje->recursosMaximos,plan->cola);
						}
					}
					else
					{
						 /*Se indica que el cliente ha cerrado la conexión y se
						 * marca con -1 el descriptor para que compactaClaves() lo
						 * elimine */
						log_info(log,"Cliente %c ha cerrado la conexión", unPersonaje->simbolo);
						log_info(log3,"Cliente %c ha cerrado la conexión", unPersonaje->simbolo);
						log_info(log3,"Libero los recursos de %c",unPersonaje->simbolo);
						pthread_mutex_lock(&controlListaPersonajes);
						liberarYAsignarRecursos(unPersonaje, log);
						pthread_mutex_unlock(&controlListaPersonajes);
						shutdown(unPersonaje->descriptor,2);
						pthread_mutex_lock(&controlListaPersonajes);
						BorrarItem(&ListaItems, unPersonaje->simbolo);
						nivel_gui_dibujar(ListaItems);
						elementoBorrado = list_remove(listaPersonajes, i);
						numeroPersonajes = list_size(listaPersonajes);
						pthread_mutex_unlock(&controlListaPersonajes);
						free(elementoBorrado);

					}
				}
			}

			/* Se comprueba si algún cliente nuevo desea conectarse y se le
			 * admite */
			if (FD_ISSET (socketServidor, &descriptoresLectura))
			{
				socketNuevoPersonaje = socketAccept(socketServidor);
				recv(socketNuevoPersonaje, &identificador, sizeof(int8_t), MSG_WAITALL);

				if(identificador == 1) // Un personaje
				{
				recv(socketNuevoPersonaje, &simbolo, sizeof(char), MSG_WAITALL);

				bool existePersonaje(void* personaje)
				{
						return (((recursosPersonajesStruct*) personaje)->simbolo == simbolo );
				}

				recursosPersonajesStruct *personajeDuplicado = list_find((listaPersonajes), existePersonaje);

				if(personajeDuplicado != NULL)
				{
					log_info(log,"Cliente %c ha cerrado la conexión.", personajeDuplicado->simbolo);
					log_info(log3,"Cliente %c ha cerrado la conexión.", personajeDuplicado->simbolo);
					log_info(log3,"Libero los recursos de %c",unPersonaje->simbolo);
					pthread_mutex_lock(&controlListaPersonajes);
					liberarYAsignarRecursos(personajeDuplicado, log);
					pthread_mutex_unlock(&controlListaPersonajes);
					shutdown(personajeDuplicado->descriptor,2);
					pthread_mutex_lock(&controlListaPersonajes);
					BorrarItem(&ListaItems, personajeDuplicado->simbolo);
					nivel_gui_dibujar(ListaItems);
					elementoBorrado = list_remove_by_condition(listaPersonajes,existePersonaje);
					numeroPersonajes = list_size(listaPersonajes);
					pthread_mutex_unlock(&controlListaPersonajes);
					free(elementoBorrado);
				}

				log_info(log, "Se conecto un personaje nuevo. Simbolo: %c",simbolo);
				//Nuevo Personaje
				unPersonaje = malloc(sizeof(recursosPersonajesStruct));
				unPersonaje->simbolo = simbolo;
				unPersonaje->recursos = queue_create();
				unPersonaje->recursosMaximos= queue_create();
				unPersonaje->posicion.posX=0;
				unPersonaje->posicion.posY=0;
				unPersonaje->descriptor = socketNuevoPersonaje;
				unPersonaje->recursoEnElQueSeBloqueo = '\0';
				pthread_mutex_lock(&controlListaPersonajes);
				list_add(listaPersonajes, (void *) unPersonaje);
				numeroPersonajes = list_size(listaPersonajes);
				log_info(log, "Numero de Personajes Conectados: %d", numeroPersonajes);

				CrearPersonaje(&ListaItems, simbolo, 0, 0);
		    	nivel_gui_dibujar(ListaItems);
		    	pthread_mutex_unlock(&controlListaPersonajes);

				}
				else
					if(identificador == 2) //Plataforma
					{
						log_info(log,"Plataforma me indica que me cierre");
						kill(getpid(),9);
					}
					else
						if(identificador == 3) //Plataforma informando personaje que mato por interbloqueo
						{
							char simboloAMatar;
							recv(socketNuevoPersonaje, &simboloAMatar, sizeof(char), MSG_WAITALL);

							log_info(log,"La plataforma me dijo que mato a %c",simboloAMatar);
							log_info(log3,"La plataforma me dijo que mato a %c",simboloAMatar);
						}
			}
		}
		return 0;
}



int32_t dameMax(t_list *lista)
{
	recursosPersonajesStruct *unElemento;
	int32_t i;
	int32_t max;

	if (list_size(lista) < 1)
		return 0;

	unElemento = (recursosPersonajesStruct *) list_get(lista,0);
	max = unElemento->descriptor;
	for (i=0; i<list_size(lista); i++)
	{
		unElemento = (recursosPersonajesStruct *) list_get(lista,i);
		if (unElemento->descriptor > max)
			max = unElemento->descriptor;
	}

	return max;
}



void rutinaSignal(int32_t n)
{
	int32_t socketOrquestador;
	switch (n)
	{
		case SIGINT:
			socketOrquestador = socketCreateClient(getIp(nivel.orquestador), getPort(nivel.orquestador));
			numNivelStruct *header = malloc(sizeof(numNivelStruct));
			header->header.id=4;  //Avisa que se cierra
			header->header.length= sizeof(char[30]);
			strcpy(header->nivel,nivel.nombre);
			socketSend(socketOrquestador,header,sizeof(numNivelStruct));
			free(header);
			kill(getpid(),9);
			break;
	}


}

