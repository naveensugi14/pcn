#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpUdpPerformanceAnalysis");

// Global counter for TCP retransmitted packets
uint32_t g_tcpRetransmissions = 0;

void TraceRetransmission(Ptr<const Packet> packet)
{
    g_tcpRetransmissions++;
}

int main(int argc, char *argv[])
{
    // Default assignment parameters
    std::string bottleneckBandwidth = "2Mbps";   // Exp 1: 2Mbps, 5Mbps, 10Mbps
    std::string bottleneckDelay = "10ms";        // Exp 4: 5ms, 10ms, 20ms, 50ms
    std::string udpRate = "1Mbps";               // Exp 2: 500Kbps, 1Mbps, 2Mbps, 5Mbps
    uint32_t queueSize = 50;                     // Exp 3: 20, 50, 100 packets
    double simTime = 20.0;                       // 20 seconds simulation time

    CommandLine cmd(__FILE__);
    cmd.AddValue("bottleneckBw", "Bandwidth of bottleneck link (e.g. 2Mbps, 5Mbps, 10Mbps)", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Propagation delay of bottleneck link (e.g. 5ms, 10ms, 20ms)", bottleneckDelay);
    cmd.AddValue("udpRate", "UDP OnOff application data rate (e.g. 500Kbps, 1Mbps, 2Mbps)", udpRate);
    cmd.AddValue("queueSize", "DropTail queue size in packets (e.g. 20, 50, 100)", queueSize);
    cmd.AddValue("simTime", "Total simulation run time", simTime);
    cmd.Parse(argc, argv);

    // 1. Configure TCP segment size
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    // -------------------------------------------------------------
    // 2. CREATE NODES
    // Nodes: n0 (TCP Source), n1 (UDP Source), r1 (Router 1),
    //        r2 (Router 2),   n2 (UDP Sink),   n3 (TCP Sink)
    // -------------------------------------------------------------
    NodeContainer endNodes;
    endNodes.Create(4); // 0: n0, 1: n1, 2: n2, 3: n3

    NodeContainer routerNodes;
    routerNodes.Create(2); // 0: r1, 1: r2

    // -------------------------------------------------------------
    // 3. CONFIGURE POINT-TO-POINT CHANNELS & QUEUE SIZES
    // -------------------------------------------------------------
    // Access Links: 10 Mbps, 2 ms delay
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));
    accessLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    // Bottleneck Link: Parameterized (Default: 2 Mbps, 10 ms delay)
    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    // Install NetDevices
    NetDeviceContainer dN0R1 = accessLink.Install(endNodes.Get(0), routerNodes.Get(0));
    NetDeviceContainer dN1R1 = accessLink.Install(endNodes.Get(1), routerNodes.Get(0));
    NetDeviceContainer dR1R2 = bottleneckLink.Install(routerNodes.Get(0), routerNodes.Get(1));
    NetDeviceContainer dR2N2 = accessLink.Install(routerNodes.Get(1), endNodes.Get(2));
    NetDeviceContainer dR2N3 = accessLink.Install(routerNodes.Get(1), endNodes.Get(3));

    // -------------------------------------------------------------
    // 4. INSTALL INTERNET STACK & IP ADDRESSING
    // -------------------------------------------------------------
    InternetStackHelper internet;
    internet.Install(endNodes);
    internet.Install(routerNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifN0R1 = ipv4.Assign(dN0R1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifN1R1 = ipv4.Assign(dN1R1);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR1R2 = ipv4.Assign(dR1R2);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR2N2 = ipv4.Assign(dR2N2);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifR2N3 = ipv4.Assign(dR2N3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // -------------------------------------------------------------
    // 5. TCP FLOW: Node 0 -> Node 3 (Port 5000)
    // -------------------------------------------------------------
    uint16_t tcpPort = 5000;
    Address tcpSinkAddress(InetSocketAddress(ifR2N3.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(endNodes.Get(3)); // n3 is sink
    tcpSinkApp.Start(Seconds(0.5));
    tcpSinkApp.Stop(Seconds(simTime));

    OnOffHelper tcpClient("ns3::TcpSocketFactory", tcpSinkAddress);
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    tcpClient.SetAttribute("DataRate", StringValue("10Mbps"));
    tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer tcpApp = tcpClient.Install(endNodes.Get(0)); // n0 is source
    tcpApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(simTime - 0.5));

    // Connect retransmission trace for TCP
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/Retransmit",
                                  MakeCallback(&TraceRetransmission));

    // -------------------------------------------------------------
    // 6. UDP FLOW: Node 1 -> Node 2 (Port 6000)
    // -------------------------------------------------------------
    uint16_t udpPort = 6000;
    Address udpSinkAddress(InetSocketAddress(ifR2N2.GetAddress(1), udpPort));
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", udpSinkAddress);
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(endNodes.Get(2)); // n2 is sink
    udpSinkApp.Start(Seconds(0.5));
    udpSinkApp.Stop(Seconds(simTime));

    OnOffHelper udpClient("ns3::UdpSocketFactory", udpSinkAddress);
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    udpClient.SetAttribute("DataRate", StringValue(udpRate));
    udpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer udpApp = udpClient.Install(endNodes.Get(1)); // n1 is source
    udpApp.Start(Seconds(1.0));
    udpApp.Stop(Seconds(simTime - 0.5));

    // -------------------------------------------------------------
    // 7. FLOWMONITOR FOR PERFORMANCE METRICS
    // -------------------------------------------------------------
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // -------------------------------------------------------------
    // 8. METRIC CALCULATION & COMPARISON OUTPUT
    // -------------------------------------------------------------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n================ SIMULATION RUN CONFIGURATION ================\n";
    std::cout << "Bottleneck Bandwidth : " << bottleneckBandwidth << "\n";
    std::cout << "Bottleneck Delay     : " << bottleneckDelay << "\n";
    std::cout << "UDP Offered Rate     : " << udpRate << "\n";
    std::cout << "DropTail Queue Limit : " << queueSize << " packets\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << std::left << std::setw(6) << "Flow"
              << std::setw(18) << "Type"
              << std::setw(12) << "Tx Packets"
              << std::setw(12) << "Rx Packets"
              << std::setw(14) << "Loss Rate(%)"
              << std::setw(18) << "Throughput(kbps)"
              << std::setw(18) << "Avg Delay(ms)" << "\n";

    for (const auto &iter : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter.first);
        std::string flowType = (t.destinationPort == tcpPort) ? "TCP (n0 -> n3)" : "UDP (n1 -> n2)";

        uint32_t tx = iter.second.txPackets;
        uint32_t rx = iter.second.rxPackets;

        double lossRate = 0.0;
        if (tx > 0)
        {
            lossRate = ((double)(tx - rx) / (double)tx) * 100.0;
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

        std::cout << std::left << std::setw(6) << iter.first
                  << std::setw(18) << flowType
                  << std::setw(12) << tx
                  << std::setw(12) << rx
                  << std::fixed << std::setprecision(2)
                  << std::setw(14) << lossRate
                  << std::setw(18) << throughput
                  << std::setw(18) << avgDelay << "\n";
    }

    std::cout << "--------------------------------------------------------------\n";
    std::cout << "Total Recorded TCP Retransmissions: " << g_tcpRetransmissions << "\n";
    std::cout << "==============================================================\n\n";

    Simulator::Destroy();
    return 0;
}