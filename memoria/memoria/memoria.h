#ifndef LIBMEMORIA_H_
#define LIBMEMORIA_H_
    #include "so-commons-library/collections/list.h"
	#include	<stdbool.h>
    typedef char* t_memoria;


    typedef struct {
        char id;
        int inicio;
        int tamanio;
        char* dato;
        bool libre;
    } t_particion;

    t_memoria crear_memoria(int tamanio);
    int almacenar_particion(t_memoria segmento, char id, int tamanio, t_memoria dato);
    int eliminar_particion(t_memoria segmento, char id);
    void liberar_memoria(t_memoria segmento);
    t_list* particiones(t_memoria segmento);
#endif /* LIBMEMORIA_H_ */
