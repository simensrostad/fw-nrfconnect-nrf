.. _mqtt_sample_description:

Description
###########

The MQTT sample communicates with a MQTT broker either over LTE using the nRF9160 DK / Thingy:91, or Wi-Fi using the nRF7002 DK.

Requirements
************

The sample supports the following development kit:

.. table-from-sample-yaml::

Additionally the sample supports emulation using :ref:`Native Posix <zephyr:native_posix>`.

Overview
*********

The sample connects to either LTE or Wi-Fi depending on the board that the sample is compiled for.
Subsequentually the sample connects to a configured MQTT server (default is **test.mosquitto.org**), where it publishes messages to the topic **my/publish/topic**.
Message publication can also be triggered by pressing any of the buttons on the board.

The sample also subscribes to the topic **my/subscribe/topic**, and will receive any message published to that topic.
Transport Layer Security (TLS) is supported and can be enabled through overlay configuration files included in the sample.

.. note::
   When enabling TLS and building for nRF9160 based boards, the size of the incoming message cannot exceed 2Kb.
   This is due a limitation in the modem's internal TLS buffers.

Configuration
*************

|config|

General
=======

Configurations that are specific to the sample:

.. _CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS:

CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS - Trigger timeout
	This configuration option sets how often the sample publishes a message to the MQTT broker.

.. _CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS:

CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS - Transport reconnection timeout
	This configuration option sets how often the sample will try to reconnect to the MQTT broker upon a lost connection.

.. _CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME:

CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME - MQTT broker hostname
	This configuration sets the MQTT broker hostname.
	Default is "test.mosquitto.org"

.. _CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID:

CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID - MQTT client ID
	This configuration sets the MQTT client ID name.
	Default is IMEI for nRF9160 boards and random for other boards.

.. _CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC:

CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC - MQTT publish topic
	This configuration option sets the topic that the sample will publish messages to.
	Default is "my/publish/topic"

.. _CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC:

CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC - MQTT subscribe topic
	This configuration option sets the topic that the sample will subscribe to.
	Default is "my/subscribe/topic"

Configurations that are specific to the MQTT helper library:

 * :kconfig:option:`CONFIG_MQTT_HELPER_PORT` - MQTT broker port
 * :kconfig:option:`CONFIG_MQTT_SEC_TAG` - MQTT connection TLS security tag.

Wi-Fi
=====

Configurations that are specific for Wi-Fi builds:

.. _CONFIG_MQTT_SAMPLE_NETWORK_WIFI_SSID:

CONFIG_MQTT_SAMPLE_NETWORK_WIFI_SSID - Service Set Identifier (SSID) of Wi-Fi router / access point.
	This configuration option sets the SSID of the Wi-Fi router / access point that the device connects to.

.. _CONFIG_MQTT_SAMPLE_NETWORK_WIFI_PSK:

CONFIG_MQTT_SAMPLE_NETWORK_WIFI_PSK - Pre-Shared Key (PSK) of Wi-Fi router / access point.
	This configuration option sets the PSK of the Wi-Fi router / access point that the device connects to.

.. _CONFIG_MQTT_SAMPLE_NETWORK_WIFI_CONNECTION_REQUEST_TIMEOUT_SECONDS:

CONFIG_MQTT_SAMPLE_NETWORK_WIFI_CONNECTION_REQUEST_TIMEOUT_SECONDS - Wi-Fi connection request timeout.
	Number of seconds that the device will try to connect to the configured SSID.

Files
=====

The sample provides predefined configuration files for the following development kits:

* :file:`prj.conf` - General project configuration file.
* :file:`boards/nrf9160dk_nrf9160_ns.conf` - Configurations for the nRF9160 DK.
* :file:`boards/thingy91_nrf9160_ns.conf` - Configurations for the Thingy:91.
* :file:`boards/nrf7002dk_nrf5340_cpuapp.conf` - Configurations for the nRF7002 DK.
* :file:`boards/native_posix.conf` - Configurations for Native posix.

Files that are located under the **/boards** folder is automatically merged with the :file:`prj.conf` file when you build for corresponding target.

In addition, the sample provides overlay configuration files, which are used to enable additional features in the sample:

