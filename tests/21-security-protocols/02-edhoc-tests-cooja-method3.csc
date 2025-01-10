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
      $(MAKE) -j$(CPUS) DEBUG=1 MAKE_EDHOC_METHOD=3 edhoc-test-client.cooja TARGET=cooja</commands>
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
      $(MAKE) -j$(CPUS) DEBUG=1 MAKE_EDHOC_METHOD=3 edhoc-test-server.cooja TARGET=cooja</commands>
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
var hasError = false;
var correctCount = 0;

while (true) {
  log.log(id + " " + msg + "\n");  // Write all output to COOJA.testlog

  // Define the device type
  var device = (id == 1) ? "Client" : (id == 2) ? "Server" : "Unknown";
  device = "[MSG : EDHOC     ] " + device;

  // Check for fail condition
  if(!msg.contains("edhoc_deserialize_err")) {
    if (msg.contains("ERR ") || msg.contains("ERR_")) {
      log.testFailed();
    }
  }

  // Check for finish condition
  if (msg.contains("Client time to finish")) {
    if (hasError || 0 == correctCount) {
       log.testFailed();
    } else {
       log.testOK();
    }
  }

  if(msg.contains("Using test vector: false") || msg.contains("Connection method: 0") || msg.contains("Connection method: 1") || msg.contains("Connection method: 2")) {
    checkingEnabled = false;
  }
  if(checkingEnabled == false) {
    YIELD();
    continue;
  }

  // Check if the OSCORE master secret is correct
  if (msg.contains("OSCORE Master Secret")) {
    if (msg.contains("OSCORE Master Secret (16 bytes): f9 86 8f 6a 3a ca 78 a0 5d 14 85 b3 50 30 b1 62")) {
      log.log("C " + device + ": Correct master secret!\n");
      correctCount++;
    } else {
      log.log("I " + device + ": Incorrect master secret!\n");
      hasError = true;
    }
  }

  // Check if the OSCORE master salt is correct
  if (msg.contains("OSCORE Master Salt")) {
    if (msg.contains("OSCORE Master Salt (8 bytes): ad a2 4c 7d bf c8 5e eb")) {
      log.log("C " + device + ": Correct master salt!\n");
      correctCount++;      
    } else {
      log.log("I " + device + ": Incorrect master salt!\n");
      hasError = true;
    }
  }

  // Check if PRK_4e3m is correct
  if (msg.contains("PRK_4e3m")) {
    if (msg.contains("PRK_4e3m (32 bytes): 81 cc 8a 29 8e 35 70 44 e3 c4 66 bb 5c 0a 1e 50 7e 01 d4 92 38 ae ba 13 8d f9 46 35 40 7c 0f f7")) {
      log.log("C " + device + ": Correct PRK_4e3m!\n");
    } else {
      log.log("I " + device + ": Incorrect PRK_4e3m!\n");
      hasError = true;      
    }
  }

  // Check if info for SALT_4e3m is correct
  if (msg.contains("info SALT_4e3m")) {
    if (msg.contains("info SALT_4e3m (37 bytes): 05 58 20 ad af 67 a7 8a 4b cc 91 e0 18 f8 88 27 62 a7 22 00 0b 25 07 03 9d f0 bc 1b bf 0c 16 1b b3 15 5c 18 20")) {
      log.log("C " + device + ": Correct info for SALT_4e3m!\n");
    } else {
      log.log("I " + device + ": Incorrect info for SALT_4e3m!\n");
      hasError = true;
    }
  }

  // Check if SALT_4e3m is correct
  if (msg.contains("SALT_4e3m")) {
    if (!msg.contains("info SALT_4e3m")) {
      if (msg.contains("SALT_4e3m (32 bytes): cf dd f9 51 5a 7e 46 e7 b4 db ff 31 cb d5 6c d0 4b a3 32 25 0d e9 ea 5d e1 ca f9 f6 d1 39 14 a7")) {
        log.log("C " + device + ": Correct SALT_4e3m!\n");
      } else {
        log.log("I " + device + ": Incorrect SALT_4e3m!\n");
        hasError = true;        
      }
    }
  }

  // Check if TH_4 is correct
  if (msg.contains("TH_4")) {
    if(!msg.contains("input to calculate TH_4")) {
      if (msg.contains("TH_4 (32 bytes): c9 02 b1 e3 a4 32 6c 93 c5 55 1f 5f 3a a6 c5 ec c0 24 68 06 76 56 12 e5 2b 5d 99 e6 05 9d 6b 6e")) {
        log.log("C " + device + ": Correct TH_4!\n");
      } else {
        log.log("I " + device + ": Incorrect TH_4!\n");
        hasError = true;
      }
    }
  }

  // Check if PRK_out is correct
  if (msg.contains("PRK_out")) {
    if (msg.contains("PRK_out (32 bytes): 2c 71 af c1 a9 33 8a 94 0b b3 52 9c a7 34 b8 86 f3 0d 1a ba 0b 4d c5 1b ee ae ab df ea 9e cb f8")) {
      log.log("C " + device + ": Correct PRK_out!\n");
    } else {
      log.log("I " + device + ": Incorrect PRK_out!\n");
      hasError = true;
    }
  }

  // Check if PRK_exporter is correct
  if (msg.contains("PRK_exporter")) {
    if (msg.contains("PRK_exporter (32 bytes): e1 4d 06 69 9c ee 24 8c 5a 04 bf 92 27 bb cd 4c e3 94 de 7d cb 56 db 43 55 54 74 17 1e 64 46 db")) {
      log.log("C " + device + ": Correct PRK_exporter!\n");
    } else {
      log.log("I " + device + ": Incorrect PRK_exporter!\n");
      hasError = true;
    }
  }

  // Check if message_3 is correct (on reception)
  // Updated to also consider correct message_3 with prepended C_R
  if (msg.contains("RX message_3")) {
    if (msg.contains("RX message_3 (19 bytes): 52 e5 62 09 7b c4 17 dd 59 19 48 5a c7 89 1f fd 90 a9 fc") || msg.contains("RX message_3 (20 bytes): 27 52 e5 62 09 7b c4 17 dd 59 19 48 5a c7 89 1f fd 90 a9 fc")) {
      log.log("C " + device + ": Correct message_3!\n");
    } else {
      log.log("I " + device + ": Incorrect message_3!\n");
      hasError = true;
    }
  }
  YIELD();
}</script>
      <active>true</active>
    </plugin_config>
    <bounds x="1081" y="0" height="700" width="600" />
  </plugin>
</simconf>
