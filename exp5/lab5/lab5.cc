/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
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
 *
 */



/*
	LAB Assignment #5

	Solution by: Konstantinos Katsaros (K.Katsaros@surrey.ac.uk)
	based on wifi-simple-adhoc-grid.cc
*/

// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// Flow 1: 0->24
// Flow 2: 20->4


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/flow-monitor-module.h"
#include "myapp.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("Lab5");

using namespace ns3;



int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double distance = 500;  // m
  uint32_t numNodes = 25;  // by default, 5x5
  double interval = 0.01; // seconds  Default = 0.001
  uint32_t packetSize = 500; // bytes  Default = 600
  uint32_t numPackets = 1;
  std::string rtslimit = "1500";  //Default = 1000000 1500
  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "distance (m)", packetSize);
  cmd.AddValue ("rtslimit", "RTS/CTS Threshold (bytes)", rtslimit);
  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (rtslimit));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-10) ); 
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a non-QoS upper mac, and disable rate control
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  MobilityHelper mobility;
  /*----------------------------------------------- */




  //set position allocator
	mobility.SetPositionAllocator( 
			"ns3::GridPositionAllocator",
			"MinX", DoubleValue(0), 
			"MinY", DoubleValue(0), 
			"DeltaX", DoubleValue(distance), 
			"DeltaY", DoubleValue(distance),
			"GridWidth", UintegerValue(5),
			"LayoutType",StringValue ("RowFirst"));



  /*----------------------------------------------- */
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  // Enable Routing Protocol (OLSR/AODV/DSDV)
  OlsrHelper olsr;
  AodvHelper aodv;
  DsdvHelper dsdv;

  Ipv4ListRoutingHelper list;
  list.Add (dsdv, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifcont = ipv4.Assign (devices);

  // Create Apps

  uint16_t sinkPort = 6; // use the same for all apps

  /*flow1 *///------------------------------------------------
  // UDP connection from N0 to N24
  Address sinkAddress1 (InetSocketAddress (ifcont.GetAddress (24), sinkPort)); // interface of n24
  PacketSinkHelper packetSinkHelper1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps1 = packetSinkHelper1.Install (c.Get (24)); //n2 as sink
  sinkApps1.Start (Seconds (0.));
  sinkApps1.Stop (Seconds (100.));

  Ptr<Socket> ns3UdpSocket1 = Socket::CreateSocket (c.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application at n0
  Ptr<MyApp> app1 = CreateObject<MyApp> ();
  app1->Setup (ns3UdpSocket1, sinkAddress1, packetSize, numPackets, DataRate ("1Mbps"));
  c.Get (0)->AddApplication (app1);
  app1->SetStartTime (Seconds (31.));
  app1->SetStopTime (Seconds (100.));
  /*flow1 end *///------------------------------------------------
  
  /*flow2 *///------------------------------------------------
  // UDP connection from N20 to N4
    //your code
  /*
  Address sinkAddress2 (InetSocketAddress (ifcont.GetAddress (4), sinkPort)); // interface of n24
  PacketSinkHelper packetSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps2 = packetSinkHelper2.Install (c.Get (4)); //n2 as sink
  sinkApps2.Start (Seconds (0.));
  sinkApps2.Stop (Seconds (100.));

  Ptr<Socket> ns3UdpSocket2 = Socket::CreateSocket (c.Get (20), UdpSocketFactory::GetTypeId ()); //source at n0
    
  // Create UDP application at n20
    //your code
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app2->Setup (ns3UdpSocket2, sinkAddress2, packetSize, numPackets, DataRate ("1Mbps"));
  c.Get (20)->AddApplication (app2);
  app2->SetStartTime (Seconds (31.));
  app2->SetStopTime (Seconds (100.));

  */
  
  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  wifiPhy.EnablePcap ("lab-5-solved", devices);
 
  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();

  double Avg_e2eDelaySec1 = 0.0;
  double Avg_PDR1 = 0.0;
  double Avg_SystemThroughput_bps1 = 0.0;
  double Avg_e2eDelaySec2 = 0.0;
  double Avg_PDR2 = 0.0;
  double Avg_SystemThroughput_bps2 = 0.0;
  // Print per flow statistics
  /*implement flow monitor */
  /*----------------------------------------------- */
	monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
	//-------------------------------------------------------------------------------------------
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
	{
		Ipv4FlowClassifier::FiveTuple ft = classifier->FindFlow(i->first);
		if( ft.destinationAddress == "10.1.1.25" && ft.sourceAddress == "10.1.1.1" )
		{
			Avg_e2eDelaySec1 = (i->second.delaySum.GetSeconds())/(i->second.rxPackets);
			Avg_PDR1 = double(i->second.rxPackets)/double(i->second.txPackets);
			Avg_SystemThroughput_bps1 =
						( 
						  double(i->second.rxBytes)*8/(i->second.timeLastRxPacket.GetSeconds()-i->second.timeFirstTxPacket.GetSeconds())
						);
		}
		if( ft.destinationAddress == "10.1.1.5" && ft.sourceAddress == "10.1.1.21" )
		{
			Avg_e2eDelaySec2 = (i->second.delaySum.GetSeconds())/(i->second.rxPackets);
			Avg_PDR2 = double(i->second.rxPackets)/double(i->second.txPackets);
			Avg_SystemThroughput_bps2 =
						( 
						  double(i->second.rxBytes)*8/(i->second.timeLastRxPacket.GetSeconds()-i->second.timeFirstTxPacket.GetSeconds())
						);
		}
	}
	
	std::cout << "DSDV, 2Flow" <<std::endl;

	std::cout << "Flow ID: 1\n";
 	std::cout << "Avg System Throughput : ";
	std::cout << Avg_SystemThroughput_bps1/1024
		  << " [kbps]\n";
	std::cout << "Avg end-to-end delay : ";
	std::cout << Avg_e2eDelaySec1
		  << "[sec]\n";
	std::cout << "Average Packet Delivery Ratio : ";
	std::cout << Avg_PDR1
		  << "\n";
	
        std::cout << "Flow ID: 2\n";
 	std::cout << "Avg System Throughput : ";
	std::cout << Avg_SystemThroughput_bps2/1024
		  << " [kbps]\n";
	std::cout << "Avg end-to-end delay : ";
	std::cout << Avg_e2eDelaySec2
		  << "[sec]\n";
	std::cout << "Average Packet Delivery Ratio : ";
	std::cout << Avg_PDR2
		  << "\n";

    //your code
  /*----------------------------------------------- */
  
  monitor->SerializeToXmlFile("lab-5.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}

