#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    CommandLine cmd;
    std::string tcpVariant = "ns3::TcpCubic";

    cmd.AddValue("transportProt",
                 "Transport protocol: ns3::TcpNewReno,ns3::TcpCubic,ns3::TcpBbr",
                 tcpVariant);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpVariant));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    NodeContainer vehicles;
    vehicles.Create(5);

    NodeContainer rsu;
    rsu.Create(1);

    NodeContainer server;
    server.Create(1);

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneckLink.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("50p"));

    NetDeviceContainer devV0 = accessLink.Install(vehicles.Get(0), rsu.Get(0));
    NetDeviceContainer devV1 = accessLink.Install(vehicles.Get(1), rsu.Get(0));
    NetDeviceContainer devV2 = accessLink.Install(vehicles.Get(2), rsu.Get(0));
    NetDeviceContainer devV3 = accessLink.Install(vehicles.Get(3), rsu.Get(0));
    NetDeviceContainer devV4 = accessLink.Install(vehicles.Get(4), rsu.Get(0));

    NetDeviceContainer devBtl = bottleneckLink.Install(rsu.Get(0), server.Get(0));

    InternetStackHelper internet;
    internet.Install(vehicles);
    internet.Install(rsu);
    internet.Install(server);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifV0 = ipv4.Assign(devV0);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifV1 = ipv4.Assign(devV1);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifV2 = ipv4.Assign(devV2);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifV3 = ipv4.Assign(devV3);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifV4 = ipv4.Assign(devV4);

    // Subnet for the Bottleneck Link (Server IP is index 1 = 10.1.10.2)
    ipv4.SetBase("10.1.10.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBtl = ipv4.Assign(devBtl);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t port = 8080;

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer tSinkApp = tcpSink.Install(server.Get(0));
    tSinkApp.Start(Seconds(0.0));
    tSinkApp.Stop(Seconds(20.0));

    BulkSendHelper tcpSource("ns3::TcpSocketFactory", InetSocketAddress(ifBtl.GetAddress(1), port));
    tcpSource.SetAttribute("SendSize", UintegerValue(1024));
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer tSrcApp = tcpSource.Install(vehicles);
    tSrcApp.Start(Seconds(1.0));
    tSrcApp.Stop(Seconds(20.0));

    bottleneckLink.EnablePcapAll("assign9-btl");

    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> mon = fmh.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    mon->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> cl = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = mon->GetFlowStats();

    std::cout << "\n================ Performance for " << tcpVariant << " ================\n";

    for (const auto& kv : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = cl->FindFlow(kv.first);
        const auto& s = kv.second;

        // Display forward data traffic targeting port 8080
        if (t.destinationPort != port)
            continue;

        double dur = (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();
        double mbps = (dur > 0) ? (s.rxBytes * 8.0 / dur / 1e6) : 0;
        double delay = (s.rxPackets > 0) ? (s.delaySum.GetSeconds() / s.rxPackets * 1000.0) : 0;
        double loss = (s.txPackets > 0) ? (100.0 * (s.txPackets - s.rxPackets) / s.txPackets) : 0;
        double pdr = (s.txPackets > 0) ? (100.0 * s.rxPackets / s.txPackets) : 0;

        std::cout << "Flow " << kv.first << " (" << t.sourceAddress << " -> Server):\n"
                  << "  Tx: " << s.txPackets << " | Rx: " << s.rxPackets
                  << " | Lost: " << s.lostPackets << "\n"
                  << "  Throughput: " << mbps << " Mbps\n"
                  << "  Avg Delay:  " << delay << " ms\n"
                  << "  Loss:       " << loss << " %\n"
                  << "  PDR:        " << pdr << " %\n"
                  << "--------------------------------------------------------\n";
    }

    Simulator::Destroy();
    return 0;
}
