# AWS IoT provisioning

PoC sample that connects to AWS IoT, requests credentials, and performs run-time provisioning of the nRF9160 modem.
After provisioning, the sample connects to AWS IoT using the new credentials.

This sample depends on https://github.com/coderbyheart/aws-iot-device-credentials-workaround

## Prerequisites

1. Load credentials into the security tag set by `CONFIG_AWS_IOT_SEC_TAG`.
2. Configure the client ID by setting `CONFIG_AWS_IOT_CLIENT_ID_STATIC`.
3. Configure the broker hostname by setting `CONFIG_AWS_IOT_BROKER_HOST_NAME`.

These configurations can be set in `samples/nrf9160/aws_iot_provisioning/prj.conf`.

## Operation

The sample performs the following operational chain:

1. Connects to LTE
2. Connects to AWS IoT using the provisioning credentials loaded in the sec tag set by `CONFIG_AWS_IOT_SEC_TAG`
3. Subscribes to `certificate/${deviceId}/create/accepted/+` (+ = Wild card token)
4. Requests new credentials by sending an empty message to `certificate/${deviceId}/create`
5. Receives new private key on topic `certificate/${deviceId}/create/accepted/key`
6. Receives new client certificate on topic `certificate/${deviceId}/create/accepted/cert`
7. Disconnects from AWS IoT
8. Puts modem into offline mode.
9. Writes private key, client certificate, and CA to security tag 50. (CA is needed and is hardcoded in the sample)
10. Clears subscriptions
11. Puts modem into normal mode (connects to LTE)
12. Connect to AWS IoT with the new credentials referenced to by security tag 50.

## Native TLS (MbedTLS) on the application core

Native TLS can be enabled by including `overlay-native_tls.conf` in the west build command.
Example: `west build -b thingy91_nrf9160_ns -- -DOVERLAY_CONFIG=overlay-native_tls.conf`

Note that in order to use Native TLS, the provisioning credentials needs to be loaded into the MbedTLS stack at run time.
This happens automatically when including the overlay file. The credentials are loaded into the security tag set by `CONFIG_AWS_IOT_SEC_TAG`.
The AWS IoT library will take the credentials located under `samples/nrf9160/aws_iot_provisioning/certs` (`ca-cert.pem`, `client-cert.pem`, and `private-key.pem`), and
provision them to the security tag set by `CONFIG_AWS_IOT_SEC_TAG`.

During testing the device was able to receive a 4.5Kb test payload.

> **Warning**
> Handling of credentials received in one big message on topic `$aws/certificates/create/payload-format/accepted` has not been implemented.
> Meaning that the device will be able to receive large payloads but will not perform provisioning and reconnection to AWS IoT.
> Only handling for the workaround that does not depend on MbedTLS has.
