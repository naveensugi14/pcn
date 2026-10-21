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

#include <iostream>
using namespace ns3;

int
main(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer hosts;
    hosts.Create(4);

    NodeContainer routers;
    routers.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("50p"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("50p"));

    NetDeviceContainer dev0r1 = p2p.Install(hosts.Get(0), routers.Get(0));
    NetDeviceContainer dev1r1 = p2p.Install(hosts.Get(1), routers.Get(0));
    NetDeviceContainer devBtl = bottleneck.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer devR2N3 = p2p.Install(routers.Get(1), hosts.Get(3));
    NetDeviceContainer devR2N2 = p2p.Install(routers.Get(1), hosts.Get(2));

    InternetStackHelper internet;
    internet.Install(hosts);
    internet.Install(routers);

    Ipv4AddressHelper ipv4;
    // Subnet for Access Link 1 (Node 0 to Router 1)
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface0r1 = ipv4.Assign(dev0r1);
    // Subnet for Access Link 2 (Node 1 to Router 1)
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface1r1 = ipv4.Assign(dev1r1);
    // Subnet for Central Bottleneck Link (Router 1 to Router 2)
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceBtl = ipv4.Assign(devBtl);
    // Subnet for Egress Link (Router 2 to Node 2)
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceR2N2 = ipv4.Assign(devR2N2);
    // Subnet for Egress Link (Router 2 to Node 3)
    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceR2N3 = ipv4.Assign(devR2N3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), 8080));
    ApplicationContainer tSinkApp = tcpSink.Install(hosts.Get(3));
    tSinkApp.Start(Seconds(0.0));
    tSinkApp.Stop(Seconds(20.0));

    BulkSendHelper tcpSource("ns3::TcpSocketFactory",
                             InetSocketAddress(ifaceR2N3.GetAddress(1), 8080));
    tcpSource.SetAttribute("SendSize", UintegerValue(1024));
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer tSrcApp = tcpSource.Install(hosts.Get(0));
    tSrcApp.Start(Seconds(1.0));
    tSrcApp.Stop(Seconds(20.0));

    UdpServerHelper server1(5000);
    ApplicationContainer sApp1 = server1.Install(hosts.Get(2));
    sApp1.Start(Seconds(0.0));
    sApp1.Stop(Seconds(20.0));
    // Sender Client on Node 1 targeting Port 5000
    // Rate formula: Interval = (PacketSize * 8) / DesiredDataRate
    // For 1 Mbps and 512B packet: (512 * 8) / 1,000,000 = 0.004096s
    UdpClientHelper client1(ifaceR2N2.GetAddress(1), 5000);
    client1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client1.SetAttribute("PacketSize", UintegerValue(512));
    client1.SetAttribute("Interval", TimeValue(Seconds(512 * 8.0 / 1e6)));
    ApplicationContainer cApp1 = client1.Install(hosts.Get(1));
    cApp1.Start(Seconds(1.0));
    cApp1.Stop(Seconds(20.0));

    p2p.EnablePcapAll("sim-p2p");
    bottleneck.EnablePcapAll("sim-bottleneck");

    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> mon = fmh.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    mon->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> cl = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = mon->GetFlowStats();

    for (const auto& kv : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = cl->FindFlow(kv.first);
        const auto& s = kv.second;

        if (t.destinationPort != 8080 && t.destinationPort != 5000)
        {
            continue;
        }

        double dur = (s.timeLastRxPacket - s.timeFirstTxPacket).GetSeconds();
        double mbps = (dur > 0) ? (s.rxBytes * 8.0 / dur / 1e6) : 0;
        double delay = (s.rxPackets > 0) ? (s.delaySum.GetSeconds() / s.rxPackets * 1000.0) : 0;
        double pdr = (s.txPackets > 0) ? (100.0 * s.rxPackets / s.txPackets) : 0;
        double loss = (s.txPackets > 0) ? (100.0 * (s.txPackets - s.rxPackets) / s.txPackets) : 0;

        std::cout << "\nFlow" << kv.first << "(" << t.sourceAddress << "->" << t.destinationAddress
                  << "):\n"
                  << "Throughput :" << mbps << "Mbps\n"
                  << "Avg delay :" << delay << "ms\n"
                  << " PDR :" << pdr << "\n";
    }

    Simulator::Destroy();
    return 0;
}
