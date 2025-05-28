#pragma once

#include "config_data.h"

/**
* This function is filled by `prepare_data_folder.py` script with everything
* from `src/config.json` and called on no config in nvs storage or mismatching
* magic string.
*/
void cfg_default_set(config_data_t *cfg);


/**
* Retrieves a magic string, which identify the current configuration set.
* This is the last 8 byte of a md5sum from `config_data.h` file or
* the `src/config.json`, if this file exists.
*/
const char* cfg_default_magic();
