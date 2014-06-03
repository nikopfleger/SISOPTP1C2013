#ifndef NIVEL_H_
#define NIVEL_H_


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "tad_items.h"
#include "so-commons-library/collections/queue.h"
#include "so-commons-library/log.h"

typedef struct {
	int32_t recurso;
} unRecursoPaquetizadoStruct ;

typedef struct {
	int8_t id;
	int16_t length;
} header_t;

//Estructuras para levantar el nivel y sus cajas
typedef struct {
  char nombre[30];
  char simbolo;
  int32_t instancias;
  int32_t posX;
  int32_t posY;
} cajaStruct ;

typedef struct {
	char recurso;
} __attribute__((__packed__)) recursoStruct ;

typedef struct {
	char nombre[30];
	t_list *cajas;
	char orquestador[30];
	int32_t tiempoChequeoDeadlock;
	int32_t recovery;
} nivelConfigStruct;
//Fin estructuras nivel

typedef struct {
	uint16_t puerto;
	char nombre[30];
} payloadNivelStruct;

typedef struct {
		char nombre[25];
		char simbolo[2];
		int16_t vidas;
		int16_t nivel;
} personajeStruct;

typedef struct {
	int8_t id;
	int16_t length;
	payloadNivelStruct nivel;

} nivelPaquetizadoStruct ;

typedef struct
{
	int8_t tipo;
	char personaje;
	char *mensaje;
} mensajesDePersonaje;

typedef struct
{
	int8_t posX;
	int8_t posY;
} posicionesEnNivel ;

typedef struct
{
	char id;
	posicionesEnNivel pos;
} personajeEnNivel ;


//Estructuras para comunicacion con Personaje


typedef struct {
	int16_t recurso;
} payloadRecursoStruct;

typedef struct {
	int8_t x;
	int8_t y;
	int8_t simbolo;
} payloadPosicionStruct;

typedef struct {
	char simbolo;
	int32_t descriptor;
	posicionesEnNivel posicion;
	t_queue * recursosMaximos;
	t_queue *recursos;
	char recursoEnElQueSeBloqueo;
} recursosPersonajesStruct;

typedef struct{
	int8_t status;
} __attribute__((__packed__)) statusStruct;

typedef struct {
        t_queue *cola;
} __attribute__((__packed__)) liberadosNoSerializadoStruct;

typedef struct{
	header_t header;
	char nivel[30];
} __attribute__((__packed__)) numNivelStruct;

typedef struct {
        char simbolo;
        char recurso;
} __attribute__((__packed__)) personajeYRecursoAsignadoStruct;

typedef struct {
        int32_t cantidadPersonajes;
        t_queue *cola;
} __attribute__((__packed__)) colaNoSerializadaStruct;

typedef struct {
        t_queue *cola;
} __attribute__((__packed__)) planDeNivelesStruct;

typedef struct {
	char simbolo;
} __attribute__((__packed__)) simboloStruct;

typedef struct {
char personaje;
char nivelNombre[30];
} __attribute__((__packed__)) nivelYPersonaje;

//Fin estructuras personaje

extern nivelConfigStruct nivel;
extern ITEM_NIVEL *ListaItems;
extern t_list *listaPersonajes;
extern char logName[20];
extern int32_t rows,cols;

void rutinaSignal(int32_t n);
char *getIp (char c[]);
int32_t getPort (char c[]);
cajaStruct *getInfoCaja(char *c);
ITEM_NIVEL * crearNivel(nivelConfigStruct nivel);
void * serializador_liberados(liberadosNoSerializadoStruct *self,int32_t *size);
planDeNivelesStruct *desserializador_planDeNiveles(void *data,int16_t length);
void arregloDePersonajesAdd(personajeEnNivel p);
posicionesEnNivel * buscarRecurso(char r);
int32_t eliminarRecurso(ITEM_NIVEL *ListaItems,char c);
void agregarRecurso(ITEM_NIVEL *ListaItems,char c);
void agregarRecursosLista(ITEM_NIVEL *ListaItems,t_queue *cola);
void eliminarDeCola(personajeYRecursoAsignadoStruct *unRecurso,t_queue *queueLiberados);
int32_t servidorNivel();
int32_t dameMax(t_list *lista);
colaNoSerializadaStruct *desserializador_recursos(void *data);
void *serializador_interbloqueo(colaNoSerializadaStruct *self,int16_t *size);
char* itoa(int32_t i, char b[]);
int32_t get_int_len (int32_t value);

t_queue * vaciarColaRecursos(t_queue *recursos, int32_t *f, int32_t *m, int32_t *h);
t_queue* enviarRecursoOrquestador(recursosPersonajesStruct *unPersonaje);

//Funciones para interbloqueo
void *chequearInterbloqueo(void *arg);
void algoritmoInterbloqueo(int32_t numeroPersonajes);
t_list * obtenerMatrizDisponibles();
t_list * obtenerMatrizAsignados(t_list *listaPersonajes,int32_t numeroPersonajes);
t_list * obtenerPeticionesActuales(t_list *listaPersonajes,int32_t numeroPersonajes);
t_list *obtenerListaSimbolos(t_list *listaPersonajes,int32_t numeroPersonajes);
void aniadirYeliminar(t_list *disponibles,t_list * asignados, int32_t proc);

void mostrarMatrizPeticiones(t_list *matriz,t_list *simbolosPersonajes);
void mostrarMatrizASIG(t_list *matriz,t_list *simbolosPersonajes);
void disponiblesMatriz(t_list *matriz);
void mostrarDisponibles(t_log *log3);


void liberarYAsignarRecursos(recursosPersonajesStruct *unPersonaje, t_log *log);

#endif /* NIVEL_H_ */
