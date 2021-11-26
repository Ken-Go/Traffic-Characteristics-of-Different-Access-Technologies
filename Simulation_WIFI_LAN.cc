
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

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



//Global variables: used to set the relevant parameters of the video transmission line
char infile[] = "test.mp4"; //Video input file
char outfilename[] = "recv.mp4"; //Video output file
//Record the reading position of the video file, each packet is 1024 bytes,
//so the new packet reading position is: num * 1024;
int num = 0;

//Record the amount of video data read this time. When the end of the file is reached, 
//the read data may not be 1024 bytes
int lastnum = 0;

//Record the Max Lantency
double delay  = 0;

//Overrid Tag: Send the sending time of the data packet as a tag to the receiving end,
//and the receiving end can calculate Latency according to the receiving time and sending time
class MyTag : public Tag
 {
 public:
   static TypeId GetTypeId (void);
   virtual TypeId GetInstanceTypeId (void) const;
   virtual uint32_t GetSerializedSize (void) const;
   virtual void Serialize (TagBuffer i) const;
   virtual void Deserialize (TagBuffer i);
   virtual void Print (std::ostream &os) const;
 
   // these are our accessors to our tag structure
   void SetstartTime (double value);
   double GetstartTime (void) const;
 private:
   double m_startTime;  
 };
TypeId 
 MyTag::GetTypeId (void)
 {
   static TypeId tid = TypeId ("ns3::MyTag")
     .SetParent<Tag> ()
     .AddConstructor<MyTag> ()
     .AddAttribute ("SimpleValue",
                    "A simple value",
                    EmptyAttributeValue (),
                    MakeDoubleAccessor (&MyTag::GetstartTime),
                    MakeDoubleChecker<uint8_t> ())
   ;
   return tid;
 }
 TypeId 
 MyTag::GetInstanceTypeId (void) const
 {
   return GetTypeId ();
 }
 uint32_t 
 MyTag::GetSerializedSize (void) const
 {
   return 128;
 }
 void 
 MyTag::Serialize (TagBuffer i) const
 {
   i.WriteDouble (m_startTime);
 }
 void 
 MyTag::Deserialize (TagBuffer i)
 {
   m_startTime = i.ReadDouble ();
 }
 void 
 MyTag::Print (std::ostream &os) const
 {
   os << "v=" << m_startTime;
 }
 void 
 MyTag::SetstartTime (double value)
 {
   m_startTime = value;
 }
 double
 MyTag::GetstartTime (void) const
 {
   return m_startTime;
 }
 


void 
UdpEchoClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  //My code
  //open file test.mp4
  std::ifstream iffile;
  iffile.open(infile,std::ios::binary|std::ios::in|std::ios::ate);
  iffile.seekg (0, std::ios::end);
  char fill[1024];
  if(iffile.is_open()){
    //find the location
    iffile.seekg (num*1024, std::ios::beg);
    num++;
    //read data
    iffile.read(fill,1024);
    lastnum = iffile.gcount();
    //Set the data fill of the packet
    this->SetFill((uint8_t *)fill,iffile.gcount(),iffile.gcount());
  }  
  
  
  if (m_dataSize)
    {
      //
      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      NS_ASSERT_MSG (m_dataSize == m_size, "UdpEchoClient::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "UdpEchoClient::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);
    }
  else
    {
      //
      // If m_dataSize is zero, the client has indicated that it doesn't care
      // about the data itself either by specifying the data size by setting
      // the corresponding attribute or by not calling a SetFill function.  In
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      p = Create<Packet> (m_size);
    }
  Address localAddress;
  m_socket->GetSockName (localAddress);
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      m_txTraceWithAddresses (p, localAddress, InetSocketAddress (Ipv4Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      m_txTraceWithAddresses (p, localAddress, Inet6SocketAddress (Ipv6Address::ConvertFrom (m_peerAddress), m_peerPort));
    }
  //My code
  //set the start time as the tag to packet
  MyTag tag;
  tag.SetstartTime(Simulator::Now().GetSeconds());
  std::cout<<"tag time is:"<<tag.GetstartTime()<<std::endl;
  p->AddPacketTag(tag);
  m_socket->Send (p);
  ++m_sent;

  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv4Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
      start_time = Simulator::Now().GetSeconds();             
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
      start_time = Simulator::Now().GetSeconds();   
    }
  else if (InetSocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (m_peerAddress).GetPort ());
      start_time = Simulator::Now().GetSeconds();
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << " port " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetPort ());
      start_time = Simulator::Now().GetSeconds();
    }
  //my code
  if (!iffile.eof()) 
    {
      //set the send packet intervalue: 0.001 s
      ScheduleTransmit (Seconds(0.001));
    }
  iffile.close();
}
//Override UdpEchoServer::HandleRead() to handle the video data 
void 
 UdpEchoServer::HandleRead (Ptr<Socket> socket)
 {
   NS_LOG_FUNCTION (this << socket);
 
   Ptr<Packet> packet;
   Address from;
   Address localAddress;
   double now_delay,now_time,last_time;
   while ((packet = socket->RecvFrom (from)))
    {
       //my code
      //Get start time and calculate the lantency
      MyTag tag;
      packet->PeekPacketTag(tag);
      last_time = tag.GetstartTime();
      now_time = Simulator::Now().GetSeconds();    
      now_delay = now_time - last_time;
      if(now_delay > delay){
        delay = now_delay;
      }
      std::cout<<"max delay is:"<<delay<<std::endl;

      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      //My code
      //Open file "recv.mp4"
      std::ofstream outfile;
      outfile.open(outfilename,std::ios::binary|std::ios::out|std::ios::app);
      char fill[1024];
      int  size = lastnum;      
      if(outfile.is_open()){
         //copy video data from packet
        packet->CopyData((uint8_t *)fill,size);
        //write to recv.mp4
        outfile.write(fill,size);
      }  

      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                      InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                      InetSocketAddress::ConvertFrom (from).GetPort ());
          end_time = Simulator::Now().GetSeconds();            
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                      Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                      Inet6SocketAddress::ConvertFrom (from).GetPort ());
          end_time = Simulator::Now().GetSeconds();
        }
      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      NS_LOG_LOGIC ("Echoing packet");
      socket->SendTo (packet, 0, from);

      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                      InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                      InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
                      Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                      Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }
    }
 }



