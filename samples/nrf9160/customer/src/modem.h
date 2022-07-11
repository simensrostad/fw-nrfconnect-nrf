/**
 * @file        modem.h
 *
 * @details     A module handling the modem.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        14-09-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#ifndef MODEM_H_
#define MODEM_H_

/**
 * @brief   Initialize the modem.
 */
void modem_init(void);

/**
 * @brief   Connect the modem to the LTE network.
 */
void modem_connect(void);

/**
 * @brief   Disconnect the modem from the LTE network.
 */
void modem_disconnect(void);

/**
 * @brief   Set the client id.
 */
void modem_set_client_id(void);

/**
 * @brief   Get the client ID.
 *
 * @return  The client ID.
 */
const uint8_t* modem_get_client_id(void);

#endif /* MODEM_H_ */