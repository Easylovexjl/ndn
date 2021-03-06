/*
 * SDN.cc
 *
 *  Created on: Oct 9, 2015
 *      Author: chl
 */
/*
  ./waf --run "scratch/SDN --traceFile=/home/chl/ns-allinone-3.23/ns-3.23/scratch/mobility.tcl --nodeNum=135 --duration=131.0 --logFile=SDN.log --numPackets=130"
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <dirent.h>//DIR*
#include "SDN.h"

NS_LOG_COMPONENT_DEFINE ("SDN");


using namespace ns3;


VanetSim::VanetSim()
{
	traceFile = "";
	logFile = "SDN.log";
	phyMode = "OfdmRate6MbpsBW10MHz";
	lossModle = "ns3::FriisPropagationLossModel";
	freq1 = 5.860e9;  //802.11p SCH CH172
	freq2 = 5.890e9;  //802.11p CCH CH178
	txp1 = 20;  // dBm SCH
	txp2 = 20;  // CCH
	range1 = 800.0;//SCH
	range2 = 1200.0;//CCH
	packetSize = 1000; // bytes
	numPackets = 1;
	interval = 0.1; // seconds
	verbose = false;
	mod = 0;
	pmod = 0;
	duration = -1;
	nodeNum = 0;
	Rx_Data_Bytes = 0;
	Rx_Data_Pkts = 0;
	Rx_Routing_Bytes = 0;
	RX_Routing_Pkts = 0;
	Tx_Data_Bytes = 0;
	Tx_Data_Pkts = 0;
	Tx_Routing_Bytes = 0;
	TX_Routing_Pkts = 0;
	m_port = 65419;
	homepath = getenv("HOME");
	folder="SDN";
}

VanetSim::~VanetSim()
{
	os.close();
}

void VanetSim::Simulate(int argc, char *argv[])
{
	SetDefault();
	ParseArguments(argc, argv);
	LoadTraffic();
	ConfigNode();
	ConfigChannels();
	ConfigDevices();
	ConfigMobility();
	ConfigApp();
	ConfigTracing();
	Run();
	ProcessOutputs();
}

void VanetSim::SetDefault()
{
	//Handle By Constructor
}

void VanetSim::ParseArguments(int argc, char *argv[])
{
	CommandLine cmd;
//	cmd.AddValue ("traceFile", "Ns2 movement trace file", traceFile);
//	cmd.AddValue ("nodeNum", "Number of nodes", nodeNum);
	cmd.AddValue ("duration", "Duration of Simulation", duration);
//	cmd.AddValue ("logFile", "Log file", logFile);
	cmd.AddValue ("folder", "Working Directory", folder);
	cmd.AddValue ("txp1", "TX power for SCH", txp1);
	cmd.AddValue ("txp2", "TX power for CCH", txp2);
	cmd.AddValue ("range1", "Range for SCH", range1);
	cmd.AddValue ("range2", "Range for CCH", range2);
	cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
	cmd.AddValue ("mod", "0=olsr(DEFAULT) 1=sdn", mod);
	cmd.AddValue ("pmod", "0=Range(DEFAULT) 1=Other", pmod);
	cmd.Parse (argc,argv);

	// Fix non-unicast data rate to be the same as that of unicast
	Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
	                      StringValue (phyMode));

}

void VanetSim::LoadTraffic()
{
	if (mod==0)
	{
		std::cout<<"Mode: OLSR-N"<<std::endl;
	}
	else
	{
		std::cout<<"Mode: SDN"<<std::endl;
	}
	DIR* dir = NULL;
	//DIR* subdir=NULL;
	std::string temp(homepath+"/"+folder);
	if((dir = opendir(temp.data()))==NULL)
		NS_FATAL_ERROR("Cannot open input path "<<temp.data()<<", Aborted.");



	std::string sumo_net = temp + "/input.net.xml";
	std::string sumo_route = temp + "/input.rou.xml";
	std::string sumo_fcd = temp + "/input.fcd.xml";

	std::string output = temp + "/result.txt";

	os.open(output.data(),std::ios::out);

	ns3::vanetmobility::VANETmobilityHelper mobilityHelper;
	VMo=mobilityHelper.GetSumoMObility(sumo_net,sumo_route,sumo_fcd);

	nodeNum = VMo->GetNodeSize();
}



void VanetSim::ConfigNode()
{
	m_nodes.Create(nodeNum+3);//Cars + Controller + Source + Sink
	/*Only Apps Are Different Between Different kind of Nodes*/
	// Name nodes
	for (uint32_t i = 0; i < nodeNum; ++i)
	{
		std::ostringstream os;
		os << "vehicle-" << i;
		Names::Add(os.str(), m_nodes.Get(i));
	}
	Names::Add("Controller",m_nodes.Get(nodeNum));
	Names::Add("Source",m_nodes.Get(nodeNum+1));
	Names::Add("Sink",m_nodes.Get(nodeNum+2));
}