int 
main (int argc, char *argv[])
{
  bool verbose = true;
  // set node number
  uint32_t nCsma = 10;
  uint32_t nWifi = 10;
  bool tracing = true;

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
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("ThirdScriptExample", LOG_LEVEL_INFO);
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
  

  //*****************Video Link***********************//
  
  UdpEchoServerHelper echoServer (9);
  //wifi server
  //ApplicationContainer serverApps = echoServer.Install (wifiStaNodes.Get(nWifi-1)); //wifi server ip:10.1.3.10
  //CSMA Server
  ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get(nCsma));  //csma server ip:10.1.2.11

  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (21.0));

  //wifi client
  //UdpEchoClientHelper echoClient (staInterfaces.GetAddress(nWifi-1),9);
  //ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (0));  //client ip:10.1.3.1

  //Csma Clinet
  UdpEchoClientHelper  echoClient (csmaInterfaces.GetAddress(nCsma),9);
  ApplicationContainer clientApps = echoClient.Install (csmaNodes.Get(0));  //client ip:10.1.2.1

  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (21.0));
  

  //+++++++++++++++++++++++++++++++++NetWork Traffic+++++++++++++++++++++++++++++++++
//"*":Link is transmitting
//time :0--1--2--3--4--5--6--7--8--9--10--11--12--13--14--15--16--17--18--19--20
//video:************************************************************************
//link1:      *******************                              *********
//link2:            ********************          ******
//link3:                  **********************               *********

