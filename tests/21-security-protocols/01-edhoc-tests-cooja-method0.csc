<?xml version="1.0" encoding="UTF-8"?>
<simconf version="2023090101">
  <simulation>
    <title>EDHOC simulation (METHOD 0)</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>50.0</transmitting_range>
      <interference_range>100.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.contikimote.ContikiMoteType
      <description>Client</description>
      <source>[CONTIKI_DIR]/examples/edhoc-tests/edhoc-client/edhoc-test-client.c</source>
      <commands>$(MAKE) TARGET=cooja clean
      $(MAKE) -j$(CPUS) DEBUG=1 MAKE_EDHOC_METHOD=0 edhoc-test-client.cooja TARGET=cooja</commands>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Battery</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiVib</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRS232</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiBeeper</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRadio</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiButton</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiPIR</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiClock</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiLED</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiCFS</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiEEPROM</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <mote>
        <interface_config>
          org.contikios.cooja.interfaces.Position
          <pos x="-41.29379881633341" y="84.55841010196319" />
        </interface_config>
        <interface_config>
          org.contikios.cooja.contikimote.interfaces.ContikiMoteID
          <id>1</id>
        </interface_config>
      </mote>
    </motetype>
    <motetype>
      org.contikios.cooja.contikimote.ContikiMoteType
      <description>Server</description>
      <source>[CONTIKI_DIR]/examples/edhoc-tests/edhoc-server/edhoc-test-server.c</source>
      <commands>$(MAKE) TARGET=cooja clean
      $(MAKE) -j$(CPUS) DEBUG=1 MAKE_EDHOC_METHOD=0 edhoc-test-server.cooja TARGET=cooja</commands>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Battery</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiVib</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRS232</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiBeeper</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRadio</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiButton</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiPIR</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiClock</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiLED</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiCFS</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiEEPROM</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <mote>
        <interface_config>
          org.contikios.cooja.interfaces.Position
          <pos x="-14.358887920221928" y="84.18613975939073" />
        </interface_config>
        <interface_config>
          org.contikios.cooja.contikimote.interfaces.ContikiMoteID
          <id>2</id>
        </interface_config>
      </mote>
    </motetype>
  </simulation>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.TrafficVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <viewport>1.696847649207872 0.0 0.0 1.696847649207872 238.2029835403895 36.46408356383216</viewport>
    </plugin_config>
    <bounds x="1" y="1" height="400" width="400" z="2" />
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <bounds x="403" y="0" height="478" width="681" z="1" />
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>TIMEOUT(800000000);
sim.setSpeedLimit(100000.0);  // Simulation speed.

var checkingEnabled = true;
var clientIsDone = false;
let clientMasterSecret = "client";
let serverMasterSecret = "server";
var hasError = 0;

while (true) {
  log.log(id + " " + msg + "\n");  // Write all output to COOJA.testlog

  // Define the device type
  var device = (id == 1) ? "Client" : (id == 2) ? "Server" : "Unknown";
  device = "[MSG : EDHOC     ] " + device;

  // Check for fail condition
  if(!msg.contains("edhoc_deserialize_err")) {
    if (msg.contains("ERR ") || msg.contains("ERR_")) {
      hasError++;
      //Alt, fail now: log.testFailed();
    }
  }

  // Check for finish condition
  if (msg.contains("Client time to finish")) {
    if (hasError || !(serverMasterSecret === clientMasterSecret)) {
       log.testFailed();
    } else {
       log.testOK();
    }
  }

  // check if the client is done
  if (msg.contains("CL_FINISHED")) {
    clientIsDone = true;
  }  

  // store the OSCORE master secret
  if (msg.contains("OSCORE Master Secret")) {
    if(clientIsDone) {
    	clientMasterSecret = msg;
    } else {
	serverMasterSecret = msg;
    }
  }

  YIELD();
}</script>
      <active>true</active>
    </plugin_config>
    <bounds x="1081" y="0" height="700" width="600" />
  </plugin>
</simconf>
