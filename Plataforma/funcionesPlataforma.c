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


#define FALSE 0
#define TRUE 1

int32_t dameMax(t_list *lista)
{
	listaPersonajesStruct *unElemento;
	int32_t i;
	int32_t max;

	if (list_size(lista) < 1)
		return 0;

	unElemento = (listaPersonajesStruct *) list_get(lista,0);
	max = unElemento->descriptorPersonaje;
	for (i=0; i<list_size(lista); i++)
	{
		unElemento = (listaPersonajesStruct *) list_get(lista,i);
		if (unElemento->descriptorPersonaje > max)
			max = unElemento->descriptorPersonaje;
	}

	return max;
}

void logearCola(t_queue *cola,t_log *log,char info){
	int j;
	listaPersonajesStruct *personajeEnCola;
	char *nombres;
	char *espacio=" ";
	nombres=calloc(1, sizeof(char[26])* queue_size(cola) + 1);
	nombres[0] = 0;
	for(j=0; j<queue_size(cola);j++)
	{
		personajeEnCola = queue_get(cola,j);
		strcat(nombres,personajeEnCola->nombre);
		strcat(nombres,espacio);
	}
	if(info == 'L')
		log_info(log,"Listos:%s",nombres);
	else
	if(info == 'B')
		log_info(log,"Bloqueados:%s\n",nombres);
	free(nombres);
}


int sacarDeCola(t_queue *cola,listaPersonajesStruct *unPersonaje,char info , t_log *log, pthread_mutex_t *semColas)
{
	t_queue *colaAuxiliar;
	int loEncontre = 0;
	listaPersonajesStruct *personajeEnCola;
	colaAuxiliar = queue_create();

	pthread_mutex_lock(semColas);
	while(!queue_is_empty(cola))
	{
		personajeEnCola = queue_pop(cola);
		if(personajeEnCola->nombre != unPersonaje->nombre)
		{
			queue_push(colaAuxiliar, (void *) personajeEnCola);
		}
		else
		{
			if(info == 'L')
				log_info(log,"Borre de Listos a %s",unPersonaje->nombre);
			if(info == 'B')
				log_info(log,"Borre de Bloqueados a %s",unPersonaje->nombre);
			loEncontre = 1;
		}
	}

	queue_copy(cola,colaAuxiliar);
	pthread_mutex_unlock(semColas);
	queue_destroy(colaAuxiliar);

	return loEncontre;
}

int pasarAOtraCola(t_queue *colaListos,t_queue *colaBloqueados, listaPersonajesStruct *unPersonaje, t_log *log, pthread_mutex_t *semColas)
{
		t_queue *colaAuxiliar;
		listaPersonajesStruct *personajeEnCola;
		colaAuxiliar = queue_create();
		int loEncontro = 0;
		pthread_mutex_lock(semColas);
		while(!queue_is_empty(colaBloqueados))
			{
				personajeEnCola = queue_pop(colaBloqueados);
				if(personajeEnCola->nombre != unPersonaje->nombre)
					queue_push(colaAuxiliar, (void *) personajeEnCola);
				else
				{
					log_info(log,"%s estaba bloqueado y reinicio el nivel. Ahora esta listo",personajeEnCola->nombre);
					queue_push(colaListos, (void *) personajeEnCola);
					loEncontro = 1;
				}
			}

		queue_copy(colaBloqueados,colaAuxiliar);

		pthread_mutex_unlock(semColas);
		return loEncontro;
}


void llenarBuffer(payloadNivelPlanificadorStruct *buffer, nivelPlanificadorStruct *nivelSolicitado)
{
	strcpy(buffer->ipNivel, nivelSolicitado->ipNivel);
	buffer->puertoNivel = nivelSolicitado->puertoNivel;
	buffer->puertoPlanificador=nivelSolicitado->puertoPlanificador;
}

void llenarUnNivel(nivelPlanificadorStruct *unNivel, payloadNivelStruct *nivel,struct sockaddr_in info)
{
	strcpy(unNivel->nivel,nivel->nombre);
	strcpy(unNivel->ipNivel,inet_ntoa(info.sin_addr)); //inet_ntoa(info.sin_addr)
	unNivel->puertoNivel=nivel->puerto;
	unNivel->estado=1; //Le aviso a la plataforma que tiene que lanzar el hilo planificador
	unNivel->listos= queue_create();
	unNivel->bloqueados=queue_create();
}

void llenarUnPersonaje(listaPersonajesStruct *nuevoPersonaje, nombreYSimboloStruct *newPersonaje, char nombreNivel[] , int socketNuevoPersonaje)
{
	nuevoPersonaje->descriptorPersonaje = socketNuevoPersonaje;
	strcpy(nuevoPersonaje->nivelDondeEsta , nombreNivel);
	nuevoPersonaje->recursoQueLoBloquea='N';
	strcpy(nuevoPersonaje->nombre, newPersonaje->nombre);
	nuevoPersonaje->simbolo = newPersonaje->simbolo;
}

/*Saca el primero de la cola
 * y le envia un mensaje de Movimiento Permitido
 * */
