#ifndef I2C_H
#define I2C_H

#include "driver/i2c_master.h"

/* Creates the I2C master bus (SDA = GPIO21, SCL = GPIO22). */
i2c_master_bus_handle_t i2c_bus_init(void);

#endif
