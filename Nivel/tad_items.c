#include "tad_items.h"
#include "stdlib.h"

void CrearItem(ITEM_NIVEL** ListaItems, char id, int x , int y, char tipo, int cant_rec) {
        ITEM_NIVEL * temp;
        temp = malloc(sizeof(ITEM_NIVEL));

        temp->id = id;
        temp->posx=x;
        temp->posy=y;
        temp->item_type = tipo;
        temp->quantity = cant_rec;
        temp->next = *ListaItems;
        *ListaItems = temp;
}



void CrearPersonaje(ITEM_NIVEL** ListaItems, char id, int x , int y) {
        CrearItem(ListaItems, id, x, y, PERSONAJE_ITEM_TYPE, 0);
}

void CrearCaja(ITEM_NIVEL** ListaItems, char id, int x , int y, int cant) {
        CrearItem(ListaItems, id, x, y, RECURSO_ITEM_TYPE, cant);
}

void BorrarItem(ITEM_NIVEL** ListaItems, char id) {
        ITEM_NIVEL * temp = *ListaItems;
        ITEM_NIVEL * oldtemp;

        if ((temp != NULL) && (temp->id == id)) {
                *ListaItems = (*ListaItems)->next;
		free(temp);
        } else {
                while((temp != NULL) && (temp->id != id)) {
                        oldtemp = temp;
                        temp = temp->next;
                }
                if ((temp != NULL) && (temp->id == id)) {
                        oldtemp->next = temp->next;
			free(temp);
                }
        }

}

int MoverPersonaje(ITEM_NIVEL* ListaItems, char id, int x, int y,int rows,int cols) {
		//valido si esta dentro del nivel
		if ((x < 0) || (y < 0) || (x > cols) || (y > rows))
			return 0;
        ITEM_NIVEL * temp;
        temp = ListaItems;

        while ((temp != NULL) && (temp->id != id)) {
                temp = temp->next;
        }
        if ((temp != NULL) && (temp->id == id)) {
                temp->posx = x;
                temp->posy = y;
        }
  return 1;
}


void restarRecurso(ITEM_NIVEL* ListaItems, char id) {

        ITEM_NIVEL * temp;
        temp = ListaItems;

        while ((temp != NULL) && (temp->id != id)) {
                temp = temp->next;
        }
        if ((temp != NULL) && (temp->id == id)) {
                if ((temp->item_type) && (temp->quantity > 0)) {
                        temp->quantity--;
                }
        }

}

void sumarRecurso(ITEM_NIVEL* ListaItems, char id) {

        ITEM_NIVEL * temp;
        temp = ListaItems;

        while ((temp != NULL) && (temp->id != id)) {
                temp = temp->next;
        }
        if ((temp != NULL) && (temp->id == id)) {
                if ((temp->item_type) && (temp->quantity >= 0)) {
                        temp->quantity++;
                }
        }

}