void movimientoPermitido(t_queue *cola, int *HayAlguienMoviendose,quantumActualStruct *quantumActual, t_log *log, pthread_mutex_t *semColas)
{
	if (!queue_is_empty(cola))
	{
		*HayAlguienMoviendose = TRUE;

		listaPersonajesStruct  *unPersonaje;
		pthread_mutex_lock(semColas);
		unPersonaje = (listaPersonajesStruct *)queue_pop(cola);
		pthread_mutex_unlock(semColas);

		mensajePersonaje *mensajeAEnviar = malloc(sizeof(mensajePersonaje));
		mensajeAEnviar->idMensaje = 1;
		socketSend(unPersonaje->descriptorPersonaje, mensajeAEnviar , sizeof(mensajePersonaje));
		//log_info(log, "Movimiento a %s", unPersonaje->nombre);
		quantumActual->personaje = *unPersonaje;
		quantumActual->quantumActual = 1;
		free(mensajeAEnviar);
		usleep(Retardo);

	}
	else
	{
		strcpy(quantumActual->personaje.nombre , " ");
		log_error(log,"La Cola de Listos está vacía\n");
	}

}

int movimientoPermitidoPorQuantum(quantumActualStruct *quantumActual, t_log *log)
{
	mensajePersonaje *mensajeAEnviar = malloc(sizeof(mensajePersonaje));
	mensajeAEnviar->idMensaje = 1;
	socketSend(quantumActual->personaje.descriptorPersonaje, mensajeAEnviar , sizeof(mensajePersonaje));
	//log_info(log, "Movimiento a %s", quantumActual->personaje.nombre);
	quantumActual->quantumActual = quantumActual->quantumActual + 1;
	free(mensajeAEnviar);
	usleep(Retardo);
	return 0;
}


void * serializador_recursos(colaNoSerializadaStruct *self,int32_t *size) {
		*size = sizeof(int32_t) + queue_size(self->cola) * sizeof(personajeYRecursoAsignadoStruct);
        void *data = malloc(*size + sizeof(int32_t));
        int32_t offset = 0, tmp_size = 0;
        t_queue *aux=queue_create();
        personajeYRecursoAsignadoStruct *unRecurso;
        queue_copy(aux,self->cola);

        int32_t personajes = self->cantidadPersonajes;
        memcpy(data, size, tmp_size = sizeof(int32_t));
        offset = tmp_size;
        memcpy(data + offset, &personajes, tmp_size = sizeof(int32_t));
        offset += tmp_size;
        while (!queue_is_empty(aux))
        {
        		unRecurso = queue_pop(aux);
                memcpy(data + offset, unRecurso, tmp_size = sizeof(personajeYRecursoAsignadoStruct));
                offset += tmp_size;
        }

        queue_destroy(aux);
        return data;
}

liberadosNoSerializadoStruct *desserializador_liberados(void *data,int16_t length)
{
		liberadosNoSerializadoStruct *self = malloc(sizeof(liberadosNoSerializadoStruct));
        recursoStruct *elem;
        int32_t i, offset = 0, tmp_size = 0;
        self->cola=queue_create();

        int32_t tamanioCola=length;

        for (i = 0; i < tamanioCola; i++)
        {
        		offset += tmp_size;
                elem = malloc(sizeof(recursoStruct));
                memcpy(elem,data + offset, sizeof(recursoStruct));
                queue_push(self->cola,elem);
                tmp_size=sizeof(recursoStruct);
        }
        return self;
}

colaNoSerializadaStruct *desserializador_interbloqueo(void *data)
{
        colaNoSerializadaStruct *self = malloc(sizeof(colaNoSerializadaStruct));
        simboloStruct *elem;
        int32_t i, offset = 0, tmp_size = 0, *personajes;
        personajes = malloc(sizeof(int32_t));

        offset = tmp_size;
        memcpy(personajes, data+offset, tmp_size = sizeof(int32_t));
        self->cantidadPersonajes = *personajes;
        self->cola=queue_create();
        for (i = 0; i < *personajes; i++)
        {
                offset += tmp_size;
                elem = malloc(sizeof(simboloStruct));
                memcpy(elem,data + offset, sizeof(simboloStruct));
                queue_push(self->cola,elem);
                tmp_size=sizeof(simboloStruct);
        }
        return self;
}



void detectarYMatarPersonaje(t_queue *cola,nivelPlanificadorStruct *nivel)
{
        t_log *log = log_create("Plataforma.log","Orquestador",true,LOG_LEVEL_INFO);
        simboloStruct *unSimbolo;
        listaPersonajesStruct *unPersonaje,*min;
        int32_t orden = 0;
        mensajePersonaje id;
        int socketNivel;
        id.idMensaje=4;

        while (!queue_is_empty(cola))
        {
                unSimbolo = queue_pop(cola);

                bool condicion(void* personaje)
                {
                        return (((listaPersonajesStruct*) personaje)->simbolo == unSimbolo->simbolo);
                }

                log_info(log,"Personaje en Interbloqueo: %c",unSimbolo->simbolo);
                unPersonaje = (listaPersonajesStruct *) list_find(nivel->listaPersonajes,condicion);
                if ((unPersonaje->ordenDeEntrada < min->ordenDeEntrada) || (orden == 0))
                {
                        min=unPersonaje;
                        orden = 1;
                }


        }
        log_info(log,"%s llego tu hora de morir",min->nombre);
        socketSend(min->descriptorPersonaje,&id,sizeof(mensajePersonaje));

        //log_info(nivel->log,"Mate a %s por Interbloqueo",min->nombre);
        sacarDeCola(nivel->bloqueados,min,'B',nivel->log,&(nivel->semColas)); //Esto es lo ultimo que agregamos. Ver si no perjudica en otro caso!!



        socketNivel = socketCreateClient(nivel->ipNivel, nivel->puertoNivel);
        personajeQueMurioStruct *identificador = malloc(sizeof(personajeQueMurioStruct));
		identificador->id=3;
		identificador->simbolo=min->simbolo;
		socketSend(socketNivel,identificador,sizeof(personajeQueMurioStruct));
		free(identificador);
		log_destroy(log);

}
