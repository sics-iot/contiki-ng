RUN_FILE = 1

EXAMPLESDIR = ../../


EXAMPLES = \
tests/07-simulation-base/code-data-structures/native:./01-test-data-structures.sh \
examples/mqtt-client/native:./02-mqtt-client-31.sh:DEFINES=MQTT_CLIENT_CONF_ORG_ID=\\\"travis-test\\\",MQTT_CLIENT_CONF_LOG_LEVEL=LOG_LEVEL_DBG,MQTT_CONF_VERSION=MQTT_PROTOCOL_VERSION_3_1 \
examples/mqtt-client/native:./03-mqtt-client-31-valgrind.sh:DEFINES=MQTT_CLIENT_CONF_ORG_ID=\\\"travis-test\\\",MQTT_CLIENT_CONF_LOG_LEVEL=LOG_LEVEL_DBG,MQTT_CONF_VERSION=MQTT_PROTOCOL_VERSION_3_1 \
examples/mqtt-client/native:./04-mqtt-client-311.sh:DEFINES=MQTT_CLIENT_CONF_ORG_ID=\\\"travis-test\\\",MQTT_CLIENT_CONF_LOG_LEVEL=LOG_LEVEL_DBG,MQTT_CONF_VERSION=MQTT_PROTOCOL_VERSION_3_1_1 \
examples/mqtt-client/native:./05-mqtt-client-311-valgrind.sh:DEFINES=MQTT_CLIENT_CONF_ORG_ID=\\\"travis-test\\\",MQTT_CLIENT_CONF_LOG_LEVEL=LOG_LEVEL_DBG,MQTT_CONF_VERSION=MQTT_PROTOCOL_VERSION_3_1_1 \
examples/mqtt-client/native:./06-mqtt-client-5.sh:DEFINES=MQTT_CLIENT_CONF_ORG_ID=\\\"travis-test\\\",MQTT_CLIENT_CONF_LOG_LEVEL=LOG_LEVEL_DBG,MQTT_CONF_VERSION=MQTT_PROTOCOL_VERSION_5 \
examples/mqtt-client/native:./07-mqtt-client-5-valgrind.sh:DEFINES=MQTT_CLIENT_CONF_ORG_ID=\\\"travis-test\\\",MQTT_CLIENT_CONF_LOG_LEVEL=LOG_LEVEL_DBG,MQTT_CONF_VERSION=MQTT_PROTOCOL_VERSION_5 \
examples/hello-world/native:./08-native-ping.sh \
examples/coap/coap-example-server/native:./09-native-coap.sh \
examples/snmp-server/native:./10-snmp-server.sh \
tests/08-native-runs/11-aes-ccm/native:./11-aes-ccm.sh \
tests/08-native-runs/12-heapmem/native:./12-heapmem.sh:DEFINES=HEAPMEM_DEBUG=0 \
tests/08-native-runs/12-heapmem/native:./12-heapmem.sh:DEFINES=HEAPMEM_DEBUG=1 \
tests/08-native-runs/13-coffee/native:./13-coffee.sh \
tests/08-native-runs/14-sha-256/native:./14-sha-256.sh \
tests/08-native-runs/15-ieee802154-security/native:./15-ieee802154-security.sh \
tests/08-native-runs/16-process-mutex/native:./16-process-mutex.sh \
tests/08-native-runs/17-ecc/native:./17-ecc.sh

include ../Makefile.compile-test
