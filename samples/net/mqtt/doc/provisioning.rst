.. _mqtt_sample_provisioning:

Provisioning
############

.. note::
  This section is only valid if building the sample with TLS enabled.

The server CA for the default MQTT broker **test.mosquitto.org** is provisioned to the network stack at run-time prior to establishing a connection to the server.
The server CA is located in :file:`src/modules/transport/certs/ca-cert.pem`.
Note that if the server is changed the CA needs to be updated as well.

To retrieve the server CA for a different MQTT broker the following command can be used:

.. code-block:: console

  openssl s_client -connect <hostname>:<port> -showcerts < /dev/null

Any of the certificates in the returned certificate chain can be used.

.. important::
  Provisioning of credentials at run-time is only meant for testing purposes and should be avoided in a production scenario.
  Especially if using client authenticated TLS, where the private key will be exposed in flash.

To turn of run-time credential provisoning, disable the option :kconfig:option:`CONFIG_MQTT_HELPER_PROVISION_CERTIFICATES` for Native Posix and nRF7002 DK builds or :kconfig:option:`CONFIG_MODEM_KEY_MGMT` for nRF9160 builds.
The CA is provisioned to the security tag set by the :kconfig:option:`CONFIG_MQTT_SEC_TAG` option.

By default, the established TLS connection to **test.mosquitto.org** does not require client authentication which removes the need to provision client certificate and private key to the network stack.
If the client certificate and private key has been generated for a server connection, the credentials need to be provisoned the same way as the server CA.
This occurs automatically when populating the respective files located in :file:`src/modules/transport/certs/`.

.. include:: /includes/cert-flashing.txt
