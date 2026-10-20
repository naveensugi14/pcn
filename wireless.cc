#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HeterogeneousCampusNetwork");

// Global or static metrics collector for PHY-layer RSSI tracking
double g_lastWifiRssiDbm = -100.0;
double g_lastZigbeeRssiDbm = -100.0;

void MonitorWifiPhyRx(std::string context, Ptr<const Packet> packet, uint16_t channelFreqMhz,
                      WifiTxVector txVector, MpduInfo mpduInfo, SignalNoiseDbm snr)
{
    g_lastWifiRssiDbm = snr.signal; // Captured RSSI in dBm
}

void MonitorLrWpanPhyRx(std::string context, Ptr<const Packet> packet, double rxPowerDbm)
{
    g_lastZigbeeRssiDbm = rxPowerDbm; // Captured RSSI in dBm
}

int main(int argc, char *argv[])
{
    double nodeSpeed = 1.0;          // Speed in m/s (1 m/s, 5 m/s, 10 m/s)
    double simulationTime = 20.0;    // Simulation time in seconds
    uint32_t numWifiNodes = 3;
    uint32_t numZigbeeNodes = 3;

    CommandLine cmd(__FILE__);
    cmd.AddValue("speed", "Mobile node speed in m/s (e.g. 1, 5, 10)", nodeSpeed);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    std::cout << "\n============================================================\n";
    std::cout << "Running Heterogeneous Campus Simulation at Speed: " << nodeSpeed << " m/s\n";
    std::cout << "============================================================\n";

    // -------------------------------------------------------------
    // 1. NODE CONTAINERS
    // -------------------------------------------------------------
    // Gateways / Access Points
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer zigbeeCoordNode;
    zigbeeCoordNode.Create(1);

    // Mobile Stations / End Devices
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numWifiNodes);

    NodeContainer zigbeeDevNodes;
    zigbeeDevNodes.Create(numZigbeeNodes);

    // -------------------------------------------------------------
    // 2. MOBILITY SETUP (RandomWaypointMobilityModel)
    // -------------------------------------------------------------
    MobilityHelper mobility;

    // Fixed Gateway Positions
    Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
    gatewayAlloc->Add(Vector(0.0, 0.0, 0.0));   // Wi-Fi AP at Origin
    gatewayAlloc->Add(Vector(50.0, 50.0, 0.0)); // Zigbee/WPAN Coordinator
    mobility.SetPositionAllocator(gatewayAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(zigbeeCoordNode);

    // Random Waypoint Mobility for Campus Mobile Nodes
    std::ostringstream speedStr;
    speedStr << "ns3::ConstantRandomVariable[Constant=" << nodeSpeed << "]";

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue(speedStr.str()),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "PositionAllocator", StringValue("ns3::RandomBoxPositionAllocator[X=0.0..100.0|Y=0.0..100.0|Z=0.0..0.0]"));
    mobility.Install(wifiStaNodes);
    mobility.Install(zigbeeDevNodes);

    // -------------------------------------------------------------
    // 3. WI-FI NETWORK CONFIGURATION
    // -------------------------------------------------------------
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("Campus-WiFi");

    // AP Device
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer wifiApDevices = wifi.Install(wifiPhy, wifiMac, wifiApNode);

    // STA Devices
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer wifiStaDevices = wifi.Install(wifiPhy, wifiMac, wifiStaNodes);

    // -------------------------------------------------------------
    // 4. ZIGBEE / LR-WPAN CONFIGURATION (IEEE 802.15.4)
    // -------------------------------------------------------------
    LrWpanHelper lrwpanHelper;
    NetDeviceContainer zigbeeCoordDevices = lrwpanHelper.Install(zigbeeCoordNode);
    NetDeviceContainer zigbeeDevDevices = lrwpanHelper.Install(zigbeeDevNodes);

    // Associate Zigbee devices
    lrwpanHelper.AssociateToPan(zigbeeCoordDevices, 0);
    lrwpanHelper.AssociateToPan(zigbeeDevDevices, 0);

    // -------------------------------------------------------------
    // 5. INTERNET STACK & IP ADDRESSING (Wi-Fi Campus Network)
    // -------------------------------------------------------------
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiApInterface = address.Assign(wifiApDevices);
    Ipv4InterfaceContainer wifiStaInterfaces = address.Assign(wifiStaDevices);

    // -------------------------------------------------------------
    // 6. APPLICATIONS & TRAFFIC GENERATION
    // -------------------------------------------------------------
    uint16_t sinkPort = 8080;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = sinkHelper.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.5));
    sinkApps.Stop(Seconds(simulationTime));

    // Traffic from Wi-Fi mobile stations to Campus Gateway
    for (uint32_t i = 0; i < numWifiNodes; ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(wifiApInterface.GetAddress(0), sinkPort));
        onoff.SetAttribute("DataRate", StringValue("512Kbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(512));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApps = onoff.Install(wifiStaNodes.Get(i));
        clientApps.Start(Seconds(1.0 + (i * 0.2)));
        clientApps.Stop(Seconds(simulationTime - 0.5));
    }

    // Connect trace sinks for physical signal power (RSSI)
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
                    MakeCallback(&MonitorWifiPhyRx));

    // -------------------------------------------------------------
    // 7. FLOWMONITOR FOR END-TO-END METRIC COLLECTION
    // -------------------------------------------------------------
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // -------------------------------------------------------------
    // 8. METRIC CALCULATION & REPORTING
    // -------------------------------------------------------------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n------------------ PERFORMANCE EVALUATION REPORT ------------------\n";
    std::cout << std::left << std::setw(8) << "FlowID"
              << std::setw(12) << "Tx Packets"
              << std::setw(12) << "Rx Packets"
              << std::setw(16) << "Packet Loss(%)"
              << std::setw(18) << "Throughput(kbps)"
              << std::setw(18) << "End-to-End Delay(ms)" << "\n";

    for (const auto &iter : stats)
    {
        uint32_t tx = iter.second.txPackets;
        uint32_t rx = iter.second.rxPackets;

        double packetLoss = 0.0;
        if (tx > 0)
        {
            packetLoss = ((double)(tx - rx) / (double)tx) * 100.0;
        }

        double duration = iter.second.timeLastRxPacket.GetSeconds() - iter.second.timeFirstTxPacket.GetSeconds();
        double throughput = 0.0;
        if (duration > 0 && rx > 0)
        {
            throughput = (iter.second.rxBytes * 8.0) / (duration * 1000.0);
        }

        double avgDelay = 0.0;
        if (rx > 0)
        {
            avgDelay = (iter.second.delaySum.GetMilliSeconds()) / (double)rx;
        }

        std::cout << std::left << std::setw(8) << iter.first
                  << std::setw(12) << tx
                  << std::setw(12) << rx
                  << std::fixed << std::setprecision(2)
                  << std::setw(16) << packetLoss
                  << std::setw(18) << throughput
                  << std::setw(18) << avgDelay << "\n";
    }

    std::cout << "\n[Physical Layer Observation]\n";
    std::cout << "Last Measured Received Signal Strength (RSSI): "
              << std::fixed << std::setprecision(2) << g_lastWifiRssiDbm << " dBm\n";
    std::cout << "-------------------------------------------------------------------\n\n";

    Simulator::Destroy();
    return 0;
}