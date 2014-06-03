/*
 * nivelauxiliares.c
 *
 *  Created on: May 4, 2013
 *      Author: niko
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nivel.h"
#include <stdint.h>
#include "so-commons-library/sockets.h"
#include "so-commons-library/log.h"
#include <unistd.h>
#include <sys/socket.h>


#define TRUE 1
#define FALSE 0

int32_t rows,cols;

void element_destroyer(void *element) {
        list_destroy(element);
}
//Cantidad maxima de personajes que coincide con la cantidad maxima de clientes

char * getIp (char c[])
{
        char *aux = strdup(c);
        aux = strtok(aux,":");
        return aux;
}

int32_t getPort (char c[])
{
        char *aux = strdup(c);
        aux = strtok(aux,":");
        aux = strtok(NULL,":");
        return atoi(aux);
}

char* itoa(int32_t i, char b[]){
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i = -1;
    }
    int32_t shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

int32_t get_int_len (int32_t value){
  int32_t l=1;
  while(value>9){ l++; value/=10; }
  return l;
}


cajaStruct * getInfoCaja(char *c) {
        char *aux = strdup(c);
        cajaStruct *caja;
        caja = malloc(sizeof(cajaStruct));
        int32_t counter=0;
        char *tokenizedSubstring;
        tokenizedSubstring = strtok(aux,",");
 while (tokenizedSubstring != NULL)
          {
                  switch (counter)  {
                                  case 0:
                                          strcpy(caja->nombre,tokenizedSubstring);
                                          break;
                                  case 1:
                                          caja->simbolo = tokenizedSubstring[0];
                                          break;
                                  case 2:
                                          caja->instancias = atoi(tokenizedSubstring);
                                          break;
                                  case 3:
                                          caja->posX = atoi(tokenizedSubstring);
                                          break;
                                  case 4:
                                          caja->posY = atoi(tokenizedSubstring);
                                          break;
                                  }
            tokenizedSubstring = strtok (NULL, ",");
            counter++;
          }
          return caja;
}

ITEM_NIVEL* crearNivel(nivelConfigStruct nivel) {

        ITEM_NIVEL* ListaItems = NULL;
        int32_t i;
        cajaStruct *caja;
        int32_t r,c;

                nivel_gui_inicializar();
                nivel_gui_get_area_nivel(&r, &c);
                rows = r;
                cols = c;

                for (i = 0; i < list_size(nivel.cajas) ; i++)
                {
                        caja = list_get(nivel.cajas,i);
                        CrearCaja(&ListaItems,caja->simbolo,caja->posX,caja->posY,caja->instancias);
                }

return ListaItems;

}

posicionesEnNivel* buscarRecurso(char r) {
        posicionesEnNivel *p;
        cajaStruct *unRecurso;
        p = malloc(sizeof(posicionesEnNivel));

        bool condicionRecurso(void* recurso) {
                        return (((cajaStruct*) recurso)->simbolo == r);
        }

        unRecurso = (cajaStruct *) list_find(nivel.cajas,condicionRecurso);


        if (unRecurso != NULL)
        {
                p->posX = unRecurso->posX;
                p->posY = unRecurso->posY;
        }
        else
        {
                p->posX = -1;
                p->posY = -1;
        }
        return p;
}

int32_t eliminarRecurso(ITEM_NIVEL *ListaItems,char c)
{
        cajaStruct *unRecurso;

        bool condicionRecurso(void* recurso) {
                        return (((cajaStruct*) recurso)->simbolo == c);
        }

        unRecurso = (cajaStruct *) list_find(nivel.cajas,condicionRecurso);

        if (unRecurso != NULL) {
                if(unRecurso->instancias == 0)
                        return 1;
                unRecurso->instancias--;
        }
        else {
                perror("recurso invalido");
                return -1;
        }

        restarRecurso(ListaItems,c);
        nivel_gui_dibujar(ListaItems);
        return 0;
}

void eliminarDeCola(personajeYRecursoAsignadoStruct *unRecurso,t_queue *queueLiberados)
{
        t_queue *aux = queue_create();

        recursoStruct *recAux;

        while (!queue_is_empty(queueLiberados))
        {
                recAux = queue_pop(queueLiberados);
                if (recAux->recurso != unRecurso->recurso)
                {
                        queue_push(aux,recAux);
                }
                else
                        break;
        }

        while(!queue_is_empty(queueLiberados))
        {
                recAux = queue_pop(queueLiberados);
                queue_push(aux,recAux);
        }

        queue_copy(queueLiberados,aux);
        queue_destroy(aux);
}

void agregarRecursosLista(ITEM_NIVEL *ListaItems,t_queue *cola)
{
        t_log *log = log_create(logName,"Nivel",FALSE,LOG_LEVEL_INFO);
        recursoStruct *unRecurso;
        cajaStruct *unaCaja;

        bool condicionAgregar(void* caja) {
                return (((cajaStruct*) caja)->simbolo == unRecurso->recurso);
        }

        while (!queue_is_empty(cola))
        {
                unRecurso = queue_pop(cola);

                unaCaja = list_find(nivel.cajas,condicionAgregar);
                unaCaja->instancias++;
                sumarRecurso(ListaItems,unaCaja->simbolo);
                nivel_gui_dibujar(ListaItems);
        }
        log_destroy(log);
}

t_queue* enviarRecursoOrquestador(recursosPersonajesStruct *unPersonaje)
{
        t_log *log = log_create(logName,"Nivel",FALSE,LOG_LEVEL_INFO);
        int32_t length,size;
        liberadosNoSerializadoStruct colaLib;
        colaNoSerializadaStruct *colaAsig;
        nivelYPersonaje nivelYPersonaje;
        colaLib.cola=unPersonaje->recursos;
        void *data = serializador_liberados(&colaLib,&size);
        length = size + sizeof(header_t);

        int32_t socketOrquestador = socketCreateClient(getIp(nivel.orquestador), getPort(nivel.orquestador));


        nivelYPersonaje.personaje = unPersonaje->simbolo;
        strcpy(nivelYPersonaje.nivelNombre,nivel.nombre);

        socketSend(socketOrquestador, data, length);
        socketSend(socketOrquestador, &(nivelYPersonaje), sizeof(char[30]));

        recv(socketOrquestador,&length,sizeof(int32_t),MSG_WAITALL);
        void *buffer = malloc(length);
        recv(socketOrquestador,buffer,length,MSG_WAITALL);
        colaAsig = desserializador_recursos(buffer);

        shutdown(socketOrquestador,2);
        log_destroy(log);
        return colaAsig->cola;
}



t_list * obtenerMatrizAsignados (t_list *listaPersonajes,int32_t numeroPersonajes) {

 t_list *asignados,*recursos;
 recursosPersonajesStruct *unPersonaje;
 unRecursoPaquetizadoStruct *instancia;
 int32_t i,j;
 char unSimbolo;

 asignados = list_create();

 bool condicionAsignados(void* simbolo) {
        return (((recursoStruct *) simbolo)->recurso == unSimbolo);
 }

 for (i = 0; i < numeroPersonajes; i++)
 {
         unPersonaje = list_get(listaPersonajes,i);
         recursos = list_create();

         for (j=0;j<list_size(nivel.cajas);j++)
         {
                 instancia = malloc(sizeof(unRecursoPaquetizadoStruct));

                 unSimbolo = ((cajaStruct *) list_get(nivel.cajas,j))->simbolo;
                 instancia->recurso = list_size(list_filter(unPersonaje->recursos->elements,condicionAsignados));
                 list_add(recursos,instancia);
         }
         list_add(asignados,recursos);
 }
 return asignados;
}

t_list * obtenerPeticionesActuales(t_list *listaPersonajes,int32_t numeroPersonajes) {
  t_list *peticionesActuales,*recursos;
  recursosPersonajesStruct *unPersonaje;
  unRecursoPaquetizadoStruct *instancia;
  int32_t i,j;
  char unSimbolo;

 peticionesActuales = list_create();
 for (i = 0; i < numeroPersonajes; i++)
 {
         recursos = list_create();
         unPersonaje = list_get(listaPersonajes, i);

         for (j=0;j<list_size(nivel.cajas);j++)
         {
                 instancia = malloc(sizeof(unRecursoPaquetizadoStruct));

                 unSimbolo = ((cajaStruct *) list_get(nivel.cajas,j))->simbolo;
                 if (unSimbolo == unPersonaje->recursoEnElQueSeBloqueo)
                	 instancia->recurso = 1;
                 else
                	 instancia->recurso = 0;

                 list_add(recursos,instancia);

         }
         list_add(peticionesActuales,recursos);
 }

 return peticionesActuales;
}

t_list *obtenerListaSimbolos(t_list *listaPersonajes,int32_t numeroPersonajes) {
        t_list *simbolos = list_create();
        simboloStruct *unSimbolo;
        int32_t i;

        for (i=0; i < numeroPersonajes ; i++)
        {
                unSimbolo = malloc(sizeof(simboloStruct));
                unSimbolo->simbolo = ((recursosPersonajesStruct *) list_get(listaPersonajes,i))->simbolo;
                list_add(simbolos,unSimbolo);
        }
        return simbolos;
}

t_list * obtenerMatrizDisponibles() {
  t_list *disponibles= list_create();
  unRecursoPaquetizadoStruct *instancia;
 int32_t i;

 for (i=0;i<list_size(nivel.cajas);i++)
 {
         instancia = malloc(sizeof(unRecursoPaquetizadoStruct));
         instancia->recurso = ((cajaStruct *) list_get(nivel.cajas,i))->instancias;
     list_add(disponibles,instancia);
 }
 return disponibles;
}

void aniadirYeliminar(t_list *disponibles,t_list *asignados, int32_t proc)
{
        t_log *log2 = log_create("matriz.log","matriz",FALSE,LOG_LEVEL_INFO);
        int32_t k;
        unRecursoPaquetizadoStruct *temp,*temp2;
        t_list *elementoAEliminar,*t;
           for (k = 0; k < list_size(disponibles); k++)
           {
                   t = (t_list *) list_get(asignados,proc);
                   temp = (unRecursoPaquetizadoStruct *) list_get(disponibles,k);
                   temp2 = (unRecursoPaquetizadoStruct *) list_get(t,k);
                   log_info(log2,"tiene: %d recursos de %c",temp2->recurso,((cajaStruct *)list_get(nivel.cajas,k))->simbolo);
                   log_info(log2,"Hay: %d recursos de %c",temp->recurso,((cajaStruct *)list_get(nivel.cajas,k))->simbolo);
                   temp->recurso += temp2->recurso;
                   log_info(log2,"El proceso termina y ahora hay %d recursos de %c",temp->recurso,((cajaStruct *)list_get(nivel.cajas,k))->simbolo);
           }
           elementoAEliminar = (t_list*) list_remove(asignados,proc);
           list_destroy(elementoAEliminar);
           log_destroy(log2);
}



void algoritmoInterbloqueo(int32_t numeroPersonajes)
{
	t_log *log = log_create(logName,"Nivel",FALSE,LOG_LEVEL_INFO);
	t_log *log2 = log_create("matriz.log","matriz",FALSE,LOG_LEVEL_INFO);
	t_list *peticionesActuales, *asignados,*disponibles,*simbolosPersonajes;
	t_list *peticionesPorProceso,*procesoAsignados;
	simboloStruct *unSimbolo;
	void* buffer;
	int32_t nProcesos;
	int32_t socketOrquestador;
	int32_t nRecursos = list_size(nivel.cajas);
	int32_t i;
	colaNoSerializadaStruct colaInterbloqueo;
    int32_t length;
    int16_t size=0;
	unRecursoPaquetizadoStruct *instanciasDisponibles,*instanciasPedidas,*instanciasAsignadas;
	int32_t aux= TRUE, proc,r;
	t_queue *enInterbloqueo;
	if (!listaPersonajes)
			listaPersonajes = list_create();
	nProcesos = numeroPersonajes;
	if (nProcesos > 0)
	{
		disponibles=obtenerMatrizDisponibles();
		asignados=obtenerMatrizAsignados(listaPersonajes,numeroPersonajes);
		peticionesActuales=obtenerPeticionesActuales(listaPersonajes,numeroPersonajes);
		simbolosPersonajes=obtenerListaSimbolos(listaPersonajes,numeroPersonajes);
		mostrarMatrizPeticiones(peticionesActuales,simbolosPersonajes);
		mostrarMatrizASIG(asignados,simbolosPersonajes);
		disponiblesMatriz(disponibles);

		//elimino los que no tienen asignados
		for (proc = 0; proc < nProcesos; proc++)
		{

				procesoAsignados = (t_list *) list_get(asignados,proc);
				 bool condicionCeros(void* recurso) {
						return (((unRecursoPaquetizadoStruct *) recurso)->recurso != 0);
				 }

				if ( list_size(list_filter(procesoAsignados,condicionCeros)) == 0 )
				{
						list_remove_and_destroy_element(peticionesActuales,proc,element_destroyer);
						list_remove_and_destroy_element(asignados,proc,element_destroyer);
						unSimbolo = list_remove(simbolosPersonajes,proc);
						free(unSimbolo);
						nProcesos--;
						proc--;
				}

		}

		aux = TRUE;
		proc = 0;
		if (nProcesos > 0)
		{
			for (proc = 0; proc < nProcesos; proc++ )
		    {
				log_info(log2,"Chequeando proceso %c",((simboloStruct *) list_get(simbolosPersonajes,proc))->simbolo);

			    aux = TRUE;
			    //lista de un solo proceso (como si fuera un arreglo)
			    peticionesPorProceso = (t_list *) list_get(peticionesActuales,proc);
			    procesoAsignados = (t_list *) list_get(asignados,proc);

			    for (r=0; r < nRecursos; r++)
			    {
					instanciasDisponibles = (unRecursoPaquetizadoStruct *) list_get(disponibles,r);
					instanciasPedidas = (unRecursoPaquetizadoStruct *) list_get(peticionesPorProceso,r);
					instanciasAsignadas = (unRecursoPaquetizadoStruct *) list_get(procesoAsignados,r);
					// comprobamos que haya recursos suficientes
					aux = (instanciasPedidas->recurso - instanciasDisponibles->recurso) <= 0;
					if (!aux)
					{
							log_info(log2,"%c todavia necesita %d instancias de %c",((simboloStruct *) list_get(simbolosPersonajes,proc))->simbolo,instanciasPedidas->recurso,((cajaStruct *)list_get(nivel.cajas,r))->simbolo);
							log_info(log2,"hay %d instancias",instanciasDisponibles->recurso);
							break;
					}
			    }

			    if (aux)
			    {
					log_info(log2,"suponemos que %c termina",((simboloStruct *) list_get(simbolosPersonajes,proc))->simbolo);
					aniadirYeliminar(disponibles, asignados, proc);
					//remover indice de maximos

					list_remove_and_destroy_element(peticionesActuales,proc,element_destroyer);
				    unSimbolo = list_remove(simbolosPersonajes,proc);
				    free(unSimbolo);
				    nProcesos--;
				    proc = -1;
				    if (nProcesos == 0)
				    {
						log_info(log,"Es estado seguro");
						log_info(log2,"Es estado seguro");
						log_destroy(log);
						log_destroy(log2);
						list_destroy(disponibles);
						list_destroy(asignados);
						list_destroy(peticionesActuales);
						list_destroy(simbolosPersonajes);
						return;
				    }
			    }
			}
		    log_info(log,"Es Interbloqueo");
		    log_info(log2,"Es Interbloqueo");

		    for (i=0 ; i < list_size(simbolosPersonajes) ; i++)
						   log_info(log,"Personaje en interbloqueo: %c",((simboloStruct *) list_get(simbolosPersonajes,i))->simbolo);

		    if (nivel.recovery == 1)
		    {

				   enInterbloqueo=queue_create();
				   socketOrquestador = socketCreateClient(getIp(nivel.orquestador), getPort(nivel.orquestador));

				   for (i=0 ; i < list_size(simbolosPersonajes) ; i++)
				   {
						   queue_push(enInterbloqueo,list_get(simbolosPersonajes,i));
				   }
				   colaInterbloqueo.cantidadPersonajes=list_size(simbolosPersonajes);
				   colaInterbloqueo.cola = enInterbloqueo;
				   buffer = serializador_interbloqueo(&colaInterbloqueo,&size);
				   length = size + sizeof(header_t);
				   socketSend(socketOrquestador,buffer,length);
				   socketSend(socketOrquestador, &(nivel.nombre), sizeof(char[30]));
				   shutdown(socketOrquestador,2);
				   queue_destroy(enInterbloqueo);
				   free(buffer);
		    }
	    }
		list_destroy(disponibles);
		list_destroy(asignados);
		list_destroy(peticionesActuales);
		list_destroy(simbolosPersonajes);
	}
	log_destroy(log);
	log_destroy(log2);
}

void *serializador_interbloqueo(colaNoSerializadaStruct *self,int16_t *size)
{
	*size = sizeof(int32_t) + queue_size(self->cola) * sizeof(simboloStruct);
    void *data = malloc(*size + sizeof(header_t));
    int16_t offset = 0, tmp_size = 0;
    //HEADER ID PARA INTERBLOQUEO
    header_t header;
    header.id=5;
    header.length=*size;

    t_queue *aux=queue_create();
    simboloStruct *unSimbolo;
    queue_copy(aux,self->cola);
    int32_t personajes = self->cantidadPersonajes;

    memcpy(data, &header, tmp_size = sizeof(header_t));
    offset = tmp_size;
    memcpy(data + offset, &personajes, tmp_size = sizeof(int32_t));
    offset += tmp_size;
    while (!queue_is_empty(aux))
    {
			unSimbolo = (simboloStruct*) queue_pop(aux);
            memcpy(data + offset, unSimbolo, tmp_size = sizeof(simboloStruct));
            offset += tmp_size;
    }

    queue_destroy(aux);
    return data;
}

colaNoSerializadaStruct *desserializador_recursos(void *data)
{
        colaNoSerializadaStruct *self = malloc(sizeof(colaNoSerializadaStruct));
        self->cola=queue_create();
        personajeYRecursoAsignadoStruct *elem;
        int32_t i, offset = 0, tmp_size = 0, *personajes;
        personajes = malloc(sizeof(int32_t));

        memcpy(personajes, data, tmp_size = sizeof(int32_t));
        self->cantidadPersonajes = *personajes;


        for (i = 0; i < *personajes; i++)
        {
                offset += tmp_size;
                elem = malloc(sizeof(personajeYRecursoAsignadoStruct));
                memcpy(elem,data + offset, sizeof(personajeYRecursoAsignadoStruct));
                queue_push(self->cola,elem);


                tmp_size=sizeof(personajeYRecursoAsignadoStruct);
        }
        return self;
}

void * serializador_liberados(liberadosNoSerializadoStruct *self,int32_t *size)
{
		*size = queue_size(self->cola) * sizeof(recursoStruct);
		void *data = malloc(*size + sizeof(header_t));
        int32_t offset = 0, tmp_size = 0;

        header_t header;
        header.id = 3;
        header.length=*size;

        t_queue *aux=queue_create();
        recursoStruct *unRecurso;
        queue_copy(aux,self->cola);

        memcpy(data, &header, tmp_size = sizeof(header_t));

        while (!queue_is_empty(aux))
        {
            offset += tmp_size;
                unRecurso = (recursoStruct*) queue_pop(aux);
            memcpy(data + offset, unRecurso, tmp_size = sizeof(recursoStruct));
        }
        queue_destroy(aux);
        return data;
}

planDeNivelesStruct *desserializador_planDeNiveles(void *data,int16_t length)
{

		planDeNivelesStruct *self = malloc(sizeof(planDeNivelesStruct));
        recursoStruct *elem;
        int32_t i, offset = 0, tmp_size = 0;

        self->cola=queue_create();
        char recurso;

        for (i = 0; i < length; i++)
        {
                        offset += tmp_size;
                elem = malloc(sizeof(recursoStruct));
                memcpy(&recurso,data + offset, sizeof(char));
                elem->recurso=recurso;
                queue_push(self->cola,elem);
                tmp_size=sizeof(recursoStruct);
        }
        return self;
}

void mostrarMatrizPeticiones(t_list *matriz,t_list *simbolosPersonajes)
{
        t_log *log = log_create("matriz.log","matriz",FALSE,LOG_LEVEL_INFO);
        t_list *x;
        int32_t i,j;
        unRecursoPaquetizadoStruct *k;

        log_info(log,"MATRIZ PETICIONES NUMERO PERSONAJES %d\n",list_size(matriz));
        for (i=0;i < list_size(matriz);i++)
        {
                x=list_get(matriz,i);
                log_info(log,"PERSONAJE: %c",((simboloStruct *) list_get(simbolosPersonajes,i))->simbolo);
                for (j=0; j < list_size(nivel.cajas) ; j++)
                {
                        k = (unRecursoPaquetizadoStruct *) list_get(x,j);
                        log_info(log,"RECURSO %c: %d",((cajaStruct *) list_get(nivel.cajas,j))->simbolo,k->recurso);
                }
                log_info(log,"\n");
        }
        log_info(log,"\n");
        log_destroy(log);
}

void mostrarMatrizASIG(t_list *matriz,t_list *simbolosPersonajes)
{
        t_log *log = log_create("matriz.log","matriz",FALSE,LOG_LEVEL_INFO);
        t_list *x;
        int32_t i,j;
        unRecursoPaquetizadoStruct *k;

        log_info(log,"MATRIZ ASIG NUMERO PERSONAJES %d\n", list_size(matriz));
        for (i=0;i < list_size(matriz);i++)
        {

                log_info(log,"PERSONAJE: %c",((simboloStruct *) list_get(simbolosPersonajes,i))->simbolo);
                for (j=0; j < list_size(nivel.cajas) ; j++)
                {
                        x=list_get(matriz,i);
                        k = (unRecursoPaquetizadoStruct *) list_get(x,j);
                        log_info(log,"RECURSO %c: %d",((cajaStruct *) list_get(nivel.cajas,j))->simbolo,k->recurso);
                }
                log_info(log,"\n");
        }
        log_info(log,"\n");
        log_destroy(log);
}

void disponiblesMatriz(t_list *matriz)
{
        t_log *log = log_create("matriz.log","matriz",FALSE,LOG_LEVEL_INFO);
        int32_t i;
        unRecursoPaquetizadoStruct *k;
        log_info(log,"MATRIZ DISPONIBLES\n");
        for (i = 0; i < list_size(nivel.cajas); i++)
        {
                k= (unRecursoPaquetizadoStruct *)list_get(matriz,i);
                log_info(log,"RECURSO %c: %d",((cajaStruct *) list_get(nivel.cajas,i))->simbolo,k->recurso);
        }
        log_info(log,"\n");
        log_destroy(log);
}




void liberarYAsignarRecursos(recursosPersonajesStruct *unPersonaje, t_log *log){

        char nombreLogR[50];

        snprintf(nombreLogR,sizeof(nombreLogR),"%s%s","R",logName);

        t_log *log3 = log_create(nombreLogR,"Recursos",FALSE,LOG_LEVEL_INFO);
        t_queue *colaAsignados;
        personajeYRecursoAsignadoStruct *unRecurso;
        recursoStruct *r;
        recursoStruct *recursoAux;
        log_info(log3,"Recursos disponibles antes de liberados:");
        mostrarDisponibles(log3);
        log_info(log,"Los recursos de %c fueron enviados al Orquestador",unPersonaje->simbolo);
        log_info(log3, "Recursos Liberados por %c:",unPersonaje->simbolo);
        t_queue *colaAux = queue_create();
        queue_copy(colaAux,unPersonaje->recursos);

        log_info(log3,"Tamaño de la cola %d",queue_size(colaAux));
        while (!queue_is_empty(colaAux))
        {
                recursoAux=queue_pop(colaAux);

                log_info(log3,"Recurso: %c",recursoAux->recurso);
        }
        queue_destroy(colaAux);

        colaAsignados = enviarRecursoOrquestador(unPersonaje);
        recursosPersonajesStruct *personajeSolicitado;

        while (!queue_is_empty(colaAsignados))
        {
                unRecurso = queue_pop(colaAsignados);
                log_info(log3,"Recursos Asignados del Orquestador a: %c", unRecurso->simbolo);
                log_info(log3,"Recurso: %c",unRecurso->recurso);
                eliminarDeCola(unRecurso,unPersonaje->recursos);
                bool condicion(void* unPersonaje) {
                        return (((recursosPersonajesStruct *) unPersonaje)->simbolo == unRecurso->simbolo);
                }

                personajeSolicitado = (recursosPersonajesStruct *) list_find(listaPersonajes,condicion);
                r = malloc(sizeof(recursoStruct));
                r->recurso = unRecurso->recurso;
                personajeSolicitado->recursoEnElQueSeBloqueo = '\0';
                queue_push(personajeSolicitado->recursos,r);
        }

        agregarRecursosLista(ListaItems, unPersonaje->recursos);

        log_info(log3,"Recursos disponibles despues de liberados:");
        mostrarDisponibles(log3);

        queue_clean(unPersonaje->recursos);

        log_destroy(log3);
        log_info(log, "Libres-Asignados ya estan disponibles");
}

void mostrarDisponibles(t_log *log3)
{
	int z=0;
	for (z=0;z<list_size(nivel.cajas);z++)
		log_info(log3,"Recurso %c: %d",((cajaStruct *) list_get(nivel.cajas,z))->simbolo,((cajaStruct *) list_get(nivel.cajas,z))->instancias);
}
