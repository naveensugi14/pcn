#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/rip-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingEvaluation");

// Function to simulate dynamic link failure
void TearDownLink(Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t interfaceA, uint32_t interfaceB)
{
  nodeA->GetObject<Ipv4>()->SetDown(interfaceA);
  nodeB->GetObject<Ipv4>()->SetDown(interfaceB);
  std::cout << "[SIMULATION EVENT] At t = 8.0s: Intermediate primary link R1 <--> R2 went DOWN!\n";
}

int main(int argc, char *argv[])
{
  std::string protocol = "OSPF";    // "RIP", "OSPF", or "AODV"
  std::string condition = "normal"; // "normal", "congested", or "failure"
  double simTime = 16.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("protocol", "Routing protocol to evaluate (RIP, OSPF, AODV)", protocol);
  cmd.AddValue("condition", "Network condition (normal, congested, failure)", condition);
  cmd.AddValue("simTime", "Total simulation run time in seconds", simTime);
  cmd.Parse(argc, argv);

  std::cout << "\n============================================================\n";
  std::cout << "Running Protocol: " << protocol << " | Condition: " << condition << "\n";
  std::cout << "============================================================\n";

  // -------------------------------------------------------------
  // 1. TOPOLOGY: Multi-Router Department Network
  // Nodes:
  // H1 (Dept A) --- R1 ---------- R2 --- H2 (Dept B)
  //                  \           /
  //                   \-- R3 ---/   (Alternate Backup Route)
  // -------------------------------------------------------------
  NodeContainer hosts;
  hosts.Create(2); // H1 (0), H2 (1)

  NodeContainer routers;
  routers.Create(3); // R1 (0), R2 (1), R3 (2)

  // Point-to-Point Links Configuration
  PointToPointHelper p2pFast, p2pBottleneck;
  p2pFast.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2pFast.SetChannelAttribute("Delay", StringValue("2ms"));

  p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
  p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

  // Host-Router connections
  NetDeviceContainer dH1R1 = p2pFast.Install(hosts.Get(0), routers.Get(0));
  NetDeviceContainer dH2R2 = p2pFast.Install(hosts.Get(1), routers.Get(1));

  // Inter-Router links
  NetDeviceContainer dR1R2 = p2pFast.Install(routers.Get(0), routers.Get(1)); // Primary Link
  NetDeviceContainer dR1R3 = p2pFast.Install(routers.Get(0), routers.Get(2)); // Alternate path
  NetDeviceContainer dR3R2 = p2pFast.Install(routers.Get(2), routers.Get(1)); // Alternate path

  // -------------------------------------------------------------
  // 2. ROUTING PROTOCOL INSTALLATION
  // -------------------------------------------------------------
  InternetStackHelper internet;

  if (protocol == "RIP")
  {
    RipHelper rip;
    // Exclude host-facing interfaces from RIP if needed
    internet.SetRoutingHelper(rip);
    internet.Install(routers);
    internet.Install(hosts);
  }
  else if (protocol == "AODV")
  {
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(routers);
    internet.Install(hosts);
  }
  else // OSPF / Link-State Shortest Path
  {
    Ipv4GlobalRoutingHelper globalRouting;
    internet.SetRoutingHelper(globalRouting);
    internet.Install(routers);
    internet.Install(hosts);
  }

  // -------------------------------------------------------------
  // 3. IP ADDRESS ASSIGNMENT
  // -------------------------------------------------------------
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifH1R1 = ipv4.Assign(dH1R1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifH2R2 = ipv4.Assign(dH2R2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR1R2 = ipv4.Assign(dR1R2);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR1R3 = ipv4.Assign(dR1R3);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR3R2 = ipv4.Assign(dR3R2);

  if (protocol == "OSPF")
  {
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  }

  // -------------------------------------------------------------
  // 4. TRAFFIC GENERATION (CBR / UDP)
  // -------------------------------------------------------------
  uint16_t sinkPort = 9000;
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(hosts.Get(1)); // Sink at H2
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(simTime));

  // Base Application Traffic: H1 -> H2
  OnOffHelper onoff("ns3::UdpSocketFactory",
                    InetSocketAddress(ifH2R2.GetAddress(0), sinkPort));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  if (condition == "congested")
  {
    // High data rate to cause packet queue drop & congestion
    onoff.SetAttribute("DataRate", StringValue("12Mbps"));
  }
  else
  {
    // Normal data rate
    onoff.SetAttribute("DataRate", StringValue("1.5Mbps"));
  }

  ApplicationContainer clientApps = onoff.Install(hosts.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simTime - 1.0));

  // -------------------------------------------------------------
  // 5. INTRODUCE LINK FAILURE (Condition 3)
  // -------------------------------------------------------------
  if (condition == "failure")
  {
    // Fail the primary link R1 <-> R2 at t = 8.0s
    Simulator::Schedule(Seconds(8.0), &TearDownLink,
                        routers.Get(0), routers.Get(1),
                        ifR1R2.Get(0).second, ifR1R2.Get(1).second);

    if (protocol == "OSPF")
    {
      // OSPF SPF route recomputation
      Simulator::Schedule(Seconds(8.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);
    }
  }

  // Print Routing Table Updates
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
  if (protocol == "RIP")
  {
    RipHelper rip;
    rip.PrintRoutingTableAt(Seconds(7.5), routers.Get(0), routingStream);
    if (condition == "failure")
    {
      rip.PrintRoutingTableAt(Seconds(12.0), routers.Get(0), routingStream);
    }
  }

  // -------------------------------------------------------------
  // 6. PERFORMANCE MEASUREMENTS (FlowMonitor)
  // -------------------------------------------------------------
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Compute Metrics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  std::cout << "\n------------------- EXPERIMENTAL EVALUATION RESULTS -------------------\n";
  std::cout << std::left << std::setw(8) << "FlowID"
            << std::setw(12) << "Tx Packets"
            << std::setw(12) << "Rx Packets"
            << std::setw(12) << "PDR (%)"
            << std::setw(18) << "Throughput(kbps)"
            << std::setw(18) << "Avg Delay(ms)" << "\n";

  for (const auto &iter : stats)
  {
    uint32_t tx = iter.second.txPackets;
    uint32_t rx = iter.second.rxPackets;

    double pdr = 0.0;
    if (tx > 0)
    {
      pdr = ((double)rx / (double)tx) * 100.0;
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
              << std::setw(12) << pdr
              << std::setw(18) << throughput
              << std::setw(18) << avgDelay << "\n";
  }
  std::cout << "-----------------------------------------------------------------------\n\n";

  Simulator::Destroy();
  return 0;
}