//Contiene el codigo de la biblioteca memoria.

#include	"memoria.h"
#include 	"so-commons-library/log.h"
#include	<stdlib.h>
#include	<string.h>


t_list* list_particiones;
int tamanioSegmento;

t_memoria crear_memoria(int tamanio){
	list_particiones = list_create();
	t_memoria segmento;
	t_log* log_e;
	t_log* log_i;
	int comp;
	comp= 10;


	tamanioSegmento=tamanio;
	log_e = log_create("koopa.log","koopa",true,LOG_LEVEL_ERROR);
	log_i = log_create("koopa.log", "koopa", true, LOG_LEVEL_INFO);

	if ((segmento=malloc(tamanio))== NULL)
		log_error(log_e, "Se produjo un error al crear el segmento de %d", tamanio);
	else
	{
		t_particion* nuevaParticion = malloc(sizeof(t_particion));
		nuevaParticion->libre= true;
		nuevaParticion->tamanio= tamanio;
		nuevaParticion->inicio=0;
		nuevaParticion->dato= segmento;
		list_add(list_particiones, nuevaParticion);
		log_info(log_i, "Se creo el segmento de tamanio %d", tamanio);
		return segmento;

	}
	return segmento;
}

int almacenar_particion(t_memoria segmento, char id, int tamanio, t_memoria dato){

t_log* log_e;
t_log* log_i;
log_e = log_create("koopa.log","koopa",true,LOG_LEVEL_ERROR);
log_i= log_create("koopa.log","koopa",true, LOG_LEVEL_INFO);
t_particion* nuevaParticion = malloc(sizeof(t_particion));

bool condicion2(void* particion2){
	return (((t_particion*) particion2)->id == id);
	}
t_particion* particion2 = list_find(list_particiones, condicion2);

if (particion2 != NULL){
	log_error(log_e,"Id duplicado");
	return -1;
}

if (tamanio> tamanioSegmento){
	log_error(log_e,"Tamaño grande");
	return -1;
}

bool condicion(void* particion) {
	return (((t_particion*) particion)->libre &&
			((((t_particion*) particion)->tamanio >= tamanio)));
		    }
t_particion* particion = list_find(list_particiones, condicion);

if(particion==NULL){
log_info(log_i,"No se encontro una particion lo suficientemente grande");
return 0;
}
	else if(particion->tamanio>tamanio){
	//aca va el caso en que la particion existente la tengo que dividir en 2
	//con una particion auxiliar
	particion->id= id;
	particion->libre= false;
	nuevaParticion->tamanio=particion->tamanio-tamanio;
	particion->tamanio= tamanio;
	memcpy(segmento+particion->inicio, dato,tamanio);
	nuevaParticion->dato = particion->dato + tamanio;
	nuevaParticion->libre = true;
	nuevaParticion->inicio = particion->inicio + tamanio;

	list_add(list_particiones,nuevaParticion);
	log_info(log_i,"Particion alamacenada");
}
else
{
	particion->id=id;
	particion->tamanio=tamanio;
	memcpy(	segmento+(particion->inicio), dato,tamanio);
	//particion->inicio = particion->inicio + tamanio;
	particion->libre= false;
	//si la particion es de igual tamaño no pasa nada si es de mayor tamaño dividirla en 2
	//list_add(list_particiones,particion);
	//este es el caso de que las particiones
	//tienen exactamente el mismo tamaño
	log_info(log_i,"Particion alamacenada");

}
return 1;
}

int eliminar_particion(t_memoria segmento, char id){

t_log* log_e;
t_log* log_i;

log_e = log_create("koopa.log","koopa",true,LOG_LEVEL_ERROR);
log_i= log_create("koopa.log","koopa",true, LOG_LEVEL_INFO);
//t_particion particion;
bool condicion(void* particion) {
	return ((t_particion*) particion)->id == id;
		    }
t_particion* particion = list_find(list_particiones, condicion);

	if (particion == NULL){
		log_error(log_e,"No se encontro el id para eliminar la particion");
		return 0;
	}
		particion->libre=true;
		particion->id= NULL;


		 log_info(log_i,"Se elimino la particion de id: %c", id);
		 return 1;
}

void liberar_memoria(t_memoria segmento){
	free(segmento);
}

t_list* particiones(t_memoria segmento)
{
	int i;
	t_list *listaParcial = list_create();
	bool funcion_comparadora(void* part1,void* part2){
			return (((t_particion*)part1)->inicio < ((t_particion*)part2)->inicio);
	}
	for (i = 0; i < list_size(list_particiones); i++)
	{
		list_add(listaParcial, list_get(list_particiones, i));
	}
	list_sort (listaParcial,funcion_comparadora);

	return listaParcial;
}



