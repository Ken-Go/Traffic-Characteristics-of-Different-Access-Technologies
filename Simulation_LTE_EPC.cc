#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/lte-module.h"
#include "fstream"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
//#include "ns3/gtk-config-store.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Simulation_LTE_EPC");

int
main (int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType",StringValue("ns3::TcpVegas"));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize",UintegerValue (1024));
  Config::SetDefault ("ns3::ComponentCarrier::UlBandwidth",UintegerValue(25));
  Config::SetDefault ("ns3::ComponentCarrier::DlBandwidth",UintegerValue(25));
  uint16_t numNodePairs = 10; //set node nums
  std::string prefix_file_name = "LTE_EPC";
  double distance = 60.0;
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  


  CommandLine cmd;
  cmd.Parse (argc, argv);

  // ConfigStore inputConfig;
  // inputConfig.ConfigureDefaults ();

  cmd.Parse(argc, argv);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();

  lteHelper->SetEpcHelper (epcHelper);
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0); 
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  //Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (numNodePairs);
  ueNodes.Create (numNodePairs);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numNodePairs; i++)
    {
      positionAlloc->Add (Vector (distance * i, 0, 0));
    }
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach one UE per eNodeB
  for (uint16_t i = 0; i < numNodePairs; i++)
    {
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(0));
      // side effect: the default EPS bearer will be activated
    }
  


  //******************Background Flow Link****************************///
  
  //***************Server Set *************//
  uint16_t port= 3000, port1 = 3001,port2=3002,port3=3003; 
  ApplicationContainer serverApps;
  
  //==============Video Sink=========================
  Address remotesinkLocalAddress(InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper remoteHelper ("ns3::TcpSocketFactory", remotesinkLocalAddress);
  serverApps.Add (remoteHelper.Install (remoteHostContainer.Get(0)));
  
  //ip:7.0.0.11 ================link1 sink=======================
  Address sinkLocalAddress(InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  serverApps.Add (sinkHelper.Install (ueNodes.Get (numNodePairs-1)));
  
  //ip:7.0.0.10 ================link2 sink======================
  Address sinkLocalAddress1(InetSocketAddress (Ipv4Address::GetAny (), port1));
  PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", sinkLocalAddress1);
  serverApps.Add (sinkHelper1.Install (ueNodes.Get (numNodePairs-2)));

  //ip:7.0.0.9 ================link3 sink========================
  Address sinkLocalAddress2(InetSocketAddress (Ipv4Address::GetAny (), port2));
  PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", sinkLocalAddress2);
  serverApps.Add (sinkHelper2.Install (ueNodes.Get (numNodePairs-3)));

  //ip:7.0.0.8 =================link4 sink===================================
  Address sinkLocalAddress3(InetSocketAddress (Ipv4Address::GetAny (), port3));
  PacketSinkHelper sinkHelper3 ("ns3::TcpSocketFactory", sinkLocalAddress3);
  serverApps.Add (sinkHelper3.Install (ueNodes.Get (numNodePairs-4)));

  serverApps.Start (Seconds (0.0));
  serverApps.Stop  (Seconds (21.0));

  ///************************Clent Set*************************************//
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address()); 
  clientHelper.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("PacketSize",UintegerValue(1024));
  ApplicationContainer clientApps1,clientApps1_c,clientApps1_e,clientApps1_e_c,clientApps2,clientApps2_e,clientApps3,clientApps3_e,client_video;
  
  //==================video send=================================
  clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("3Mbps"))); //video send rate
  AddressValue remoteAddress(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-1),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  client_video = clientHelper.Install(ueNodes.Get(0));
  
  //=====================background send========================= 
  clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("4Mbps"))); //background send rate
  //########First Link Client#########   ip =  7.0.0.3
  // remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-2),port1));
  remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-1),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps1 = clientHelper.Install(ueNodes.Get(1));
  clientApps1_e = clientHelper.Install(ueNodes.Get(1));

  
  //#######Second Link Client#########   ip = 7.0.0.5
  // remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-3),port2));
   remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-1),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps2 = clientHelper.Install(ueNodes.Get(2));
  clientApps2_e = clientHelper.Install(ueNodes.Get(2));

  //##### Third Link Client#########      ip = 7.0.0.6
  // remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-4),port3));
   remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-1),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps3 = clientHelper.Install(ueNodes.Get(3));
  clientApps3_e = clientHelper.Install(ueNodes.Get(3));

//Time Chart
//+++++++++++++++++++++++++++++++++Network Traffic+++++++++++++++++++++++++++++++++
//"*":表示线路正在传输
//time :0--1--2--3--4--5--6--7--8--9--10--11--12--13--14--15--16--17--18--19--20
//video:************************************************************************
//link1:      *******************                              *********
//link2:            ********************          ******
//link3:                  **********************               *********

  client_video.Start (Seconds (0));
  client_video.Stop  (Seconds (21.0));

  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop  (Seconds (8.0));

  clientApps2.Start (Seconds (4.0));
  clientApps2.Stop  (Seconds (10.0));

  clientApps3.Start (Seconds (6.0));
  clientApps3.Stop  (Seconds (12.0));

  clientApps1_e.Start (Seconds(16.0));
  clientApps1_e.Stop  (Seconds(18.0));

  clientApps2_e.Start (Seconds(12.0));
  clientApps2_e.Stop  (Seconds(14.0));

  clientApps3_e.Start (Seconds(16.0));
  clientApps3_e.Stop  (Seconds(18.0));

  
  //lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  p2ph.EnablePcapAll("Simulation_LTE_EPC",true);
  FlowMonitorHelper flowHelper;

  flowHelper.InstallAll ();
  
  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();


  flowHelper.SerializeToXmlFile (prefix_file_name + ".flowmonitor", true, true);
  
  

  Simulator::Destroy ();
  return 0;
}