void VanetSim::ConfigChannels()
{
	//===channel
	YansWifiChannelHelper SCHChannel;
	SCHChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	if (pmod == 1)
	{
		SCHChannel.AddPropagationLoss(lossModle,"Frequency", DoubleValue(freq1));
	}
	else
	{
		SCHChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange",
				DoubleValue(range1));
	}
	YansWifiChannelHelper CCHChannel;
	CCHChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	if (pmod ==1)
	{
		CCHChannel.AddPropagationLoss(lossModle,"Frequency", DoubleValue(freq2));
	}
	else
	{
		CCHChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange",
				DoubleValue(range2));
	}



	// the channelg
	Ptr<YansWifiChannel> SCH = SCHChannel.Create();
	Ptr<YansWifiChannel> CCH = CCHChannel.Create();

	//===wifiphy
	YansWifiPhyHelper SCHPhy =  YansWifiPhyHelper::Default ();
	SCHPhy.SetChannel (SCH);
	SCHPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
	YansWifiPhyHelper CCHPhy =  YansWifiPhyHelper::Default ();
	CCHPhy.SetChannel (CCH);
	CCHPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

	// 802.11p mac
	NqosWaveMacHelper SCH80211pMac = NqosWaveMacHelper::Default ();
	Wifi80211pHelper SCH80211p = Wifi80211pHelper::Default ();
	NqosWaveMacHelper CCH80211pMac = NqosWaveMacHelper::Default ();
	Wifi80211pHelper CCH80211p = Wifi80211pHelper::Default ();

	SCH80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
										"DataMode",StringValue (phyMode),
										"ControlMode",StringValue (phyMode));
	CCH80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
											"DataMode",StringValue (phyMode),
											"ControlMode",StringValue (phyMode));

	// Set Tx Power For The SCH
	SCHPhy.Set ("TxPowerStart",DoubleValue (txp1));
	SCHPhy.Set ("TxPowerEnd", DoubleValue (txp1));
	m_SCHDevices = SCH80211p.Install(SCHPhy, SCH80211pMac, m_nodes);

	// CCH
	CCHPhy.Set ("TxPowerStart",DoubleValue (txp2));
	CCHPhy.Set ("TxPowerEnd", DoubleValue (txp2));
	m_CCHDevices = CCH80211p.Install(CCHPhy, CCH80211pMac, m_nodes);

}

void VanetSim::ConfigDevices()
{
	//Done in ConfigChannels()
}

void VanetSim::ConfigMobility()
{
/*	Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);
	ns2.Install (m_nodes.Begin(),m_nodes.End()-3);
	// configure movements for Car node, while reading trace file
	Ptr<MobilityModel> Temp = m_nodes.Get(nodeNum);//Controller
	Temp->SetPosition(Vector(0.0, 0.0, 0.0));
	Temp = m_nodes.Get(nodeNum+1);//source
	Temp->SetPosition(Vector(5.1, 0.0, 0.0));
	Temp = m_nodes.Get(nodeNum+2);//Sink
*/
	VMo->Install();
	double rt = VMo->GetReadTotalTime();
	if (duration<0)
	{
		duration = rt;
	}
	Ptr<MobilityModel> Temp = m_nodes.Get(nodeNum)->GetObject<MobilityModel>();//Controller
	Temp->SetPosition(Vector(0.0, 0.0, 0.0));
	Temp = m_nodes.Get(nodeNum+1)->GetObject<MobilityModel>();//source
	Temp->SetPosition(Vector(5.1, 0.0, 0.0));
	Temp = m_nodes.Get(nodeNum+2)->GetObject<MobilityModel>();//Sink
	Temp->SetPosition(Vector(1000.0, 0.0, 0.0));
}

