#ifndef PLATAFORMA_H_
#define PLATAFORMA_H_


#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include "so-commons-library/collections/queue.h"

//Estructuras
typedef struct {
		char nombre[25];
		char simbolo;
		int16_t vidas;
		char nivel[30];
	} personajeStruct;

typedef struct
{
	int8_t id;
	int16_t length;
} header_t;

typedef struct {
	uint16_t puerto;
	char nombre[30];
} payloadNivelStruct;

typedef struct {
	char ipNivel[16];
	uint16_t puertoNivel;
	int16_t puertoPlanificador;
} __attribute__((__packed__))  payloadNivelPlanificadorStruct;

typedef struct {
	int8_t idMensaje;
}  __attribute__((__packed__)) mensajePersonaje;

typedef struct {
	int8_t idMensaje;
}  __attribute__((__packed__)) statusStruct;

//Estructura para recibir recurso que bloquea al personaje
typedef struct {
	int8_t recursoQueLoBloquea;
} payloadPersonajeBloqueado;



//Estructura de Cola de Bloqueados
typedef struct {
	int16_t socketPersonaje;
	char recursoQueLoBloquea;
	int16_t nivelDondeEstaBloqueado;
} colaBloqueadosStruct;

//Estructura de Cola para Mantener conexion con el nivel
typedef struct {
	t_queue *colaPersonajes;
} colaPersonajesPlanificador;

typedef struct {
	int32_t descriptorPersonaje;
	char simbolo;
	char nombre[25];
	char recursoQueLoBloquea;
	int32_t ordenDeEntrada;
	char nivelDondeEsta[30];
	pthread_mutex_t recursosLiberados;
} listaPersonajesStruct;

typedef struct{
	char nombre[25];
	char simbolo;
} nombreYSimboloStruct;

typedef struct{
	char nivel[30];
} nivelCerrandoStruct;


typedef struct{
	int8_t estado;
	char nivel[30];
	char ipNivel[16];
	uint16_t puertoNivel;
	uint16_t puertoPlanificador;
	t_queue *listos;
	t_queue *bloqueados;
	t_list *listaPersonajes;
	pthread_t hilo;
	pthread_mutex_t semColas;
	t_log *log;
} nivelPlanificadorStruct;

typedef struct {
	listaPersonajesStruct personaje;
	int32_t quantumActual;
} quantumActualStruct;

typedef struct{
	char personaje;
	char nivelNombre[30];
} nivelYPersonajeStruct;

typedef struct {
        char simbolo;
        char recurso;
} __attribute__((__packed__)) personajeYRecursoAsignadoStruct;

typedef struct {
        int32_t cantidadPersonajes;
        t_queue *cola;
} __attribute__((__packed__)) colaNoSerializadaStruct;

typedef struct {
	char simbolo;
} __attribute__((__packed__)) simboloStruct;

typedef struct {
        t_queue *cola;
} __attribute__((__packed__)) liberadosNoSerializadoStruct;

typedef struct {
	char recurso;
} __attribute__((__packed__)) recursoStruct ;

typedef struct{
	char simbolo;
} simboloTerminoStruct;

typedef struct{
	int8_t id;
}__attribute__((__packed__)) identificadorStruct;

typedef struct{
	int8_t id;
	char simbolo;
}__attribute__((__packed__)) personajeQueMurioStruct;

//Fin Estructuras

//Prototipo Función que sigue el Hilo Orquestador
void * orquestar(void* argumento);
//Fin Prototipos

//Prototipo de Función que siguen los Hilos Planificadores
void * planificar(void* argumento);
//Fin Prototipos

//Prototipo de Función que siguen los Hilos Comprobadores de Quantum
void * comprobarQuantum();
//Fin Prototipos

extern int32_t Retardo;

//Prototipos de Funciones usadas en Hilos Planificadores
int dameMaximo (int *tabla, int n);
void compactaClaves (int *tabla, int *n);
void movimientoPermitido(t_queue *cola, int *HayAlguienMoviendose,quantumActualStruct *quantumActual, t_log *log, pthread_mutex_t *semColas);
int movimientoPermitidoPorQuantum(quantumActualStruct *quantumActual, t_log *log);
void logearCola(t_queue *cola,t_log *log,char info);
int sacarDeCola(t_queue *cola,listaPersonajesStruct *unPersonaje,char info , t_log *log, pthread_mutex_t *semColas);

//Prototipo de Funciones usadas por el Orquestador
liberadosNoSerializadoStruct *desserializador_liberados(void *data,int16_t length);
void detectarYMatarPersonaje(t_queue *cola,nivelPlanificadorStruct *nivel);
int32_t asignarRecursosAPersonajesBloqueados(t_queue *recursosLiberados,nivelYPersonajeStruct *nivelYPersonaje, int socketNivel);
void llenarBuffer(payloadNivelPlanificadorStruct *buffer, nivelPlanificadorStruct *nivelSolicitado);
void llenarUnNivel(nivelPlanificadorStruct *unNivel, payloadNivelStruct *nivel,struct sockaddr_in info);
void llenarUnPersonaje(listaPersonajesStruct *nuevoPersonaje, nombreYSimboloStruct *newPersonaje, char nombreNivel[] , int socketNuevoPersonaje);
int pasarAOtraCola(t_queue *colaListos,t_queue *colaBloqueados, listaPersonajesStruct *unPersonaje, t_log *log, pthread_mutex_t *semColas);
void * serializador_recursos(colaNoSerializadaStruct *self,int32_t *size);
colaNoSerializadaStruct *desserializador_interbloqueo(void *data);
int32_t dameMax(t_list *lista);
//Fin Prototipos





#endif /* PLATAFORMA_H_ */

