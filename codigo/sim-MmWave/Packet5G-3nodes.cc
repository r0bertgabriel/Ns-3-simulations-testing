/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// SIMULAÇÃO COM 3 USUARIOS SEM ARGUMENTO DE ENTRADA //
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include <ns3/buildings-helper.h>
#include "ns3/config-store.h"
#include "ns3/mmwave-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/isotropic-antenna-model.h"
#include <map>
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/global-route-manager.h"

using namespace ns3;
using namespace mmwave;

void
TxMacPacketTraceUe (Ptr<OutputStreamWrapper> stream, uint16_t rnti, uint8_t ccId, uint32_t size)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << (uint32_t)ccId << '\t' << size << std::endl;
}

void
Traces (std::string filePath)
{
  std::string path = "/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/MmWaveUeMac/TxMacPacketTraceUe";
  filePath = filePath + "TxMacPacketTraceUe.txt";
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (filePath);
  *stream1->GetStream () << "Time" << "\t" << "CC" << '\t' << "Packet size" << std::endl;
  Config::ConnectWithoutContextFailSafe (path, MakeBoundCallback (&TxMacPacketTraceUe, stream1));
}

int
main (int argc, char *argv[])
{
  bool blockage = false;
  bool useEpc = true;
  double totalBandwidth = 200e6;
  double frequency = 100.0e9;
  double simTime = 60;
  std::string condition = "l";

  CommandLine cmd;
  cmd.AddValue ("blockage", "If enabled blockage = true", blockage);
  cmd.AddValue ("frequency", "CC central frequency", frequency);
  cmd.AddValue ("totalBandwidth", "System bandwidth in Hz", totalBandwidth);
  cmd.AddValue ("simTime", "Simulation time", simTime);
  cmd.AddValue ("useEpc", "If enabled use EPC, else use RLC saturation mode", useEpc);
  cmd.AddValue ("condition", "Channel condition, l = LOS, n = NLOS, otherwise the condition is randomly determined", condition);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  
  // Creating Objects
  Ptr<MmWavePhyMacCommon> phyMacConfig0 = CreateObject<MmWavePhyMacCommon> ();
  phyMacConfig0->SetBandwidth (totalBandwidth);
  phyMacConfig0->SetCentreFrequency (frequency);
  
  Ptr<MmWaveComponentCarrier> cc0 = CreateObject<MmWaveComponentCarrier> ();
  cc0->SetConfigurationParameters (phyMacConfig0);
  cc0->SetAsPrimary (true);
  
  std::map<uint8_t, MmWaveComponentCarrier> ccMap;
  ccMap [0] = *cc0;
  
    // create and set the helper
  
  Config::SetDefault ("ns3::MmWaveHelper::ChannelModel",StringValue ("ns3::ThreeGppSpectrumPropagationLossModel"));
  Config::SetDefault ("ns3::ThreeGppChannelModel::Scenario", StringValue ("UMa"));
  Config::SetDefault ("ns3::ThreeGppChannelModel::Blockage", BooleanValue (blockage)); // Enable/disable the blockage model  
  Config::SetDefault ("ns3::MmWaveHelper::PathlossModel",StringValue ("ns3::ThreeGppUmaPropagationLossModel"));

  // by default, isotropic antennas are used. To use the 3GPP radiation pattern instead, use the <ThreeGppAntennaArrayModel>
  // beware: proper configuration of the bearing and downtilt angles is needed
  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement", PointerValue (CreateObject<IsotropicAntennaModel> ())); 

  Ptr<MmWaveHelper> helper = CreateObject<MmWaveHelper> ();
  
  helper->SetCcPhyParams (ccMap);
  if (condition == "l")
  {
    helper->SetChannelConditionModelType ("ns3::AlwaysLosChannelConditionModel");
  }
  else if (condition == "n")
  {
    helper->SetChannelConditionModelType ("ns3::NeverLosChannelConditionModel");
  }
  
  // create the EPC
  Ipv4Address remoteHostAddr;
  Ptr<Node> remoteHost;
  InternetStackHelper internet;
  Ptr<MmWavePointToPointEpcHelper> epcHelper;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  if (useEpc)
    {
      epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
      helper->SetEpcHelper (epcHelper);

      // create the Internet by connecting remoteHost to pgw. Setup routing too
      Ptr<Node> pgw = epcHelper->GetPgwNode ();

      // create remotehost
      NodeContainer remoteHostContainer;
      remoteHostContainer.Create (1);
      internet.Install (remoteHostContainer);
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ipv4InterfaceContainer internetIpIfaces;

      remoteHost = remoteHostContainer.Get (0);
      // create the Internet
      PointToPointHelper p2ph;
      p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("10Gb/s")));
      p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
      p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

      NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

      Ipv4AddressHelper ipv4h;
      ipv4h.SetBase ("1.0.0.0", "255.255.0.0");
      internetIpIfaces = ipv4h.Assign (internetDevices);
      // interface 0 is localhost, 1 is the p2p device
      remoteHostAddr = internetIpIfaces.GetAddress (1);

      Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
      remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.255.0.0"), 1);
    }

  // create the enb node
  NodeContainer enbNodes;
  enbNodes.Create (1);

  // set mobility
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (250.0,250.0,1.6)); // Posição padrão da Antena

  MobilityHelper enbmobility;
  enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator (enbPositionAlloc);
  enbmobility.Install (enbNodes);
  BuildingsHelper::Install (enbNodes);

  // install enb device
  NetDeviceContainer enbNetDevices = helper->InstallEnbDevice (enbNodes);
  std::cout << "eNB device installed" << std::endl;

  // create ue nodes
  NodeContainer ueNodes;
  ueNodes.Create (3);  // Create two UEs

  // set mobility for UEs with fixed positions
  MobilityHelper uemobility;
  uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  
  // Define the positions for each UE
  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (250.23037587,299.99946927,1.6)); // Position for UE 1
  uePositionAlloc->Add (Vector (212.67793643,216.72743516,1.6)); // Position for UE 2
  uePositionAlloc->Add (Vector (299.80260502,245.5614717,1.6)); // Position for UE 3
  
  uemobility.SetPositionAllocator (uePositionAlloc);
  uemobility.Install (ueNodes);
  BuildingsHelper::Install (ueNodes);

  std::cout << "UE1 position: " << ueNodes.Get (0)->GetObject<MobilityModel> ()->GetPosition () << std::endl;
  std::cout << "UE2 position: " << ueNodes.Get (1)->GetObject<MobilityModel> ()->GetPosition () << std::endl;
  std::cout << "UE3 position: " << ueNodes.Get (2)->GetObject<MobilityModel> ()->GetPosition () << std::endl;

  // install ue devices
  NetDeviceContainer ueNetDevices = helper->InstallUeDevice (ueNodes);
  std::cout << "UE devices installed" << std::endl;
  
  if (useEpc)
    {
      // install the IP stack on the UEs
      internet.Install (ueNodes);
      Ipv4InterfaceContainer ueIpIface;
      ueIpIface = epcHelper->AssignUeIpv4Address (ueNetDevices);
      // assign IP address to UEs, and install applications
      // set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting1 = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (0)->GetObject<Ipv4> ());
      ueStaticRouting1->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
      
      Ptr<Ipv4StaticRouting> ueStaticRouting2 = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (1)->GetObject<Ipv4> ());
      ueStaticRouting2->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

      Ptr<Ipv4StaticRouting> ueStaticRouting3 = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (2)->GetObject<Ipv4> ());
      ueStaticRouting2->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

      helper->AttachToClosestEnb (ueNetDevices, enbNetDevices);

      // install and start applications on UEs and remote host
      uint16_t dlPort = 1234;
      uint16_t ulPort = 2000;
      ApplicationContainer clientApps;
      ApplicationContainer serverApps;

      uint16_t interPacketInterval = 10;

      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (0)));
      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

      UdpClientHelper dlClient (ueIpIface.GetAddress (0), dlPort);
      dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds (interPacketInterval)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));

      UdpClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (MilliSeconds (interPacketInterval)));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));

      clientApps.Add (dlClient.Install (remoteHost));
      clientApps.Add (ulClient.Install (ueNodes.Get (0)));

      serverApps.Start (Seconds (0.01));
      clientApps.Start (Seconds (0.01));

      // Similar setup for the second UE
      PacketSinkHelper dlPacketSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort + 1));
      PacketSinkHelper ulPacketSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort + 1));
      serverApps.Add (dlPacketSinkHelper2.Install (ueNodes.Get (1)));
      serverApps.Add (ulPacketSinkHelper2.Install (remoteHost));

      UdpClientHelper dlClient2 (ueIpIface.GetAddress (1), dlPort + 1);
      dlClient2.SetAttribute ("Interval", TimeValue (MilliSeconds (interPacketInterval)));
      dlClient2.SetAttribute ("MaxPackets", UintegerValue (1000000));

      UdpClientHelper ulClient2 (remoteHostAddr, ulPort + 1);
      ulClient2.SetAttribute ("Interval", TimeValue (MilliSeconds (interPacketInterval)));
      ulClient2.SetAttribute ("MaxPackets", UintegerValue (1000000));

      clientApps.Add (dlClient2.Install (remoteHost));
      clientApps.Add (ulClient2.Install (ueNodes.Get (1)));

      serverApps.Start (Seconds (0.01));
      clientApps.Start (Seconds (0.01));

      // Similar setup for the third UE
      PacketSinkHelper dlPacketSinkHelper3("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort + 2));
      PacketSinkHelper ulPacketSinkHelper3("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ulPort + 2));
      serverApps.Add(dlPacketSinkHelper3.Install(ueNodes.Get(2)));
      serverApps.Add(ulPacketSinkHelper3.Install(remoteHost));

      UdpClientHelper dlClient3(ueIpIface.GetAddress(2), dlPort + 2);
      dlClient3.SetAttribute("Interval", TimeValue(MilliSeconds(interPacketInterval)));
      dlClient3.SetAttribute("MaxPackets", UintegerValue(1000000));

      UdpClientHelper ulClient3(remoteHostAddr, ulPort + 2);
      ulClient3.SetAttribute("Interval", TimeValue(MilliSeconds(interPacketInterval)));
      ulClient3.SetAttribute("MaxPackets", UintegerValue(1000000));

      clientApps.Add(dlClient3.Install(remoteHost));
      clientApps.Add(ulClient3.Install(ueNodes.Get(2)));

      serverApps.Start (Seconds (0.01));
      clientApps.Start (Seconds (0.01));

    }
    
  else
    {
      helper->AttachToClosestEnb (ueNetDevices, enbNetDevices);

      // activate a data radio bearer
      enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
      EpsBearer bearer (q);
      helper->ActivateDataRadioBearer (ueNetDevices, bearer);
    }
  
  helper->EnableTraces ();
  Traces ("./"); // enable UL MAC traces
  

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  
  //flowMonitor->SerializeToXmlFile("flow5g.xml", true, true);     
  
  return 0;
}
