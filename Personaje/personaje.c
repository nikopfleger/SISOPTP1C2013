#include "so-commons-library/sockets.h"
#include "so-commons-library/string.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include "so-commons-library/log.h"
#include "so-commons-library/collections/queue.h"
#include <signal.h> //Señales
#include <setjmp.h> //Guarda y restaura contexto con señales.
#include <unistd.h> //Sleep
#include <pthread.h> //semaforos
#include <errno.h>

#define true 1
#define false 0

typedef struct {
		char nombre[25];
		char simbolo;
		int16_t vidas;
		char nivel[30];
	} personajeStruct;

typedef struct {
	int8_t id;
	int16_t length;
} header_t;

typedef struct {
	header_t header;
	personajeStruct personaje;
} __attribute__((__packed__)) personajePaquetizadoStruct ;

typedef struct {
	char ipNivel[16];
	uint16_t puertoNivel;
	uint16_t puertoPlanificador;
} payloadNivelPlanificadorStruct;

typedef struct {
	int8_t x;
	int8_t y;
} positionStruct;

typedef struct{
	int16_t r;
}recursoStruct;

typedef struct {
	header_t header;
	recursoStruct recurso;
} __attribute__((__packed__)) pedirRecursoPaquetizadoStruct ;

typedef struct {
	int8_t idMensaje;
} mensajePersonaje;

typedef struct{
	int8_t recursoQueMeBloquea;
} bloqueadoStruct;

typedef struct {
        t_queue *cola;
} __attribute__((__packed__)) planDeNivelesStruct;

//Estructura para avisar bloqueo al planificador
typedef struct {
	header_t header;
	bloqueadoStruct bloqueado;
} __attribute__((__packed__)) mensajeBloqueado;

typedef struct {
	int8_t x;
	int8_t y;
	int16_t simbolo;
} payloadPosicionStruct;

typedef struct{
	header_t header;
	payloadPosicionStruct posicion;
} __attribute__((__packed__)) dibujarMovimientoStruct ;

typedef struct{
	header_t header;
} __attribute__((__packed__)) headerStruct;

typedef struct {
	header_t header;
	recursoStruct recurso;
} __attribute__((__packed__)) recursoObtenidoStruct ;

typedef struct{
	int8_t status;
}statusStruct;

typedef struct{
	header_t header;
	statusStruct movimientoFinal;
} __attribute__((__packed__)) movimientoStruct ;

typedef struct{
	char nombre[25];
	char simbolo;
}__attribute__((__packed__)) nombreYSimboloStruct;

typedef struct{
	header_t header;
	char simbolo;
}__attribute__((__packed__)) envioSimboloStruct;

typedef struct{
	int8_t id;
}__attribute__((__packed__)) identificadorStruct;

//Inicio Variables globales
t_log *Log;
int Vidas;
int Signal;
pthread_mutex_t controlListaPersonajes= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2= PTHREAD_MUTEX_INITIALIZER;
//Fin variables globales

payloadNivelPlanificadorStruct conectarOrquestador(personajePaquetizadoStruct *personajePaquetizado,char orquestador[]);  //Prototipo
int moverse(positionStruct *J, positionStruct R);
void rutinaSignal(int n);
void * serializador_planDeNiveles(planDeNivelesStruct *self,int32_t *size);
char * getIp (char c[]);
int32_t getPort (char c[]);
void suicidarse();
void avisoTerminoPlanOrquestador(char *simbolo,char orquestador[]);

