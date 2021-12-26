#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/data-rate.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>


// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Simulation_WIFI_LAN");



int 
main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",StringValue("ns3::TcpVegas"));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize",UintegerValue (1024));
 
  bool verbose = true;
  // set node number
  uint32_t nCsma = 10;
  uint32_t nWifi = 10;
  bool tracing = true;
  bool flow_monitor = true;
  CommandLine cmd;


  cmd.Parse (argc,argv);

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the
  // bounding box if more than 18 nodes are provided.
  if (nWifi > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
      return 1;
    }
  if (verbose)
    {
      LogComponentEnable("Simulation_WIFI_LAN",LOG_LEVEL_INFO);
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));//6560

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  //set WIFI mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  // mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (csmaNodes);
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);
  
  
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  //+++++++++++++++++++++++++++++++++NetWork Traffic Schedule+++++++++++++++++++++++++++++++++
//"*":Link is transmitting
//time :0--1--2--3--4--5--6--7--8--9--10--11--12--13--14--15--16--17--18--19--20
//video:************************************************************************
//link1:      *******************                              *********
//link2:            ********************          ******
//link3:                  **********************               *********

//***********Server Set**************///
  //Tcp Server Set
  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  //Tcp Wifi
  ApplicationContainer other_serverApps = sinkHelper.Install (wifiStaNodes.Get(nWifi-1));
  other_serverApps.Add(sinkHelper.Install(wifiStaNodes.Get(nWifi-2)));
  other_serverApps.Add(sinkHelper.Install(wifiStaNodes.Get(nWifi-3)));
  other_serverApps.Add(sinkHelper.Install(wifiStaNodes.Get(nWifi-4)));

  other_serverApps.Start (Seconds (0.0));
  other_serverApps.Stop (Seconds (21.0));


  //********************Client Set**************************//
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address()); 
  clientHelper.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("PacketSize",UintegerValue(1024));
  clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("4Mbps")));
  ApplicationContainer clientApps1,clientApps1_e,clientApps2,clientApps2_e,clientApps3,clientApps3_e,video_apps;


  //=============video TCP WIFI Send================================= 
  AddressValue remoteAddress(InetSocketAddress(staInterfaces.GetAddress(nWifi-1),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  video_apps = clientHelper.Install(wifiStaNodes.Get(1));//Client ip address is :10.1.3.2

  
  video_apps.Start (Seconds (0.0));
  video_apps.Stop  (Seconds (21.0));


  //background rate
  clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("2Mbps"))); 
  // //#######  First link wifi node #######
  remoteAddress = AddressValue(InetSocketAddress(staInterfaces.GetAddress(nWifi-2),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps1 = clientHelper.Install(wifiStaNodes.Get(2));//Client ip address is :10.1.3.2
  clientApps1_e = clientHelper.Install(wifiStaNodes.Get(2));

  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop  (Seconds (8.0));

  clientApps1_e.Start (Seconds(16.0));
  clientApps1_e.Stop  (Seconds(18.0));


  
  //############## second link : wifi node ##########
  remoteAddress = AddressValue(InetSocketAddress(staInterfaces.GetAddress(nWifi-3),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps2 = clientHelper.Install(wifiStaNodes.Get(3));//Client ip adddress: 10.1.3.3
  clientApps2_e = clientHelper.Install(wifiStaNodes.Get(3));

  clientApps2.Start (Seconds (4.0));
  clientApps2.Stop  (Seconds (10.0));
 
  clientApps2_e.Start (Seconds(12.0));
  clientApps2_e.Stop  (Seconds(14.0));


  //##########  third link wifi node  ########  
  remoteAddress = AddressValue(InetSocketAddress(staInterfaces.GetAddress(nWifi-4),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps3 = clientHelper.Install(wifiStaNodes.Get(4)); //Client ip address: 10.1.3.4
  clientApps3_e = clientHelper.Install(wifiStaNodes.Get(4));
  
  clientApps3.Start (Seconds (6.0));
  clientApps3.Stop  (Seconds (12.0));

  clientApps3_e.Start (Seconds(16.0));
  clientApps3_e.Stop  (Seconds(18.0));

  
  if (tracing == true)
  {
    // // trace WIFI Node
    phy.EnablePcap ("third_wifi", apDevices.Get (0),true);
    phy.EnablePcap ("third_wifi", staDevices.Get (0));
    phy.EnablePcap ("third_wifi", staDevices.Get (1));
    phy.EnablePcap ("third_wifi", staDevices.Get (2));
    phy.EnablePcap ("third_wifi", staDevices.Get (3));
    phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-5));
    phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-2));
    phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-3));
    phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-4));
  }
  
  //========flow monitor name=====================
  std::string prefix_file_name = "TcpWifiLan_WIFI";
  
  FlowMonitorHelper flowHelper;

  if (flow_monitor)
  {
    flowHelper.InstallAll ();
  }
  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();

  if (flow_monitor)
  {
    flowHelper.SerializeToXmlFile (prefix_file_name + ".flowmonitor", true, true);
  }
  
  Simulator::Destroy ();
  
  return 0;
}




