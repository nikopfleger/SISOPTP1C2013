/*
 * Copyright (C) 2012 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

	#include "so-commons-library/collections/dictionary.h"

typedef struct {
		int32_t quantum;
		double retardo;
		char* path;
		t_dictionary *properties;
	} t_configQuantum;

	t_configQuantum *config_create(char *path);
	bool 	  config_has_property(t_configQuantum*, char* key);
	char 	 *config_get_string_value(t_configQuantum*, char *key);
	int 	  config_get_int_value(t_configQuantum*, char *key);
	long	  config_get_long_value(t_configQuantum*, char *key);
	double 	  config_get_double_value(t_configQuantum*, char *key);
	char**    config_get_array_value(t_configQuantum*, char* key);
	int 	  config_keys_amount(t_configQuantum*);
	void 	  config_destroy(t_configQuantum *config);

#endif /* CONFIG_H_ */
