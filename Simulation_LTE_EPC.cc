#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "fstream"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
//#include "ns3/gtk-config-store.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Simulation_LTE_EPC");
//Global variables: used to set the relevant parameters of the video transmission line
char infile[] = "test.mp4"; //  Video input file
char outfilename[] = "recv.mp4";  //Video output file

//Record the reading position of the video file, each packet is 1024 bytes,
//so the new packet reading position is: num * 1024;
int num = 0;  

//Record the amount of video data read this time. When the end of the file is reached, 
//the read data may not be 1024 bytes
int readSize = 0;  

//Record the Max Lantency
double lantency = 0;

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

//Overrid the Send() method of the UdpEchoClient class to achieve the transmission video stream line 
//(only part of the code is added, the source code is not deleted)
void 
UdpEchoClient::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  //add my code

  //open file
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
    readSize = iffile.gcount();  //record the data size
    //fill data to the packet
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
  p->AddPacketTag(tag);
  m_socket->Send (p);
  ++m_sent;

  if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv4Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (InetSocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (m_peerAddress).GetPort ());
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << " port " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetPort ());
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
        lantency = now_delay;
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
      int  size = readSize; 
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
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                      Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                      Inet6SocketAddress::ConvertFrom (from).GetPort ());
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
  uint16_t numNodePairs = 10; //set node nums
  Time interPacketInterval = MilliSeconds (1);
  double distance = 60.0;
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("LenaSimpleEpc", LOG_LEVEL_INFO);


  CommandLine cmd;
  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

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
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
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
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
      // side effect: the default EPS bearer will be activated
    }
  //*******************************Video Link******************************
  //Server ip: 7.0.0.11
  UdpEchoServerHelper others_server(9); 
  ApplicationContainer serverApps_video = others_server.Install(ueNodes.Get(numNodePairs-1));

  serverApps_video.Start (Seconds (0.0));
  serverApps_video.Stop (Seconds (21.0));
  //Client ip:7.0.0.2
  UdpEchoClientHelper  echoClient (ueIpIface.GetAddress(numNodePairs-1),9);
  ApplicationContainer clientApps_video = echoClient.Install (ueNodes.Get(0));

  clientApps_video.Start (Seconds (0.0));
  clientApps_video.Stop (Seconds (21.0));



  //******************Background Flow Link****************************///
  
  //***************Server Set *************//
  uint16_t port1 = 3001,port2=3002,port3=3003; 
  ApplicationContainer serverApps;
  //ip:7.0.0.10
  PacketSinkHelper pl1PacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port1));
  serverApps.Add (pl1PacketSinkHelper.Install (ueNodes.Get (numNodePairs-2)));
  //ip:7.0.0.9
  PacketSinkHelper pl2PacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (),port2));
  serverApps.Add (pl2PacketSinkHelper.Install (ueNodes.Get (numNodePairs-3)));
  //ip:7.0.0.8
  PacketSinkHelper pl3PacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port3));
  serverApps.Add (pl3PacketSinkHelper.Install (ueNodes.Get (numNodePairs-4)));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop  (Seconds (21.0));

  ///************************Clent Set*************************************//
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address()); 
  clientHelper.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute("PacketSize",UintegerValue(1024));
  clientHelper.SetAttribute("DataRate",DataRateValue(DataRate("9Mbps")));
  ApplicationContainer clientApps1,clientApps1_e,clientApps2,clientApps2_e,clientApps3,clientApps3_e,client_video;
  
  //########First Link Client#########   ip =  7.0.0.3
  AddressValue remoteAddress(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-2),port1));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps1 = clientHelper.Install(ueNodes.Get(1));
  clientApps1_e = clientHelper.Install(ueNodes.Get(1));
  

  //#######Second Link Client#########   ip = 7.0.0.5
  remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-3),port2));
  clientHelper.SetAttribute("Remote",remoteAddress);
  clientApps2 = clientHelper.Install(ueNodes.Get(2));
  clientApps2_e = clientHelper.Install(ueNodes.Get(2));

  //#####Third Link Client#########      ip = 7.0.0.6
  remoteAddress = AddressValue(InetSocketAddress(ueIpIface.GetAddress(numNodePairs-4),port3));
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
  
  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
