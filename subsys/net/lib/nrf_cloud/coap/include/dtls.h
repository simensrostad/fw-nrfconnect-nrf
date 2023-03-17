/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DTLS_H_
#define DTLS_H_

int dtls_init(int sock);
bool dtls_cid_is_available(void);
bool dtls_cid_is_active(int sock);
int dtls_session_save(int sock);
int dtls_session_load(int sock);

#endif /* DTLS_H_ */
