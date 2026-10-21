#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    double speed = 5.0;

    CommandLine cmd;
    cmd.AddValue("speed", "Speed of the mobile nodes in m/s", speed);
    cmd.Parse(argc, argv);

    // 1. Create Nodes
    NodeContainer gateways;
    gateways.Create(3); // 0 = Wi-Fi AP, 1 = Zigbee Coord, 2 = Bluetooth GW

    NodeContainer mobileNodes;
    mobileNodes.Create(3); // 0 = Wi-Fi STA, 1 = Zigbee Sensor, 2 = Bluetooth Node

    // 2. Wi-Fi Subnet (802.11n, 16 dBm TxPower)
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HtMcs7"),
                                 "ControlMode",
                                 StringValue("HtMcs0"));

    WifiMacHelper wifiMac;
    Ssid ssidWifi = Ssid("campus-wifi");

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidWifi));
    NetDeviceContainer devWifiAp = wifi.Install(wifiPhy, wifiMac, gateways.Get(0));

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidWifi));
    NetDeviceContainer devWifiSta = wifi.Install(wifiPhy, wifiMac, mobileNodes.Get(0));

    // 3. Zigbee-Profile Subnet (Low-rate DSSS 1Mbps, 3 dBm TxPower)
    YansWifiChannelHelper zigbeeChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper zigbeePhy;
    zigbeePhy.SetChannel(zigbeeChannel.Create());
    zigbeePhy.Set("TxPowerStart", DoubleValue(3.0));
    zigbeePhy.Set("TxPowerEnd", DoubleValue(3.0));

    WifiHelper zigbeeWifi;
    zigbeeWifi.SetStandard(WIFI_STANDARD_80211b);
    zigbeeWifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode",
                                       StringValue("DsssRate1Mbps"),
                                       "ControlMode",
                                       StringValue("DsssRate1Mbps"));

    WifiMacHelper zigbeeMac;
    Ssid ssidZigbee = Ssid("campus-zigbee");

    zigbeeMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidZigbee));
    NetDeviceContainer devZigbeeAp = zigbeeWifi.Install(zigbeePhy, zigbeeMac, gateways.Get(1));

    zigbeeMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidZigbee));
    NetDeviceContainer devZigbeeSta = zigbeeWifi.Install(zigbeePhy, zigbeeMac, mobileNodes.Get(1));

    // 4. Bluetooth-Profile Subnet (Short-range DSSS 2Mbps, 0 dBm TxPower)
    YansWifiChannelHelper btChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper btPhy;
    btPhy.SetChannel(btChannel.Create());
    btPhy.Set("TxPowerStart", DoubleValue(0.0));
    btPhy.Set("TxPowerEnd", DoubleValue(0.0));

    WifiHelper btWifi;
    btWifi.SetStandard(WIFI_STANDARD_80211b);
    btWifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                   "DataMode",
                                   StringValue("DsssRate2Mbps"),
                                   "ControlMode",
                                   StringValue("DsssRate1Mbps"));

    WifiMacHelper btMac;
    Ssid ssidBt = Ssid("campus-bluetooth");

    btMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssidBt));
    NetDeviceContainer devBtAp = btWifi.Install(btPhy, btMac, gateways.Get(2));

    btMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidBt));
    NetDeviceContainer devBtSta = btWifi.Install(btPhy, btMac, mobileNodes.Get(2));

    // 5. Mobility Models
    MobilityHelper mobility;

    // Stationary Gateways
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gateways);

    // Mobile Users moving with command-line speed parameter
    std::ostringstream speedStr;
    speedStr << "ns3::ConstantRandomVariable[Constant=" << speed << "]";

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(5.0),
                                  "MinY",
                                  DoubleValue(5.0),
                                  "DeltaX",
                                  DoubleValue(10.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-80, 80, -80, 80)),
                              "Speed",
                              StringValue(speedStr.str()));
    mobility.Install(mobileNodes);

    // 6. Internet Stack & Addressing
    InternetStackHelper internet;
    internet.Install(gateways);
    internet.Install(mobileNodes);

    Ipv4AddressHelper ipv4;

    // Subnet 1: Wi-Fi (192.168.1.0/24)
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifWifiAp = ipv4.Assign(devWifiAp);
    Ipv4InterfaceContainer ifWifiSta = ipv4.Assign(devWifiSta);

    // Subnet 2: Zigbee (192.168.2.0/24)
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifZigbeeAp = ipv4.Assign(devZigbeeAp);
    Ipv4InterfaceContainer ifZigbeeSta = ipv4.Assign(devZigbeeSta);

    // Subnet 3: Bluetooth (192.168.3.0/24)
    ipv4.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBtAp = ipv4.Assign(devBtAp);
    Ipv4InterfaceContainer ifBtSta = ipv4.Assign(devBtSta);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 7. Applications: Traffic from Mobile Nodes to Gateways
    uint16_t port = 9000;

    // Sinks on all 3 Gateways
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(gateways);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    // Wi-Fi Client (2 Mbps)
    OnOffHelper clientWifi("ns3::UdpSocketFactory",
                           InetSocketAddress(ifWifiAp.GetAddress(0), port));
    clientWifi.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientWifi.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientWifi.SetAttribute("DataRate", StringValue("2Mbps"));
    clientWifi.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer appWifi = clientWifi.Install(mobileNodes.Get(0));
    appWifi.Start(Seconds(1.0));
    appWifi.Stop(Seconds(20.0));

    // Zigbee Client (50 Kbps)
    OnOffHelper clientZigbee("ns3::UdpSocketFactory",
                             InetSocketAddress(ifZigbeeAp.GetAddress(0), port));
    clientZigbee.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientZigbee.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientZigbee.SetAttribute("DataRate", StringValue("50Kbps"));
    clientZigbee.SetAttribute("PacketSize", UintegerValue(128));
    ApplicationContainer appZigbee = clientZigbee.Install(mobileNodes.Get(1));
    appZigbee.Start(Seconds(1.0));
    appZigbee.Stop(Seconds(20.0));

    // Bluetooth Client (200 Kbps)
    OnOffHelper clientBt("ns3::UdpSocketFactory", InetSocketAddress(ifBtAp.GetAddress(0), port));
    clientBt.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientBt.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientBt.SetAttribute("DataRate", StringValue("200Kbps"));
    clientBt.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer appBt = clientBt.Install(mobileNodes.Get(2));
    appBt.Start(Seconds(1.0));
    appBt.Stop(Seconds(20.0));

    // 8. FlowMonitor & Performance Metrics
    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> mon = fmh.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    mon->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> cl = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = mon->GetFlowStats();

    std::cout << "\n================ Heterogeneous Network Results (Speed = " << speed
              << " m/s) ================\n";

    if (cl)
    {
        for (const auto& kv : stats)
        {
            Ipv4FlowClassifier::FiveTuple t = cl->FindFlow(kv.first);
            const auto& s = kv.second;

            if (t.destinationPort != port)
                continue;

            double dur = (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();
            double mbps = (dur > 0) ? (s.rxBytes * 8.0 / dur / 1e6) : 0;
            double delay = (s.rxPackets > 0) ? (s.delaySum.GetSeconds() / s.rxPackets * 1000.0) : 0;
            double loss =
                (s.txPackets > 0) ? (100.0 * (s.txPackets - s.rxPackets) / s.txPackets) : 0;
            double pdr = (s.txPackets > 0) ? (100.0 * s.rxPackets / s.txPackets) : 0;

            std::string tech = "Unknown";
            if (t.destinationAddress == "192.168.1.1")
                tech = "Wi-Fi (802.11n)";
            else if (t.destinationAddress == "192.168.2.1")
                tech = "Zigbee (Sensor)";
            else if (t.destinationAddress == "192.168.3.1")
                tech = "Bluetooth (Short-Range)";

            std::cout << "\n[" << tech << "] Flow " << kv.first << " (" << t.sourceAddress << " -> "
                      << t.destinationAddress << "):\n"
                      << "  Tx: " << s.txPackets << " | Rx: " << s.rxPackets
                      << " | Lost: " << s.lostPackets << "\n"
                      << "  Throughput: " << mbps << " Mbps\n"
                      << "  Avg Delay:  " << delay << " ms\n"
                      << "  Loss:       " << loss << " %\n"
                      << "  PDR:        " << pdr << " %\n";
        }
    }

    Simulator::Destroy();
    return 0;
}
