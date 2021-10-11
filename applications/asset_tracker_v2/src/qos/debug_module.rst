.. _asset_tracker_v2_qos:

QoS
###

nRF Cloud, Azure IoT Hub and AWS IoT dependent changes
 - Forwarding of PUBACKS to the application
 - Possiblity of setting message ID in application, if not set the net lib will assign.
 - Internal handling of unacked PINGs.

Modem module needed chanages
 - Prior to every cloud publication a connection evaluation is performed
 - When an evaluation has been performed an event is sent from the modem module containing
   the evaluation.
 - The event is acted upon in the cloud module which sets an substate in the cloud module that is
   handled under the pre-existing CONNECTED state.

How to handle CONEVAL, two options:
 - The cloud module can decide depending on the data type if a message should be sent. I.e
	GENERIC: Contains newly sampled data, should be sent regardless.
	BATCH: Contains older buffered data and can be large, send when energy costs are low.
	UI: Contains button presses, should have high priority.
	NEIGHBOR_CELLS: Should have same priority as the generic data type.
	AGPS_REQUEST: Should be sent at all costs, will lower TTF.
	CONFIG: Not important, the application has applied the config. Should be sent when energy costs are low.

 - Minimum energy level is passed in as metadata in structure qos_data.
   When the callback QOS_EVT_MESSAGE_NEW or QOS_EVT_MESSAGE_ACK_TIMER_EXPIRED is notified in the
   cloud module the cloud module checks if the current energy level is sufficient. If not,
   the message is sendt to the qos library via qos_message_add.