* :file:`overlay-tls-nrf9160.conf` - TLS overlay configuration file for nRF9160 DK and Thingy:91.
* :file:`overlay-tls-nrf7002.conf` - TLS overlay configuration file for nRF7002 DK.
* :file:`overlay-tls-native_posix.conf` - TLS overlay configuration file for Native Posix.

They are located in :file:`samples/net/mqtt` folder.

To add a specific overlay configuration file to the build, add the ``-- -DOVERLAY_CONFIG=<overlay_config_file>`` flag to your build.

See :ref:`cmake_options` for instructions on how to add this option to your build.
For example, when building with the command line, the following commands can be used for the nRF9160 DK:

  .. code-block:: console

     west build -b nrf9160dk_nrf9160_ns -- -DOVERLAY_CONFIG=overlay-tls-nrf9160.conf

Building and running
********************

.. |sample path| replace:: :file:`samples/net/mqtt`

.. include:: /includes/build_and_run_ns.txt

Testing
=======

|test_sample|

1. |connect_kit|
#. |connect_terminal|
#. Reset your board.
#. Observe that the board connects to the network and the configured MQTT broker :ref:`CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME <CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME>`.
   When a network connection has been established, LED 1 (green) on the board lights up.
   After the connection has been established the board will start to publish messages to the topic set by :ref:`CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC <CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC>`.
   The frequency of the messages that are published to the broker can be set by :ref:`CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS <CONFIG_MQTT_SAMPLE_TRIGGER_TIMEOUT_SECONDS>` or triggered asynchronously by pressing any of the buttons on the board.
   At any time, the sample can receive messages published to the subscribe topic set by :ref:`CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC <CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC>`.
#. Use an MQTT client like `Mosquitto`_ or `VSMQTT`_ to subscribe to and publish data to the broker.

Sample output
=============

The following serial UART output is displayed in the terminal emulator:

.. code-block:: console

      *** Booting Zephyr OS build v2.4.0-ncs1-rc1-6-g45f2d5cf8ea4  ***
      [00:00:01.448,394] <inf> network: Scanning for Wi-Fi networks...
      [00:00:05.784,637] <inf> network: Num  | SSID                             (len) | Chan (Band)   | RSSI | Security        | BSSID
      [00:00:05.796,539] <inf> network: 1    | NORDIC-GUEST                     12    | 6    (2.4GHz) | -38  | WPA2-PSK        | EC:BD:1D:8C:0D:D1
      [00:00:05.809,692] <inf> network: 2    |                                  1     | 6    (2.4GHz) | -36  | UNKNOWN         | EC:BD:1D:8C:0D:D0
      [00:00:05.822,692] <inf> network: 3    | NORDIC-GUEST                     12    | 11   (2.4GHz) | -56  | WPA2-PSK        | 10:05:CA:64:11:51
      [00:00:05.835,662] <inf> network: 4    | NORDIC-GUEST                     12    | 36   (5GHz  ) | -72  | WPA2-PSK        | 7C:21:0E:69:B5:0F
      [00:00:05.848,663] <inf> network: 5    | NORDIC-INTERNAL                  15    | 36   (5GHz  ) | -72  | UNKNOWN         | 7C:21:0E:69:B5:0E
      [00:00:05.861,694] <inf> network: 6    | NORDIC-GUEST                     12    | 36   (5GHz  ) | -62  | WPA2-PSK        | 10:05:CA:64:11:5E
      [00:00:05.874,694] <inf> network: 7    | ClickShare-Oslo-4-1              19    | 36   (5GHz  ) | -81  | WPA2-PSK        | 48:A9:D2:A6:74:83
      [00:00:05.887,664] <inf> network: 8    |                                  1     | 36   (5GHz  ) | -70  | UNKNOWN         | EC:BD:1D:B0:0B:0F
      [00:00:05.900,726] <inf> network: 9    |                                  1     | 36   (5GHz  ) | -62  | UNKNOWN         | 10:05:CA:64:11:5F
      [00:00:05.913,726] <inf> network: 10   | NORDIC-GUEST                     12    | 36   (5GHz  ) | -69  | WPA2-PSK        | EC:BD:1D:B0:0B:0E
      [00:00:05.926,727] <inf> network: 11   | NORDIC-GUEST                     12    | 52   (5GHz  ) | -36  | WPA2-PSK        | EC:BD:1D:8C:0D:DE
      [00:00:05.939,727] <inf> network: 12   | NORDIC-GUEST                     12    | 52   (5GHz  ) | -83  | WPA2-PSK        | 10:05:CA:64:10:8E
      [00:00:05.952,697] <inf> network: 13   |                                  1     | 52   (5GHz  ) | -37  | UNKNOWN         | EC:BD:1D:8C:0D:DF
      [00:00:05.965,698] <inf> network: 14   |                                  1     | 52   (5GHz  ) | -85  | UNKNOWN         | 10:05:CA:64:10:8F
      [00:00:05.978,698] <inf> network: 15   | NORDIC-INTERNAL                  15    | 100  (5GHz  ) | -86  | UNKNOWN         | 6C:31:0E:2B:99:2E
      [00:00:05.991,638] <inf> network: Scan finished
      [00:00:05.996,520] <inf> network: Connecting to SSID: NORDIC-GUEST
      [00:00:12.477,783] <inf> network: Wi-Fi Connected
      [00:00:17.997,253] <inf> transport: Connected to MQTT broker
      [00:00:18.007,049] <inf> transport: Hostname: test.mosquitto.org
      [00:00:18.009,981] <inf> transport: Client ID: F4CE36000350
      [00:00:18.013,519] <inf> transport: Port: 8883
      [00:00:18.018,341] <inf> transport: TLS: Yes
      [00:00:18.078,521] <inf> transport: Subscribed to topic my/subscribe/topic
      [00:01:01.475,982] <inf> transport: Publishing message: "Hello MQTT! Current uptime is: 61458" on topic: "my/publish/topic"
      [00:02:01.475,982] <inf> transport: Publishing message: "Hello MQTT! Current uptime is: 121458" on topic: "my/publish/topic"
      [00:03:01.475,982] <inf> transport: Publishing message: "Hello MQTT! Current uptime is: 181458" on topic: "my/publish/topic"
      [00:04:01.475,982] <inf> transport: Publishing message: "Hello MQTT! Current uptime is: 241458" on topic: "my/publish/topic"
      [00:05:01.475,982] <inf> transport: Publishing message: "Hello MQTT! Current uptime is: 301459" on topic: "my/publish/topic"