int main(int argc,char *argv[]){

	signal(SIGTERM, rutinaSignal);
	signal(SIGUSR1, rutinaSignal);
	signal(SIGINT, rutinaSignal);
	int socketNivel;
	int socketPlanificador;
	struct timeval tv;
	mensajePersonaje *mensaje;
	mensaje = malloc(sizeof(mensajePersonaje));
	int planDeNivelesTerminado = false;
	positionStruct *posicion;
	posicion = malloc(sizeof(positionStruct));
	positionStruct recursoPosicion;
	dibujarMovimientoStruct *dibujarMovimiento;
	pedirRecursoPaquetizadoStruct *pedirRecurso;
	statusStruct *otorgado;
	statusStruct *sePudoMover;
	int  estoySobreRecurso;
	int hayNivelSiguiente;
	char namelog[20];
	char orquestador[30];
	int flagBloqueado;
	Signal=0;
	int vale =8;
	int recibiAlgo;
	char ipPlataforma[16];
	int solicitePosicion;
	char nombreLog[25];

	t_queue *planDeNiveles = queue_create();
	t_queue *objetivosNivel = queue_create();
	personajeStruct personaje;
	personajePaquetizadoStruct *personajePaquetizado = malloc(sizeof(personajePaquetizadoStruct));
	t_config *personajeConfig = malloc(sizeof(t_config));
	payloadNivelPlanificadorStruct nivelPlanificador;
	nombreYSimboloStruct *datosPlanificador = malloc(sizeof(nombreYSimboloStruct));

	personajeConfig = config_create(argv[1]);
	personajeConfig->nombre  = config_get_string_value(personajeConfig,"nombre");
	personajeConfig->simbolo = config_get_string_value(personajeConfig,"simbolo");
	personajeConfig->planDeNiveles = config_get_array_value(personajeConfig,"planDeNiveles");

	personajeConfig->vidas = config_get_int_value(personajeConfig,"vidas");
	personajeConfig->orquestador = config_get_string_value(personajeConfig,"orquestador");

	snprintf(namelog,sizeof(namelog),"%s%s",personajeConfig->nombre,".log");
	snprintf(nombreLog,sizeof(nombreLog),"%s %c",personajeConfig->nombre,personajeConfig->simbolo[0]);
	Log=log_create(namelog,nombreLog,true,LOG_LEVEL_INFO);

	strcpy(orquestador,personajeConfig->orquestador);
	strcpy(ipPlataforma,getIp(orquestador));

InicioPlan:
	if(Signal==2)
	{
		//Enviar mensaje a nivel actual para que libere mis recursos
		queue_clean(planDeNiveles);
		queue_clean(objetivosNivel);
		shutdown(socketPlanificador,2);
		shutdown(socketNivel,2);
		Signal = 0;
	//	pthread_mutex_unlock(&mutex2);
	}

	Vidas = personajeConfig->vidas;

	//Obtengo la cantidad de niveles de mi plan, y guardo el nombre del nivel en una cola.
	int cantidadDeNivelesEnPlan = 0;
	while (personajeConfig->planDeNiveles[cantidadDeNivelesEnPlan] != NULL) 
	{
		queue_push(planDeNiveles,(void*)personajeConfig->planDeNiveles[cantidadDeNivelesEnPlan]);
		cantidadDeNivelesEnPlan++;
	}

	strcpy(personaje.nombre,personajeConfig->nombre);
	personaje.simbolo=personajeConfig->simbolo[0];
	personaje.vidas = personajeConfig->vidas;
	
	while(planDeNivelesTerminado == false)
	{


		int nivelTerminado = false;
		strcpy(personaje.nivel , queue_peek(planDeNiveles));

		personajePaquetizado->header.id=1; //Soy un personaje
		personajePaquetizado->header.length= sizeof(personajeStruct);
		personajePaquetizado->personaje = personaje;



		nivelPlanificador = conectarOrquestador(personajePaquetizado,orquestador);
		log_info(Log, "Recibi los datos del Nivel/Planificador");
		if(strcmp(nivelPlanificador.ipNivel,"0") == 0 &&  nivelPlanificador.puertoNivel == 0 && nivelPlanificador.puertoPlanificador == 0)
		{
			log_info(Log,"El nivel %s no esta levantado",personajePaquetizado->personaje.nivel);
			suicidarse();
		}
		if(nivelPlanificador.puertoPlanificador == -1)
		{
			log_info(Log,"El Planificador %d no esta levantado",personajePaquetizado->personaje.nivel);
			suicidarse();
		}

		
		log_info(Log,"Nivel IP:%s Puerto:%d\n",nivelPlanificador.ipNivel,nivelPlanificador.puertoNivel);
		log_info(Log,"Planificador IP:%s Puerto:%d\n",ipPlataforma,nivelPlanificador.puertoPlanificador);


		InicioNivel:
				if(Signal == 1)
				{
					log_info(Log,"Estoy reiniciando el nivel");
					queue_clean(objetivosNivel);
					shutdown(socketPlanificador,2);
					shutdown(socketNivel,2);
					Signal = 0;
				}



		socketNivel = socketCreateClient(nivelPlanificador.ipNivel, nivelPlanificador.puertoNivel); //Crea Sockets para el nivel
		if(socketNivel !=  0)
			log_info(Log,"Me conecte correctamente al nivel %s", personaje.nivel);

		identificadorStruct *identificador = malloc(sizeof(identificadorStruct));
		identificador->id=1;
		char *simbolo = &personaje.simbolo;
		pthread_mutex_lock(&controlListaPersonajes);
		socketSend(socketNivel,identificador,sizeof(identificadorStruct));
		socketSend(socketNivel,simbolo,sizeof(char));
		pthread_mutex_unlock(&controlListaPersonajes);
		free(identificador);


		socketPlanificador = socketCreateClient(ipPlataforma, nivelPlanificador.puertoPlanificador); //Crea sockets para el planificador
		if(socketPlanificador !=  0)
			log_info(Log,"Me conecte correctamente al planificador");
		datosPlanificador->simbolo = simbolo[0];
		strcpy(datosPlanificador->nombre,personaje.nombre);
		pthread_mutex_lock(&controlListaPersonajes);
		socketSend(socketPlanificador,datosPlanificador,sizeof(nombreYSimboloStruct));
		pthread_mutex_unlock(&controlListaPersonajes);





		//Obtengo objetivos de mi proximo nivel
		char obj[15]= "obj[";
		strcat(obj,queue_peek(planDeNiveles));
		strcat(obj,"]");
		personajeConfig->objetivos = config_get_array_value(personajeConfig,obj);

		//Pongo los objetivos en una cola
		int cantidadDeObjetivos = 0;
		while (personajeConfig->objetivos[cantidadDeObjetivos] != NULL)
		{
			queue_push(objetivosNivel,(void*)(personajeConfig->objetivos[cantidadDeObjetivos][0]));
			cantidadDeObjetivos++;
		}
		posicion->x=0;
		posicion->y=0;
		log_info(Log,"Posicion Inicial- X:%d Y:%d",posicion->x,posicion->y);

		flagBloqueado = 0;

		//Le mando mi plan de niveles al recurso para que pueda chequear interbloqueo
		{
			int32_t size,length;
			planDeNivelesStruct planNivel;
			void *data;
			planNivel.cola=queue_create();
			queue_copy(planNivel.cola,objetivosNivel);
			data = serializador_planDeNiveles(&planNivel,&size);
			length = size + sizeof(header_t);
			pthread_mutex_lock(&controlListaPersonajes);
			socketSend(socketNivel,data,length);
			pthread_mutex_unlock(&controlListaPersonajes);
			free(data);
		}



		while(nivelTerminado == false)
		{

				solicitePosicion = false;
				estoySobreRecurso = false;

				while(estoySobreRecurso==false)
				{

					pthread_mutex_lock(&controlListaPersonajes);

					if((vale =recv(socketPlanificador, mensaje, sizeof(mensajePersonaje) , MSG_WAITALL)) < 0)
					{
						printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
						log_info(Log,"Vale: %d",vale);
						free(mensaje);
						return -1;
					}
					pthread_mutex_unlock(&controlListaPersonajes);
					if(mensaje->idMensaje == 1)
					{
MovimientoDespuesDeBloqueado:
						if(solicitePosicion == false)
						{
							log_info(Log,"Solicito posicion de %c",queue_peek(objetivosNivel));
							//Como me faltan conseguir recursos, le pido la posicion al nivel
							pedirRecurso = malloc(sizeof(pedirRecursoPaquetizadoStruct));
							pedirRecurso->header.id = 1;
							pedirRecurso->header.length = sizeof(recursoStruct);
							pedirRecurso->recurso.r = (int) queue_peek(objetivosNivel);
							pthread_mutex_lock(&controlListaPersonajes);
							socketSend(socketNivel,pedirRecurso,sizeof(pedirRecursoPaquetizadoStruct));
							pthread_mutex_unlock(&controlListaPersonajes);
							free(pedirRecurso);
							pthread_mutex_lock(&controlListaPersonajes);
							recv(socketNivel,&recursoPosicion,sizeof(positionStruct),MSG_WAITALL); //Recibo la posicion del recurso solicitado
							pthread_mutex_unlock(&controlListaPersonajes);
							log_info(Log,"La posicion de %c es: X:%d Y:%d",queue_peek(objetivosNivel),recursoPosicion.x,recursoPosicion.y);
							solicitePosicion = true;
						}


						estoySobreRecurso= moverse(posicion,recursoPosicion);//Se mueve y corta el while si estoy sobre el recurso
						log_info(Log,"%s Posicion Actual- X:%d Y:%d",queue_peek(planDeNiveles),posicion->x,posicion->y);
						dibujarMovimiento= malloc(sizeof(dibujarMovimientoStruct));
						dibujarMovimiento->header.id=2;
						dibujarMovimiento->header.length = sizeof(payloadPosicionStruct);
						dibujarMovimiento->posicion.simbolo=personaje.simbolo;
						dibujarMovimiento->posicion.x = posicion->x;
						dibujarMovimiento->posicion.y = posicion->y;
						pthread_mutex_lock(&controlListaPersonajes);
						socketSend(socketNivel, dibujarMovimiento,sizeof(dibujarMovimientoStruct));
						pthread_mutex_unlock(&controlListaPersonajes);
						sePudoMover = malloc(sizeof(statusStruct));
						pthread_mutex_lock(&controlListaPersonajes);
						recv(socketNivel,sePudoMover,sizeof(statusStruct),MSG_WAITALL);
						pthread_mutex_unlock(&controlListaPersonajes);
						if(sePudoMover->status == false)
						{
							log_error(Log,"Hubo un error al moverme en el nivel\n");
							shutdown(socketPlanificador,2);
							shutdown(socketNivel,2);
							kill(getpid(),9);
						}
						free(dibujarMovimiento);

						if(estoySobreRecurso == false)
						{
							movimientoStruct *mensajeRespuesta;
							mensajeRespuesta = malloc(sizeof(movimientoStruct));
							mensajeRespuesta->header.id=1;
							mensajeRespuesta->header.length=sizeof(statusStruct); //Para no mandar basura
							mensajeRespuesta->movimientoFinal.status=false;
							pthread_mutex_lock(&controlListaPersonajes);
							socketSend(socketPlanificador, mensajeRespuesta,sizeof(movimientoStruct));	//Le aviso al Planificador que ya me movi
							pthread_mutex_unlock(&controlListaPersonajes);
							free(mensajeRespuesta);
						}

						if(Signal != 0)
						{
							if(Signal ==1)
							{
									goto InicioNivel;
							}
							else
							if(Signal == 2)
								goto InicioPlan;
							else
							if(Signal ==3)
							{
								log_info(Log,"Me quisiste matar %c%c%c%c%c\nEspera que cierro las conexiones y me suicido\n",40,39,126,39,41);
								shutdown(socketPlanificador,2);
								shutdown(socketNivel,2);
								Signal = 0;
								kill(getpid(),9);
								pthread_mutex_unlock(&mutex2);
							}
								log_error(Log,"Valor incorrecto de Signal: %d",Signal);
						}

					}
					else
						if(mensaje->idMensaje == 4)
						{
							if(Vidas >0)
							{
								Signal=1;
								Vidas--;
								log_info(Log,"Me mato el Orquestador. Reinicio el nivel");
								log_info(Log,"Ahora dispongo de %d vidas",Vidas);
							goto InicioNivel;
							}
							else
							if(Vidas == 0)
							{
								Signal=2;
								log_info(Log,"Me mato el Orquestador. Reinicio el plan");
								goto InicioPlan;
							}
						}
				}

					//Estoy sobre el recurso y se lo pido al nivel
					log_info(Log,"Solicito %c", queue_peek(objetivosNivel));
					pedirRecurso=malloc(sizeof(pedirRecursoPaquetizadoStruct));
					pedirRecurso->header.id = 3;
					pedirRecurso->header.length = sizeof(recursoStruct);
					pedirRecurso->recurso.r = (int) queue_peek(objetivosNivel);
					char recursoPedido = pedirRecurso->recurso.r;
					otorgado=malloc(sizeof(statusStruct));
					pthread_mutex_lock(&controlListaPersonajes);
					socketSend(socketNivel,pedirRecurso,sizeof(pedirRecursoPaquetizadoStruct));
					recv(socketNivel,otorgado,sizeof(statusStruct),MSG_WAITALL);
					pthread_mutex_unlock(&controlListaPersonajes);
					free(pedirRecurso);
	RecursoOtorgado:
					if(otorgado->status == true)
					{
						log_info(Log,"Tome una %c",queue_peek(objetivosNivel));
						queue_pop(objetivosNivel);
					}
					else if(otorgado->status == false)
					{
						log_info(Log,"Estoy bloqueado en una %c",queue_peek(objetivosNivel));
						flagBloqueado = 1;
						//Envío mensaje al planificador para avisar que estoy bloqueado
						mensajeBloqueado *bloqueado;
						bloqueado = malloc(sizeof(mensajeBloqueado));
						bloqueado->header.id = 2;
						bloqueado->header.length = sizeof(bloqueadoStruct);
						bloqueado->bloqueado.recursoQueMeBloquea = recursoPedido;
						pthread_mutex_lock(&controlListaPersonajes);
						socketSend(socketPlanificador, bloqueado, sizeof(mensajeBloqueado));
						pthread_mutex_unlock(&controlListaPersonajes);

						tv.tv_sec = 3;  /* 3 Secs Timeout */
						tv.tv_usec = 0;  // Not init'ing this can cause strange errors
						setsockopt(socketPlanificador, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
						recibiAlgo = 0;
						while(recibiAlgo <= 0)
						{
							pthread_mutex_lock(&controlListaPersonajes);
							recibiAlgo = recv(socketPlanificador, mensaje, sizeof(mensajePersonaje) , MSG_WAITALL); //En este recv se queda esperando que el planificador lo vuelva a planificar dado que esta bloqueado
							pthread_mutex_unlock(&controlListaPersonajes);
							if(Signal != 0)
							{
								tv.tv_sec = 0;  /* 0 Secs Timeout */
								tv.tv_usec = 0;  // Not init'ing this can cause strange errors
								setsockopt(socketPlanificador, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
								if(Signal ==1)
									goto InicioNivel;
								else
								if(Signal == 2)
									goto InicioPlan;
								else
								if(Signal ==3)
								{
									log_info(Log,"Me quisiste matar %c%c%c%c%c\nEspera que cierro las conexiones y me suicido\n",40,39,126,39,41);
									shutdown(socketPlanificador,2);
									shutdown(socketNivel,2);
									Signal = 0;
									kill(getpid(),9);
									pthread_mutex_unlock(&mutex2);
								}
									log_error(Log,"Valor incorrecto de Signal: %d",Signal);
							}
						}

						tv.tv_sec = 0;  /* 0 Secs Timeout */
						tv.tv_usec = 0;  // Not init'ing this can cause strange errors

						setsockopt(socketPlanificador, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

						if(mensaje->idMensaje == 1)
						{
							otorgado->status=true;
							goto RecursoOtorgado;

						}
						else
						if(mensaje->idMensaje == 4)
						{
							if(Vidas >0)
							{
								Signal=1;
								Vidas--;
								log_info(Log,"Me mato el Orquestador. Reinicio el nivel");
								log_info(Log,"Ahora dispongo de %d vidas",Vidas);
							goto InicioNivel;
							}
							else
							if(Vidas == 0)
							{
								Signal=2;
								log_info(Log,"Me mato el Orquestador\n Reinicio mi plan de niveles");
								goto InicioPlan;
							}
						}
					}


				nivelTerminado = queue_is_empty(objetivosNivel);

				if(nivelTerminado == true)
				{
					movimientoStruct *mensajeRespuesta;
					mensajeRespuesta = malloc(sizeof(movimientoStruct));
					mensajeRespuesta->header.id=1;
					mensajeRespuesta->header.length=sizeof(statusStruct); //Para no mandar basura
					mensajeRespuesta->movimientoFinal.status=2;
					socketSend(socketPlanificador,mensajeRespuesta,sizeof(movimientoStruct));
					free(mensajeRespuesta);
					log_info(Log,"Nivel %s terminado",queue_peek(planDeNiveles));
					queue_pop(planDeNiveles);
					shutdown(socketPlanificador,2);
					shutdown(socketNivel,2);
					//Notifico al orquestador que termine el nivel
				}
				else //Aviso al planificador que me faltan objetivos, para que me meta en la cola y me planifique.
				{
					if(flagBloqueado == 1 )
					{
							solicitePosicion = false;
							estoySobreRecurso = false;
							flagBloqueado = 0;
							goto MovimientoDespuesDeBloqueado;
					}
					else
					{
						movimientoStruct *mensajeRespuesta;
						mensajeRespuesta = malloc(sizeof(movimientoStruct));
						mensajeRespuesta->header.id=1;
						mensajeRespuesta->header.length=sizeof(statusStruct); //Para no mandar basura
						mensajeRespuesta->movimientoFinal.status=true;
						pthread_mutex_lock(&controlListaPersonajes);
						socketSend(socketPlanificador, mensajeRespuesta,sizeof(movimientoStruct));	//Le aviso al Planificador que ya me movi
						pthread_mutex_unlock(&controlListaPersonajes);
						free(mensajeRespuesta);
					}
				}

		}

		hayNivelSiguiente = !queue_is_empty(planDeNiveles);
		if(hayNivelSiguiente == false)
		{
			planDeNivelesTerminado = true;
			sleep(5);
			avisoTerminoPlanOrquestador(simbolo,orquestador);
			log_info(Log,"Termine mi plan de niveles\nKoopa alli voy!");

		}
	}


	return 0;
}


payloadNivelPlanificadorStruct conectarOrquestador(personajePaquetizadoStruct *personajePaquetizado,char orquestador[])
{
	
	payloadNivelPlanificadorStruct buffer;
	int socket = socketCreateClient(getIp(orquestador),getPort(orquestador));
	if(socket == 1 )
	{
		log_info(Log, "El orquestador no esta levantado ;)");
		suicidarse();
	}
	if(socket !=  0)
		log_info(Log, "Me conecte al socket del orquestador");
	pthread_mutex_lock(&controlListaPersonajes);
	socketSend(socket, personajePaquetizado, sizeof(personajePaquetizadoStruct));	
	recv(socket, &buffer, sizeof(payloadNivelPlanificadorStruct), MSG_WAITALL);
	pthread_mutex_unlock(&controlListaPersonajes);
	shutdown(socket,2);
	return buffer;
}

void avisoTerminoPlanOrquestador(char *simbolo,char orquestador[])
{
	envioSimboloStruct *buffer =malloc(sizeof(envioSimboloStruct));
	buffer->header.id=6;
	buffer->header.length=sizeof(char);
	buffer->simbolo = simbolo[0];
	int socket = socketCreateClient(getIp(orquestador),getPort(orquestador));
	if(socket == 1 )
	{
		log_info(Log, "El orquestador no esta levantado ;)");
		suicidarse();
	}
	if(socket !=  0)
		log_info(Log, "Me conecte al socket del orquestador");

	pthread_mutex_lock(&controlListaPersonajes);
	socketSend(socket, buffer, sizeof(envioSimboloStruct));
	pthread_mutex_unlock(&controlListaPersonajes);
	shutdown(socket,2);
	free(buffer);
}



int moverse(positionStruct *J, positionStruct R) // J = Jugador ; R = Recurso
{
	if ( J->x != R.x)
	{
		if (J->x < R.x)
			J->x = J->x +1;	
		else
			J->x = J->x -1;
		if( J->x == R.x && J->y == R.y)
			return 1;
		else
			return 0;
	}	
	else
	if (J->y != R.y)
	{
		if (J->y < R.y)
			J->y = J->y +1;
		else
			J->y = J->y -1;
		if( J->x == R.x && J->y == R.y)
					return 1;
				else
					return 0;
	}
	return -1; //Error
}


void rutinaSignal(int n) 
{
	switch (n) 
	{
		case SIGTERM:
			log_info(Log,"Recibi la señal SIGTERM");
			if(Vidas >0)
			{
				Vidas--;
				log_info(Log,"Ahora dispongo de %d vidas.",Vidas);
				Signal=1; //Reinicia Nivel
			}
			else
			if(Vidas == 0)
			{
				log_info(Log,"Reinicio mi plan de niveles");
				Signal=2; //Reinicia plan
			}
			break;
		case SIGUSR1:
			log_info(Log,"Recibi la señal SIGUSR1");
			Vidas++;
			log_info(Log,"Ahora dispongo de %d vidas",Vidas);
			break;
		case SIGINT:
			Signal = 3;
			break;
	}
}

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

void suicidarse()
{
	kill(getpid(),9);
}

void * serializador_planDeNiveles(planDeNivelesStruct *self,int32_t *size) {
		*size = queue_size(self->cola) * sizeof(char);
		void *data = malloc(*size + sizeof(header_t));
        int32_t offset = 0, tmp_size = 0;
        char recurso;

        header_t header;
        header.id = 4;
        header.length=*size;

        t_queue *aux=queue_create();
        queue_copy(aux,self->cola);
        memcpy(data, &header, tmp_size = sizeof(header_t));
        while (!queue_is_empty(aux))
        {
            offset += tmp_size;
        	recurso = (char) queue_pop(aux);
            memcpy(data + offset, &recurso, tmp_size = sizeof(char));
        }
        queue_destroy(aux);
        return data;
}

