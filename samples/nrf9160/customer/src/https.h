/**
 * @file        https.h
 *
 * @details     A module handling the HTTPS Client protocol.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        04-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#ifndef HTTPS_H_
#define HTTPS_H_

#define CC_DEBUG_HTTPS_ENABLED                              0                       //!< Enable(1)/Disable(0) the debug HTTPS functionality (used for testing the internet connection).

#if CC_DEBUG_HTTPS_ENABLED == 1

#define CC_HTTPS_PORT                                       443                     //!< The HTTPS port.
#define CC_HTTPS_IP_ADDRESS                                 "google.com"            //!< The IP address / DNS name for the test site.

#define CC_HTTPS_RECV_BUF_SIZE                              4096                    //!< The total number of bytes that can be received in the rx buffer.  
#define CC_HTTPS_RECV_RESP_BUF_SIZE                         48                      //!< The total number of bytes that can be received in the response buffer. 

#define CC_HTTPS_RESPONSE_OK                                "HTTP/1.1 200 OK"       //!< The text that indicates an OK response.
#define CC_HTTPS_HEAD_GOOGLE                                "HEAD / HTTP/1.1\r\n" "Host: www.google.com:443\r\n" "Connection: close\r\n\r\n"    //!< The HTTPS "HEAD" request on the specific IP address.

/**
 * @brief   Handle the HTTPS request and response by opening, sending a request, receiving a response and closing.
 *
 * @param   _request    The request to send.
 *
 * @return  The received response.
 */
const char* https_open_send_recv_close(const char* const _request);

#endif

#endif /* HTTPS_H_ */