Reconnection logic
------------------

If the network connection is lost the following log output is displayed:

  .. code-block:: console

     <inf> network: Disconnected

If this occurs, the network stack will automatically reconnect to the network using its built-in backoff functionality.

If the TCP/IP (MQTT) connection is lost the following log output is displayed:

  .. code-block:: console

     <inf> transport: Disconnected from MQTT broker

If this occurs, the sample's transport module has built in reconnection logic that will try to reconnect in the frequency set by
:ref:`CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS <CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS>`.

Emulation
=========

The sample can be run in :ref:`Native Posix <zephyr:native_posix>` to simplify development and testing by removing the need for hardware.
Before you can build and run for Native Posix you need to perform the steps included in this link: :ref:`networking_with_native_posix`.

When the aforementioned steps has been completed, the sample can be build and run by calling these commands:

.. code-block:: console

   west build -b native_posix samples/net/mqtt
   west build -t run

Troubleshooting
===============

* If you are having issues with connectivity on nRF9160 based boards, following this link `Trace Collector`_ to learn how to capture modem traces in order to debug network traffic in wireshark.
* Public MQTT brokers might be unstable.
  If you experience problems connecting to the MQTT broker, try switching to another broker by changing the value of the :ref:`CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME <CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME>` configuration option.
  If switching to another broker, remember to update the CA certificate. More on certificates and provisioning in :ref:`mqtt_sample_provisioning`.

Dependencies
************

General:

* :ref:`mqtt_socket_interface`
* :ref:`zbus`
* :ref:`smf`

nRF9160 DK and Thingy:91 builds:

* :ref:`lte_lc_readme`
* :ref:`nrfxlib:nrf_modem`
* :ref:`Trusted Firmware-M <ug_tfm>`

nRF7002 DK builds:

* :ref:`nrfxlib:nrf_security`
* :ref:`net_if_interface`