void VanetSim::ConfigApp()
{
	//===Routing
	InternetStackHelper internet;
	if (mod != 1)
	{
		OlsrHelper olsr;
		//Ipv4ListRoutingHelper list;
		//list.Add(olsr,100);
		internet.SetRoutingHelper(olsr);
		std::cout<<"OLSR"<<std::endl;
	}
	else
	{
		std::cout<<"SDN"<<std::endl;
	}
	internet.Install (m_nodes);

	std::cout<<"Now Internet Done"<<std::endl;

	//===IP ADDRESS
	Ipv4AddressHelper ipv4S;
	NS_LOG_INFO ("Assign IP Addresses.");
	ipv4S.SetBase ("10.1.1.0", "255.255.255.0");//SCH
	m_SCHInterface = ipv4S.Assign (m_SCHDevices);

	if (mod ==1)
	{
		Ipv4AddressHelper ipv4C;
		NS_LOG_INFO ("Assign IP-C Addresses.");
		ipv4C.SetBase("192.168.0.0","255.255.255.0");//CCH
		m_CCHInterface = ipv4C.Assign(m_CCHDevices);
	}


	//===Traffic
	//source

	//onoff
	Address remote (InetSocketAddress(m_SCHInterface.GetAddress(nodeNum+2), m_port));
	OnOffHelper Source("ns3::UdpSocketFactory",remote);//SendToSink
	Source.SetAttribute("OffTime",StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

	m_source = Source.Install(m_nodes.Get(nodeNum+1));//Insatll on Source
	m_source.Stop(Seconds(duration));//Default Start time is 0.

	/*
	TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
	source = Socket::CreateSocket (m_nodes.Get(nodeNum+1), tid);
	Simulator::Schedule(Seconds(0.0), &VanetSim::SendDataPacket, this);
	*/

	//sink
	TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
	Ptr<Socket> sink = Socket::CreateSocket (m_nodes.Get(nodeNum+2), tid);//The Sink
	InetSocketAddress local = InetSocketAddress(m_SCHInterface.GetAddress(nodeNum+2),m_port);
	sink->Bind(local);
	sink->SetRecvCallback(MakeCallback(&VanetSim::ReceiveDataPacket, this));
}

void VanetSim::ReceiveDataPacket(Ptr<Socket> socket)
{
	Ptr<Packet> packet;
	while ((packet = socket->Recv()))
	{
		Rx_Data_Bytes += packet->GetSize();
		Rx_Data_Pkts++;
		std::cout<<".";
	}
}

void VanetSim::SendDataPacket()
{
	/*Ptr<Packet> packet = Create<Packet> (packetSize);
	source->SendTo(packet, 0, )
	Simulator::Schedule(Seconds(interval), &VanetSim::SendDataPacket, this);*/
	//TODO
}

void VanetSim::ConfigTracing()
{
	//TODO
}

void VanetSim::ProcessOutputs()
{
	Ptr<OnOffApplication> app = DynamicCast<OnOffApplication>(m_source.Get(0));
	//Tx_Data_Pkts = app->Tx_packets;
	std::cout<<Tx_Data_Pkts<<std::endl;
	std::cout<<Rx_Data_Pkts<<std::endl;
}

void VanetSim::Run()
{
	Simulator::Schedule(Seconds(0.0), &VanetSim::Look_at_clock, this);
	std::cout << "Starting simulation for " << duration << " s ..."<< std::endl;
	Simulator::Stop(Seconds(duration));
	Simulator::Run();
	Simulator::Destroy();

}

void VanetSim::Look_at_clock()
{
	std::cout<<"Now:"<<Simulator::Now().GetSeconds()<<std::endl;
	os<<"Now:"<<Simulator::Now().GetSeconds()<<std::endl;
	Ptr<OutputStreamWrapper> osw = Create<OutputStreamWrapper> (&std::cout);
	m_nodes.Get(nodeNum+1)->GetObject<Ipv4>()->GetRoutingProtocol()->PrintRoutingTable(osw);
	Ptr<OutputStreamWrapper> osw2 = Create<OutputStreamWrapper> (&os);
	m_nodes.Get(nodeNum+1)->GetObject<Ipv4>()->GetRoutingProtocol()->PrintRoutingTable(osw2);
	/*2  Ptr<MobilityModel> Temp;
	Vector vt;
	for (int i = 0;i<=nodeNum+2;++i)
	{
		Temp = m_nodes.Get(i)->GetObject<MobilityModel>();
		vt = Temp->GetPosition();
		std::cout<<i<<":"<<vt.x<<","<<vt.y<<","<<vt.z<<";"<<std::flush;
	}
	std::cout<<std::endl;*/
	ProcessOutputs();

	Simulator::Schedule(Seconds(1.0), &VanetSim::Look_at_clock, this);
}

// Example to use ns2 traces file in ns3
int main (int argc, char *argv[])
{
	VanetSim SDN_test;
	SDN_test.Simulate(argc, argv);
	return 0;
}