//***********Server Set**************///
  UdpServerHelper others_server(10);
  //Csma Server Set
  ApplicationContainer other_serverApps = others_server.Install(csmaNodes.Get(nCsma-1)); //server ip:10.1.2.10
  other_serverApps.Add(others_server.Install(csmaNodes.Get(nCsma-2)));//server ip:10.1.2.9
  other_serverApps.Add(others_server.Install(csmaNodes.Get(nCsma-3)));//server ip:10.1.2.8

  //WIFI Server Set
  // ApplicationContainer other_serverApps = others_server.Install(wifiStaNodes.Get(nWifi-2));//server ip:10.1.3.9
  // other_serverApps.Add(others_server.Install(wifiStaNodes.Get(nWifi-3)));//server ip:10.1.3.8
  // other_serverApps.Add(others_server.Install(wifiStaNodes.Get(nWifi-4)));//server ip:10.1.3.7

  other_serverApps.Start (Seconds (0.0));
  other_serverApps.Stop (Seconds (21.0));

  //********************Client Set**************************//
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address()); 
  clientHelper.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("PacketSize",UintegerValue(1024));
  clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("2Mbps")));
  ApplicationContainer clientApps1,clientApps1_e,clientApps2,clientApps2_e,clientApps3,clientApps3_e,video_apps;
  uint16_t port = 10;


  //######## First Link Csma Node  Set#########
  AddressValue remoteAddress(InetSocketAddress(csmaInterfaces.GetAddress(nCsma-1),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps1 = clientHelper.Install(csmaNodes.Get(1)); //Client ip address is :10.1.2.2
  clientApps1_e = clientHelper.Install(csmaNodes.Get(1));
  // //#######  second link wifi node #######
  // AddressValue remoteAddress(InetSocketAddress(staInterfaces.GetAddress(nWifi-2),port));
  // clientHelper.SetAttribute("Remote",remoteAddress);
  // clientApps1 = clientHelper.Install(wifiStaNodes.Get(1));//Client ip address is :10.1.3.2
  // clientApps1_e = clientHelper.Install(wifiStaNodes.Get(1));

  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop  (Seconds (8.0));

  clientApps1_e.Start (Seconds(16.0));
  clientApps1_e.Stop  (Seconds(18.0));

  //############## second link:  csma node   ########
  remoteAddress = AddressValue(InetSocketAddress(csmaInterfaces.GetAddress(nCsma-2),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps2 = clientHelper.Install(csmaNodes.Get(2)); //Client ip adddress: 10.1.2.3
  clientApps2_e = clientHelper.Install(csmaNodes.Get(2));
  //############## second link : wifi node ##########
  // remoteAddress = AddressValue(InetSocketAddress(staInterfaces.GetAddress(nWifi-3),port));
  // clientHelper.SetAttribute("Remote",remoteAddress);
  // clientApps2 = clientHelper.Install(wifiStaNodes.Get(2));//Client ip adddress: 10.1.3.3
  // clientApps2_e = clientHelper.Install(wifiStaNodes.Get(2));

  clientApps2.Start (Seconds (4.0));
  clientApps2.Stop  (Seconds (10.0));
 
  clientApps2_e.Start (Seconds(12.0));
  clientApps2_e.Stop  (Seconds(14.0));

  //########### third link  csma node ######
  remoteAddress = AddressValue(InetSocketAddress(csmaInterfaces.GetAddress(nCsma-3),port));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps3 = clientHelper.Install(csmaNodes.Get(3)); //Client ip address: 10.1.2.4
  clientApps3_e = clientHelper.Install(csmaNodes.Get(3));
  //##########  third link wifi node  ########  
  // remoteAddress = AddressValue(InetSocketAddress(staInterfaces.GetAddress(nWifi-4),port));
  // clientHelper.SetAttribute("Remote",remoteAddress);
  // clientApps3 = clientHelper.Install(wifiStaNodes.Get(3));Client ip address: 10.1.3.4
  // clientApps3_e = clientHelper.Install(wifiStaNodes.Get(3));
  
  clientApps3.Start (Seconds (6.0));
  clientApps3.Stop  (Seconds (12.0));

  clientApps3_e.Start (Seconds(16.0));
  clientApps3_e.Stop  (Seconds(18.0));


  Simulator::Stop (Seconds (21.0));

  if (tracing == true)
    {
      // trace WIFI Node
      // phy.EnablePcap ("third_wifi", apDevices.Get (0),true);
      // phy.EnablePcap ("third_wifi", staDevices.Get (0));
      // phy.EnablePcap ("third_wifi", staDevices.Get (1));
      // phy.EnablePcap ("third_wifi", staDevices.Get (2));
      // phy.EnablePcap ("third_wifi", staDevices.Get (3));
      // phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-1));
      // phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-2));
      // phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-3));
      // phy.EnablePcap ("third_wifi", staDevices.Get (nWifi-4));

      //trace the csma node
      csma.EnablePcap ("third_csma", csmaDevices.Get (0));
      csma.EnablePcap ("third_csma", csmaDevices.Get (1));
      csma.EnablePcap ("third_csma", csmaDevices.Get (2));
      csma.EnablePcap ("third_csma", csmaDevices.Get (3));
      csma.EnablePcap ("third_csma", csmaDevices.Get (nCsma),true);
      csma.EnablePcap ("third_csma", csmaDevices.Get (nCsma-1),true);
      csma.EnablePcap ("third_csma", csmaDevices.Get (nCsma-2));
      csma.EnablePcap ("third_csma", csmaDevices.Get (nCsma-3));
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}




