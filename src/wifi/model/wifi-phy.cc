/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
 * Copyright (c) 2015,2016 IMDEA Networks Institute
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
 * Authors: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 *          Sébastien Deronne <sebastien.deronne@gmail.com>
 *          Hany Assasa <hany.assasa@gmail.com>
 */

#include "wifi-phy.h"
#include "wifi-mode.h"
#include "wifi-channel.h"
#include "wifi-preamble.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/trace-source-accessor.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("WifiPhy");

/****************************************************************
 *       This destructor is needed.
 ****************************************************************/

WifiPhyListener::~WifiPhyListener ()
{
}

/****************************************************************
 *       The actual WifiPhy class
 ****************************************************************/

NS_OBJECT_ENSURE_REGISTERED (WifiPhy);

TypeId
WifiPhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiPhy")
    .SetParent<Object> ()
    .SetGroupName ("Wifi")
    .AddTraceSource ("PhyTxBegin",
                     "Trace source indicating a packet "
                     "has begun transmitting over the channel medium",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyTxBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxEnd",
                     "Trace source indicating a packet "
                     "has been completely transmitted over the channel. "
                     "NOTE: the only official WifiPhy implementation "
                     "available to this date (YansWifiPhy) never fires "
                     "this trace source.",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyTxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxDrop",
                     "Trace source indicating a packet "
                     "has been dropped by the device during transmission",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyTxDropTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyRxBegin",
                     "Trace source indicating a packet "
                     "has begun being received from the channel medium "
                     "by the device",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyRxBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyRxEnd",
                     "Trace source indicating a packet "
                     "has been completely received from the channel medium "
                     "by the device",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyRxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyRxDrop",
                     "Trace source indicating a packet "
                     "has been dropped by the device during reception",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyRxDropTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MonitorSnifferRx",
                     "Trace source simulating a wifi device in monitor mode "
                     "sniffing all received frames",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyMonitorSniffRxTrace),
                     "ns3::WifiPhy::MonitorSnifferRxTracedCallback")
    .AddTraceSource ("MonitorSnifferTx",
                     "Trace source simulating the capability of a wifi device "
                     "in monitor mode to sniff all frames being transmitted",
                     MakeTraceSourceAccessor (&WifiPhy::m_phyMonitorSniffTxTrace),
                     "ns3::WifiPhy::MonitorSnifferTxTracedCallback")
  ;
  return tid;
}

WifiPhy::WifiPhy ()
{
  NS_LOG_FUNCTION (this);
  m_totalAmpduSize = 0;
  m_totalAmpduNumSymbols = 0;
  m_totalBits = 0;
  m_txDuration = NanoSeconds(0.0);
}

WifiPhy::~WifiPhy ()
{
  NS_LOG_FUNCTION (this);
}

WifiMode
WifiPhy::GetHtPlcpHeaderMode (WifiMode payloadMode)
{
  return WifiPhy::GetHtMcs0 ();
}

WifiMode
WifiPhy::GetVhtPlcpHeaderMode (WifiMode payloadMode)
{
  return WifiPhy::GetVhtMcs0 ();
}

Time
WifiPhy::GetPlcpHtTrainingSymbolDuration (WifiPreamble preamble, WifiTxVector txVector)
{
  uint8_t Ndltf, Neltf;
  //We suppose here that STBC = 0.
  //If STBC > 0, we need a different mapping between Nss and Nltf (IEEE 802.11n-2012 standard, page 1682).
  if (txVector.GetNss () < 3)
    {
      Ndltf = txVector.GetNss ();
    }
  else if (txVector.GetNss () < 5)
    {
      Ndltf = 4;
    }
  else if (txVector.GetNss () < 7)
    {
      Ndltf = 6;
    }
  else
    {
      Ndltf = 8;
    }

  if (txVector.GetNess () < 3)
    {
      Neltf = txVector.GetNess ();
    }
  else
    {
      Neltf = 4;
    }

  switch (preamble)
    {
    case WIFI_PREAMBLE_HT_MF:
      return MicroSeconds (4 + (4 * Ndltf) + (4 * Neltf));
    case WIFI_PREAMBLE_HT_GF:
      return MicroSeconds ((4 * Ndltf) + (4 * Neltf));
    case WIFI_PREAMBLE_VHT:
      return MicroSeconds (4 + (4 * Ndltf));
    default:
      //no training for non HT
      return MicroSeconds (0);
    }
}

Time
WifiPhy::GetPlcpHtSigHeaderDuration (WifiPreamble preamble)
{
  switch (preamble)
    {
    case WIFI_PREAMBLE_HT_MF:
    case WIFI_PREAMBLE_HT_GF:
      //HT-SIG
      return MicroSeconds (8);
    default:
      //no HT-SIG for non HT
      return MicroSeconds (0);
    }
}

Time
WifiPhy::GetPlcpVhtSigA1Duration (WifiPreamble preamble)
{
  switch (preamble)
    {
    case WIFI_PREAMBLE_VHT:
      //VHT-SIG-A1
      return MicroSeconds (4);
    default:
      // no VHT-SIG-A1 for non VHT
      return MicroSeconds (0);
    }
}

Time
WifiPhy::GetPlcpVhtSigA2Duration (WifiPreamble preamble)
{
  switch (preamble)
    {
    case WIFI_PREAMBLE_VHT:
      //VHT-SIG-A2
      return MicroSeconds (4);
    default:
      // no VHT-SIG-A2 for non VHT
      return MicroSeconds (0);
    }
}

Time
WifiPhy::GetPlcpVhtSigBDuration (WifiPreamble preamble)
{
  switch (preamble)
    {
    case WIFI_PREAMBLE_VHT:
      //VHT-SIG-B
      return MicroSeconds (4);
    default:
      // no VHT-SIG-B for non VHT
      return MicroSeconds (0);
    }
}

WifiMode
WifiPhy::GetPlcpHeaderMode (WifiMode payloadMode, WifiPreamble preamble, WifiTxVector txVector)
{
  switch (payloadMode.GetModulationClass ())
    {
    case WIFI_MOD_CLASS_OFDM:
    case WIFI_MOD_CLASS_HT:
    case WIFI_MOD_CLASS_VHT:
      switch (txVector.GetChannelWidth ())
        {
        case 5:
          return WifiPhy::GetOfdmRate1_5MbpsBW5MHz ();
        case 10:
          return WifiPhy::GetOfdmRate3MbpsBW10MHz ();
        case 20:
        case 40:
        case 80:
        case 160:
        default:
          //(Section 18.3.2 "PLCP frame format"; IEEE Std 802.11-2012)
          //actually this is only the first part of the PlcpHeader,
          //because the last 16 bits of the PlcpHeader are using the
          //same mode of the payload
          return WifiPhy::GetOfdmRate6Mbps ();
        }
    case WIFI_MOD_CLASS_ERP_OFDM:
      return WifiPhy::GetErpOfdmRate6Mbps ();
    case WIFI_MOD_CLASS_DSSS:
    case WIFI_MOD_CLASS_HR_DSSS:
      if (preamble == WIFI_PREAMBLE_LONG || payloadMode == WifiPhy::GetDsssRate1Mbps ())
        {
          //(Section 16.2.3 "PLCP field definitions" and Section 17.2.2.2 "Long PPDU format"; IEEE Std 802.11-2012)
          return WifiPhy::GetDsssRate1Mbps ();
        }
      else //WIFI_PREAMBLE_SHORT
        {
          //(Section 17.2.2.3 "Short PPDU format"; IEEE Std 802.11-2012)
          return WifiPhy::GetDsssRate2Mbps ();
        }

    case WIFI_MOD_CLASS_DMG_CTRL:
      return WifiPhy::GetDMG_MCS0 ();

    case WIFI_MOD_CLASS_DMG_SC:
      return WifiPhy::GetDMG_MCS1 ();

    case WIFI_MOD_CLASS_DMG_OFDM:
      return WifiPhy::GetDMG_MCS13 ();

    default:
      NS_FATAL_ERROR ("unsupported modulation class");
      return WifiMode ();
    }
}

Time
WifiPhy::GetPlcpHeaderDuration (WifiTxVector txVector, WifiPreamble preamble)
{
  if (preamble == WIFI_PREAMBLE_NONE)
    {
      return MicroSeconds (0);
    }
  switch (txVector.GetMode ().GetModulationClass ())
    {
    case WIFI_MOD_CLASS_OFDM:
      {
        switch (txVector.GetChannelWidth ())
          {
          case 20:
          default:
            //(Section 18.3.3 "PLCP preamble (SYNC))" and Figure 18-4 "OFDM training structure"; IEEE Std 802.11-2012)
            //also (Section 18.3.2.4 "Timing related parameters" Table 18-5 "Timing-related parameters"; IEEE Std 802.11-2012)
            //We return the duration of the SIGNAL field only, since the
            //SERVICE field (which strictly speaking belongs to the PLCP
            //header, see Section 18.3.2 and Figure 18-1) is sent using the
            //payload mode.
            return MicroSeconds (4);
          case 10:
            //(Section 18.3.2.4 "Timing related parameters" Table 18-5 "Timing-related parameters"; IEEE Std 802.11-2012)
            return MicroSeconds (8);
          case 5:
            //(Section 18.3.2.4 "Timing related parameters" Table 18-5 "Timing-related parameters"; IEEE Std 802.11-2012)
            return MicroSeconds (16);
          }
      }
    case WIFI_MOD_CLASS_HT:
      {
        //L-SIG
        //IEEE 802.11n Figure 20.1
        switch (preamble)
          {
          case WIFI_PREAMBLE_HT_MF:
          default:
            return MicroSeconds (4);
          case WIFI_PREAMBLE_HT_GF:
            return MicroSeconds (0);
          }
      }
    case WIFI_MOD_CLASS_VHT:
    case WIFI_MOD_CLASS_ERP_OFDM:
      return MicroSeconds (4);
    case WIFI_MOD_CLASS_DSSS:
    case WIFI_MOD_CLASS_HR_DSSS:
      if ((preamble == WIFI_PREAMBLE_SHORT) && (txVector.GetMode ().GetDataRate (22, 0, 1) > 1000000))
        {
          //(Section 17.2.2.3 "Short PPDU format" and Figure 17-2 "Short PPDU format"; IEEE Std 802.11-2012)
          return MicroSeconds (24);
        }
      else //WIFI_PREAMBLE_LONG
        {
          //(Section 17.2.2.2 "Long PPDU format" and Figure 17-1 "Short PPDU format"; IEEE Std 802.11-2012)
          return MicroSeconds (48);
        }
    case WIFI_MOD_CLASS_DMG_CTRL:
      /* From Annex L (L.5.2.5) */
      return NanoSeconds(4654);
    case WIFI_MOD_CLASS_DMG_SC:
    case WIFI_MOD_CLASS_DMG_LP_SC:
      /* From Table 21-4 in 802.11ad spec 21.3.4 */
      return NanoSeconds(582);
    case WIFI_MOD_CLASS_DMG_OFDM:
      /* From Table 21-4 in 802.11ad spec 21.3.4 */
      return NanoSeconds(242);
    default:
      NS_FATAL_ERROR ("unsupported modulation class");
      return MicroSeconds (0);
    }
}

Time
WifiPhy::GetPlcpPreambleDuration (WifiTxVector txVector, WifiPreamble preamble)
{
  if (preamble == WIFI_PREAMBLE_NONE)
    {
      return MicroSeconds (0);
    }
  switch (txVector.GetMode ().GetModulationClass ())
    {
    case WIFI_MOD_CLASS_OFDM:
      {
        switch (txVector.GetChannelWidth ())
          {
          case 20000000:
          default:
            //(Section 18.3.3 "PLCP preamble (SYNC))" Figure 18-4 "OFDM training structure"
            //also Section 18.3.2.3 "Modulation-dependent parameters" Table 18-4 "Modulation-dependent parameters"; IEEE Std 802.11-2012)
            return MicroSeconds (16);
          case 10000000:
            //(Section 18.3.3 "PLCP preamble (SYNC))" Figure 18-4 "OFDM training structure"
            //also Section 18.3.2.3 "Modulation-dependent parameters" Table 18-4 "Modulation-dependent parameters"; IEEE Std 802.11-2012)
            return MicroSeconds (32);
          case 5000000:
            //(Section 18.3.3 "PLCP preamble (SYNC))" Figure 18-4 "OFDM training structure"
            //also Section 18.3.2.3 "Modulation-dependent parameters" Table 18-4 "Modulation-dependent parameters"; IEEE Std 802.11-2012)
            return MicroSeconds (64);
          }
      }
    case WIFI_MOD_CLASS_VHT:
    case WIFI_MOD_CLASS_HT:
      //IEEE 802.11n Figure 20.1 the training symbols before L_SIG or HT_SIG
      return MicroSeconds (16);
    case WIFI_MOD_CLASS_ERP_OFDM:
      return MicroSeconds (16);
    case WIFI_MOD_CLASS_DSSS:
    case WIFI_MOD_CLASS_HR_DSSS:
      if ((preamble == WIFI_PREAMBLE_SHORT) && (txVector.GetMode ().GetDataRate (22, 0, 1) > 1000000))
        {
          //(Section 17.2.2.3 "Short PPDU format)" Figure 17-2 "Short PPDU format"; IEEE Std 802.11-2012)
          return MicroSeconds (72);
        }
      else //WIFI_PREAMBLE_LONG
        {
          //(Section 17.2.2.2 "Long PPDU format)" Figure 17-1 "Long PPDU format"; IEEE Std 802.11-2012)
          return MicroSeconds (144);
        }
		
    case WIFI_MOD_CLASS_DMG_CTRL:
      // CTRL Preamble = (6400 + 1152) Samples * Tc (Chip Time for SC), Tc = Tccp = 0.57ns.
      // CTRL Preamble = 4.291 micro seconds.
      return NanoSeconds(4291);

    case WIFI_MOD_CLASS_DMG_SC:
    case WIFI_MOD_CLASS_DMG_LP_SC:
      // SC Preamble = 3328 Samples * Tc (Chip Time for SC), Tc = 0.57ns.
      // SC Preamble = 1.89 micro seconds.
      return NanoSeconds(1891);

    case WIFI_MOD_CLASS_DMG_OFDM:
      // OFDM Preamble = 4992 Samples * Ts (Chip Time for OFDM), Tc = 0.38ns.
      // OFDM Preamble = 1.89 micro seconds.
      return NanoSeconds(1891);
		
    default:
      NS_FATAL_ERROR ("unsupported modulation class");
      return MicroSeconds (0);
    }
}

Time
WifiPhy::GetPayloadDuration (uint32_t size, WifiTxVector txVector, WifiPreamble preamble, double frequency)
{
  return GetPayloadDuration (size, txVector, preamble, frequency, NORMAL_MPDU, 0);
}

Time
WifiPhy::GetPayloadDuration (uint32_t size, WifiTxVector txVector, WifiPreamble preamble, double frequency, enum mpduType mpdutype, uint8_t incFlag)
{
  WifiMode payloadMode = txVector.GetMode ();
  NS_LOG_FUNCTION (size << payloadMode);

  switch (payloadMode.GetModulationClass ())
    {
    case WIFI_MOD_CLASS_OFDM:
    case WIFI_MOD_CLASS_ERP_OFDM:
      {
        //(Section 18.3.2.4 "Timing related parameters" Table 18-5 "Timing-related parameters"; IEEE Std 802.11-2012
        //corresponds to T_{SYM} in the table)
        Time symbolDuration;

        switch (txVector.GetChannelWidth ())
          {
          case 20:
          default:
            symbolDuration = MicroSeconds (4);
            break;
          case 10:
            symbolDuration = MicroSeconds (8);
            break;
          case 5:
            symbolDuration = MicroSeconds (16);
            break;
          }

        //(Section 18.3.2.3 "Modulation-dependent parameters" Table 18-4 "Modulation-dependent parameters"; IEEE Std 802.11-2012)
        //corresponds to N_{DBPS} in the table
        double numDataBitsPerSymbol = payloadMode.GetDataRate (txVector.GetChannelWidth (), 0, 1) * symbolDuration.GetNanoSeconds () / 1e9;
        double numSymbols;

        if (mpdutype == MPDU_IN_AGGREGATE && preamble != WIFI_PREAMBLE_NONE)
          {
            //First packet in an A-MPDU
            numSymbols = ((16 + size * 8.0 + 6) / numDataBitsPerSymbol);
            if (incFlag == 1)
              {
                m_totalAmpduSize += size;
                m_totalAmpduNumSymbols += numSymbols;
              }
          }
        else if (mpdutype == MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE)
          {
            //consecutive packets in an A-MPDU
            numSymbols = ((size * 8.0) / numDataBitsPerSymbol);
            if (incFlag == 1)
              {
                m_totalAmpduSize += size;
                m_totalAmpduNumSymbols += numSymbols;
              }
          }
        else if (mpdutype == LAST_MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE)
          {
            //last packet in an A-MPDU
            uint32_t totalAmpduSize = m_totalAmpduSize + size;
            numSymbols = lrint (ceil ((16 + totalAmpduSize * 8.0 + 6) / numDataBitsPerSymbol));
            NS_ASSERT (m_totalAmpduNumSymbols <= numSymbols);
            numSymbols -= m_totalAmpduNumSymbols;
            if (incFlag == 1)
              {
                m_totalAmpduSize = 0;
                m_totalAmpduNumSymbols = 0;
              }
          }
        else if (mpdutype == NORMAL_MPDU && preamble != WIFI_PREAMBLE_NONE)
          {
            //Not an A-MPDU
            numSymbols = lrint (ceil ((16 + size * 8.0 + 6.0) / numDataBitsPerSymbol));
          }
        else
          {
            NS_FATAL_ERROR ("Wrong combination of preamble and packet type");
          }

        //Add signal extension for ERP PHY
        if (payloadMode.GetModulationClass () == WIFI_MOD_CLASS_ERP_OFDM)
          {
            return NanoSeconds (numSymbols * symbolDuration.GetNanoSeconds ()) + MicroSeconds (6);
          }
        else
          {
            return NanoSeconds (numSymbols * symbolDuration.GetNanoSeconds ());
          }
      }
    case WIFI_MOD_CLASS_HT:
    case WIFI_MOD_CLASS_VHT:
      {
        Time symbolDuration;
        double m_Stbc;
        //if short GI data rate is used then symbol duration is 3.6us else symbol duration is 4us
        //In the future has to create a stationmanager that only uses these data rates if sender and reciever support GI
        if (txVector.IsShortGuardInterval ())
          {
            symbolDuration = NanoSeconds (3600);
          }
        else
          {
            symbolDuration = MicroSeconds (4);
          }

        if (txVector.IsStbc ())
          {
            m_Stbc = 2;
          }
        else
          {
            m_Stbc = 1;
          }

        //check tables 20-35 and 20-36 in the .11n standard to get cases when nes = 2
        double Nes = 1;
        if (payloadMode.GetUniqueName () == "HtMcs21"
          || payloadMode.GetUniqueName () == "HtMcs22"
          || payloadMode.GetUniqueName () == "HtMcs23"
          || payloadMode.GetUniqueName () == "HtMcs28"
          || payloadMode.GetUniqueName () == "HtMcs29"
          || payloadMode.GetUniqueName () == "HtMcs30"
          || payloadMode.GetUniqueName () == "HtMcs31")
        {
          Nes = 2;
        }
        //check tables 22-30 to 22-61 in the .11ac standard to get cases when nes > 1
        //todo: improve logic to reduce the number of if cases
        //todo: extend to NSS > 4 for VHT rates
        if (txVector.GetChannelWidth () == 40
            && txVector.GetNss () == 3
            && payloadMode.GetMcsValue () >= 8)
          {
            Nes = 2;
          }
        if (txVector.GetChannelWidth () == 80
            && txVector.GetNss () == 2
            && payloadMode.GetMcsValue () >= 7)
          {
            Nes = 2;
          }
        if (txVector.GetChannelWidth () == 80
            && txVector.GetNss () == 3
            && payloadMode.GetMcsValue () >= 7)
          {
            Nes = 2;
          }
        if (txVector.GetChannelWidth () == 80
            && txVector.GetNss () == 3
           && payloadMode.GetMcsValue () == 9)
          {
           Nes = 3;
          }
        if (txVector.GetChannelWidth () == 80
            && txVector.GetNss () == 4
            && payloadMode.GetMcsValue () >= 4)
          {
            Nes = 2;
          }
        if (txVector.GetChannelWidth () == 80
            && txVector.GetNss () == 4
            && payloadMode.GetMcsValue () >= 7)
          {
            Nes = 3;
          }
        if (txVector.GetChannelWidth () == 160
            && (payloadMode.GetUniqueName () == "VhtMcs7" && payloadMode.GetMcsValue () >= 7))
          {
            Nes = 2;
          }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 2
              && payloadMode.GetMcsValue () >= 4)
            {
              Nes = 2;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 2
              && payloadMode.GetMcsValue () >= 7)
            {
              Nes = 3;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 3
             && payloadMode.GetMcsValue () >= 3)
            {
              Nes = 2;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 3
              && payloadMode.GetMcsValue () >= 5)
            {
              Nes = 3;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 3
              && payloadMode.GetMcsValue () >= 7)
            {
              Nes = 4;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 4
              && payloadMode.GetMcsValue () >= 2)
            {
              Nes = 2;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 4
              && payloadMode.GetMcsValue () >= 4)
            {
              Nes = 3;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 4
              && payloadMode.GetMcsValue () >= 5)
            {
              Nes = 4;
            }
          if (txVector.GetChannelWidth () == 160
              && txVector.GetNss () == 4
              && payloadMode.GetMcsValue () >= 7)
            {
              Nes = 6;
            }

        //IEEE Std 802.11n, section 20.3.11, equation (20-32)
        double numDataBitsPerSymbol = payloadMode.GetDataRate (txVector.GetChannelWidth (), txVector.IsShortGuardInterval (), txVector.GetNss ()) * symbolDuration.GetNanoSeconds () / 1e9;
        double numSymbols;

        if (mpdutype == MPDU_IN_AGGREGATE && preamble != WIFI_PREAMBLE_NONE)
          {
            //First packet in an A-MPDU
            numSymbols = (m_Stbc * (16 + size * 8.0 + 6 * Nes) / (m_Stbc * numDataBitsPerSymbol));
            if (incFlag == 1)
              {
                m_totalAmpduSize += size;
                m_totalAmpduNumSymbols += numSymbols;
              }
          }
        else if (mpdutype == MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE)
          {
            //consecutive packets in an A-MPDU
            numSymbols = (m_Stbc * size * 8.0) / (m_Stbc * numDataBitsPerSymbol);
            if (incFlag == 1)
              {
                m_totalAmpduSize += size;
                m_totalAmpduNumSymbols += numSymbols;
              }
          }
        else if (mpdutype == LAST_MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE)
          {
            //last packet in an A-MPDU
            uint32_t totalAmpduSize = m_totalAmpduSize + size;
            numSymbols = lrint (m_Stbc * ceil ((16 + totalAmpduSize * 8.0 + 6 * Nes) / (m_Stbc * numDataBitsPerSymbol)));
            NS_ASSERT (m_totalAmpduNumSymbols <= numSymbols);
            numSymbols -= m_totalAmpduNumSymbols;
            if (incFlag == 1)
              {
                m_totalAmpduSize = 0;
                m_totalAmpduNumSymbols = 0;
              }
          }
        else if (mpdutype == NORMAL_MPDU && preamble != WIFI_PREAMBLE_NONE)
          {
            //Not an A-MPDU
            numSymbols = lrint (m_Stbc * ceil ((16 + size * 8.0 + 6.0 * Nes) / (m_Stbc * numDataBitsPerSymbol)));
          }
        else
          {
            NS_FATAL_ERROR ("Wrong combination of preamble and packet type");
          }

        if (payloadMode.GetModulationClass () == WIFI_MOD_CLASS_HT && frequency >= 2400 && frequency <= 2500 && ((mpdutype == NORMAL_MPDU && preamble != WIFI_PREAMBLE_NONE) || (mpdutype == LAST_MPDU_IN_AGGREGATE && preamble == WIFI_PREAMBLE_NONE))) //at 2.4 GHz
          {
            return NanoSeconds (numSymbols * symbolDuration.GetNanoSeconds ()) + MicroSeconds (6);
          }
        else //at 5 GHz
          {
            return NanoSeconds (numSymbols * symbolDuration.GetNanoSeconds ());
          }
      }
    case WIFI_MOD_CLASS_DSSS:
    case WIFI_MOD_CLASS_HR_DSSS:
      //(Section 17.2.3.6 "Long PLCP LENGTH field"; IEEE Std 802.11-2012)
      NS_LOG_LOGIC (" size=" << size
                             << " mode=" << payloadMode
                             << " rate=" << payloadMode.GetDataRate (22, 0, 1));
      return MicroSeconds (lrint (ceil ((size * 8.0) / (payloadMode.GetDataRate (22, 0, 1) / 1.0e6))));

    case WIFI_MOD_CLASS_DMG_CTRL:
      {
        if (txVector.GetTrainngFieldLength () == 0)
          {
            uint32_t Ncw;                       /* Number of LDPC codewords. */
            uint32_t Ldpcw;                     /* Number of bits in the second and any subsequent codeword except the last. */
            uint32_t Ldplcw;                    /* Number of bits in the last codeword. */
            uint32_t DencodedSymmbols;          /* Number of differentailly encoded payload symbols. */
            uint32_t Chips;                     /* Number of chips (After spreading using Ga32 Golay Sequence). */
            uint32_t Nbits = (size - 8) * 8;    /* Number of bits in the payload part. */

            Ncw = 1 + (uint32_t) ceil ((double (size) - 6) * 8/168);
            Ldpcw = (uint32_t) ceil ((double (size) - 6) * 8/(Ncw - 1));
            Ldplcw = (size - 6) * 8 - (Ncw - 2) * Ldpcw;
            DencodedSymmbols = (672 - (504 - Ldpcw)) * (Ncw - 2) + (672 - (504 - Ldplcw));
            Chips = DencodedSymmbols * 32;

            /* Make sure the result is in nanoseconds. */
            double ret = double (Chips)/1.76;
            NS_LOG_DEBUG("bits " << Nbits << " Diff encoded Symmbols " << DencodedSymmbols << " rate " << payloadMode.GetDataRate() << " Payload Time " << ret << " ns");

            return NanoSeconds (ceil (ret));
          }
        else
          {
            uint32_t Ncw;                       /* Number of LDPC codewords. */
            Ncw = 1 + (uint32_t) ceil ((double (size) - 6) * 8/168);
            return NanoSeconds (ceil (double ((88 + (size - 6) * 8 + Ncw * 168) * 0.57 * 32)));
          }
      }

    case WIFI_MOD_CLASS_DMG_LP_SC:
      {
//        uint32_t Nbits = (size * 8);  /* Number of bits in the payload part. */
//        uint32_t Nrsc;                /* The total number of Reed Solomon codewords */
//        uint32_t Nrses;               /* The total number of Reed Solomon encoded symbols */
//        Nrsc = (uint32_t) ceil(Nbits/208);
//        Nrses = Nbits + Nrsc * 16;

//        uint32_t Nsbc;                 /* Short Block code Size */
//        if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_13_28)
//          Nsbc = 16;
//        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_13_21)
//          Nsbc = 12;
//        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_52_63)
//          Nsbc = 9;
//        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_13_14)
//          Nsbc = 8;
//        else
//          NS_FATAL_ERROR("unsupported code rate");

//        uint32_t Ncbps;               /* Ncbps = Number of coded bits per symbol. Check Table 21-21 for different constellations. */
//        if (payloadMode.GetConstellationSize() == 2)
//          Ncbps = 336;
//        else if (payloadMode.GetConstellationSize() == 4)
//          Ncbps = 2 * 336;
//          NS_FATAL_ERROR("unsupported constellation size");

//        uint32_t Neb;                 /* Total number of encoded bits */
//        uint32_t Nblks;               /* Total number of 512 blocks containing 392 data symbols */
//        Neb = Nsbc * Nrses;
//        Nblks = (uint32_t) ceil(neb/());
        return NanoSeconds (0);
      }

    case WIFI_MOD_CLASS_DMG_SC:
      {
        /* 21.3.4 Timeing Related Parameters, Table 21-4 TData = (Nblks * 512 + 64) * Tc. */
        /* 21.6.3.2.3.3 (4), Compute Nblks = The number of symbol blocks. */

        uint32_t Ncbpb; // Ncbpb = Number of coded bits per symbol block. Check Table 21-20 for different constellations.
        if (payloadMode.GetConstellationSize () == 2)
          Ncbpb = 448;
        else if (payloadMode.GetConstellationSize () == 4)
          Ncbpb = 2 * 448;
        else if (payloadMode.GetConstellationSize () == 16)
          Ncbpb = 4 * 448;
        else if (payloadMode.GetConstellationSize () == 64)
          Ncbpb = 6 * 448;
        else if (payloadMode.GetConstellationSize () == 256)
          Ncbpb = 8 * 448;
        else
          NS_FATAL_ERROR("unsupported constellation size");

        uint32_t Nbits = (size * 8); /* Nbits = Number of bits in the payload part. */
        uint32_t Ncbits;             /* Ncbits = Number of coded bits in the payload part. */

        if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_1_4)
          Ncbits = Nbits * 4;
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_1_2)
          Ncbits = Nbits * 2;
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_13_16)
          Ncbits = (uint32_t) ceil (double (Nbits) * 16.0 / 13);
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_3_4)
          Ncbits = (uint32_t) ceil (double (Nbits) * 4.0 / 3);
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_5_8)
          Ncbits = (uint32_t) ceil (double (Nbits) * 8.0 / 5);
        else
          NS_FATAL_ERROR("unsupported code rate");

        /* We have Lcw = 672 which is the LDPC codeword length. */
        uint32_t Ncw = (uint32_t) ceil (double (Ncbits) / 672.0);         /* Ncw = The number of LDPC codewords.  */
        uint32_t Nblks = (uint32_t) ceil (double (Ncw) * 672.0 / Ncbpb);  /* Nblks = The number of symbol blocks. */

        /* Make sure the result is in nanoseconds. */
        uint32_t tData; /* The duration of the data part */
        tData = lrint (ceil ((double (Nblks) * 512 + 64) / 1.76));
        NS_LOG_DEBUG ("bits " << Nbits << " cbits " << Ncbits << " rate " << payloadMode.GetDataRate() << " Payload Time " << tData << " ns");

        if (txVector.GetTrainngFieldLength () != 0)
          {
            if (tData < OFDMSCMin)
              tData = OFDMSCMin;
          }
        return NanoSeconds (tData);
      }

    case WIFI_MOD_CLASS_DMG_OFDM:
      {
        /* 21.3.4 Timeing Related Parameters, Table 21-4 TData = Nsym * Tsys(OFDM) */
        /* 21.5.3.2.3.3 (5), Compute Nsym = Number of OFDM Symbols */

        uint32_t Ncbps; // Ncbpb = Number of coded bits per symbol. Check Table 21-20 for different constellations.
        if (payloadMode.GetConstellationSize () == 2)
          Ncbps = 336;
        else if (payloadMode.GetConstellationSize () == 4)
          Ncbps = 2 * 336;
        else if (payloadMode.GetConstellationSize () == 16)
          Ncbps = 4 * 336;
        else if (payloadMode.GetConstellationSize () == 64)
          Ncbps = 6 * 336;
        else
          NS_FATAL_ERROR("unsupported constellation size");

        uint32_t Nbits = (size * 8); /* Nbits = Number of bits in the payload part. */
        uint32_t Ncbits;             /* Ncbits = Number of coded bits in the payload part. */

        if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_1_4)
          Ncbits = Nbits * 4;
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_1_2)
          Ncbits = Nbits * 2;
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_13_16)
          Ncbits = (uint32_t) ceil (double (Nbits) * 16.0 / 13);
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_3_4)
          Ncbits = (uint32_t) ceil (double (Nbits) * 4.0 / 3);
        else if (payloadMode.GetCodeRate () == WIFI_CODE_RATE_5_8)
          Ncbits = (uint32_t) ceil (double (Nbits) * 8.0 / 5);
        else
          NS_FATAL_ERROR ("unsupported code rate");

        uint32_t Ncw = (uint32_t) ceil (double (Ncbits) / 672.0);         /* Ncw = The number of LDPC codewords.  */
        uint32_t Nsym = (uint32_t) ceil (double (Ncw * 672.0) / Ncbps);   /* Nsym = Number of OFDM Symbols. */

        /* Make sure the result is in nanoseconds */
        uint32_t tData;       /* The duration of the data part */
        tData = Nsym * 242;   /* Tsys(OFDM) = 242ns */
        NS_LOG_DEBUG ("bits " << Nbits << " cbits " << Ncbits << " rate " << payloadMode.GetDataRate() << " Payload Time " << tData << " ns");

        if (txVector.GetTrainngFieldLength () != 0)
          {
            if (tData < OFDMBRPMin)
              tData = OFDMBRPMin;
          }
        return NanoSeconds (tData);
      }

    default:
      NS_FATAL_ERROR ("unsupported modulation class");
      return MicroSeconds (0);
  }
}

uint64_t
WifiPhy::CaluclateTransmittedBits (uint32_t size, WifiTxVector txvector)
{
  WifiMode payloadMode = txvector.GetMode();
  NS_LOG_FUNCTION (size << payloadMode);

  switch (payloadMode.GetModulationClass ())
    {
    case WIFI_MOD_CLASS_DMG_SC:
      {
        uint64_t bits;
        /* 21.3.4 Timeing Related Parameters, Table 21-4 TData = (Nblks * 512 + 64) * Tc. */
        /* 21.6.3.2.3.3 (4), Compute Nblks = The number of symbol blocks. */

        uint32_t Ncbpb; // Ncbpb = Number of coded bits per symbol block. Check Table 21-20 for different constellations.
        if (payloadMode.GetConstellationSize() == 2)
          Ncbpb = 448;
        else if (payloadMode.GetConstellationSize() == 4)
          Ncbpb = 2 * 448;
        else if (payloadMode.GetConstellationSize() == 16)
          Ncbpb = 4 * 448;
        else if (payloadMode.GetConstellationSize() == 64)
          Ncbpb = 6 * 448;
        else if (payloadMode.GetConstellationSize() == 256)
          Ncbpb = 8 * 448;
        else
          NS_FATAL_ERROR("unsupported constellation size");

        uint32_t Nbits = (size * 8); /* Nbits = Number of bits in the payload part. */
        uint32_t Ncbits;             /* Ncbits = Number of coded bits in the payload part. */

        if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_1_4)
          Ncbits = Nbits * 4;
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_1_2)
          Ncbits = Nbits * 2;
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_13_16)
          Ncbits = (uint32_t) ceil(Nbits * 16.0 / 13);
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_3_4)
          Ncbits = (uint32_t) ceil(Nbits * 4.0 / 3);
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_5_8)
          Ncbits = (uint32_t) ceil(Nbits * 8.0 / 5);
        else
          NS_FATAL_ERROR("unsupported code rate");

        /* We have Lcw = 672 which is the LDPC codeword length. */
        uint32_t Ncw = (uint32_t) ceil(Ncbits / 672.0);         /* Ncw = The number of LDPC codewords.  */
        uint32_t Nblks = (uint32_t) ceil(Ncw * 672.0 / Ncbpb);  /* Nblks = The number of symbol blocks. */

        bits = 3328 + 1024         /* Preamble + Header */
               + (Nblks + 1) * 64  /* Guard Interval Bits */
               + (Nblks  * Ncbpb); /* Number of Bits Per Block */
        return bits;
      }

    case WIFI_MOD_CLASS_DMG_OFDM:
      {
        uint32_t Ncbps; // Ncbpb = Number of coded bits per symbol. Check Table 21-20 for different constellations.
        if (payloadMode.GetConstellationSize() == 2)
          Ncbps = 336;
        else if (payloadMode.GetConstellationSize() == 4)
          Ncbps = 2 * 336;
        else if (payloadMode.GetConstellationSize() == 16)
          Ncbps = 4 * 336;
        else if (payloadMode.GetConstellationSize() == 64)
          Ncbps = 6 * 336;
        else
          NS_FATAL_ERROR("unsupported constellation size");

        uint32_t Nbits = (size * 8); /* Nbits = Number of bits in the payload part. */
        uint32_t Ncbits;             /* Ncbits = Number of coded bits in the payload part. */

        if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_1_4)
          Ncbits = Nbits * 4;
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_1_2)
          Ncbits = Nbits * 2;
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_13_16)
          Ncbits = (uint32_t) ceil(Nbits * 16.0 / 13);
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_3_4)
          Ncbits = (uint32_t) ceil(Nbits * 4.0 / 3);
        else if (payloadMode.GetCodeRate() == WIFI_CODE_RATE_5_8)
          Ncbits = (uint32_t) ceil(Nbits * 8.0 / 5);
        else
          NS_FATAL_ERROR("unsupported code rate");

        uint32_t Ncw = (uint32_t) ceil(Ncbits / 672.0);         /* Ncw = The number of LDPC codewords.  */
        uint32_t Nsym = (uint32_t) ceil(Ncw * 672.0 / Ncbps);   /* Nsym = Number of OFDM Symbols. */

        return 3328 + 672 + Nsym * Ncbps; /* Preamble + Header + Payload */
      }

    default:
      NS_FATAL_ERROR ("unsupported modulation class");
      return 0;
    }
}

uint64_t
WifiPhy::GetTotalTransmittedBits () const
{
  return m_totalBits;
}

Time
WifiPhy::GetTxDuration (void) const
{
  return m_txDuration;
}

Time
WifiPhy::GetLastRxDuration (void) const
{
  return m_rxDuration;
}

Time
WifiPhy::CalculatePlcpPreambleAndHeaderDuration (WifiTxVector txVector, WifiPreamble preamble)
{
  Time duration = GetPlcpPreambleDuration (txVector, preamble)
    + GetPlcpHeaderDuration (txVector, preamble)
    + GetPlcpHtSigHeaderDuration (preamble)
    + GetPlcpVhtSigA1Duration (preamble)
    + GetPlcpVhtSigA2Duration (preamble)
    + GetPlcpHtTrainingSymbolDuration (preamble, txVector)
    + GetPlcpVhtSigBDuration (preamble);
  return duration;
}

Time
WifiPhy::CalculateTxDuration (uint32_t size, WifiTxVector txVector, WifiPreamble preamble, double frequency, enum mpduType mpdutype, uint8_t incFlag)
{
  Time duration = CalculatePlcpPreambleAndHeaderDuration (txVector, preamble)
    + GetPayloadDuration (size, txVector, preamble, frequency, mpdutype, incFlag);
  return duration;
}

Time
WifiPhy::CalculateTxDuration (uint32_t size, WifiTxVector txVector, WifiPreamble preamble, double frequency)
{
  return CalculateTxDuration (size, txVector, preamble, frequency, NORMAL_MPDU, 0);
}

void
WifiPhy::SetAntenna (Ptr<AbstractAntenna> antenna)
{
  m_antenna = antenna;
}

Ptr<AbstractAntenna>
WifiPhy::GetAntenna (void) const
{
  return m_antenna;
}

void
WifiPhy::SetDirectionalAntenna (Ptr<DirectionalAntenna> antenna)
{
  m_directionalAntenna = antenna;
}

Ptr<DirectionalAntenna>
WifiPhy::GetDirectionalAntenna (void) const
{
  return m_directionalAntenna;
}

void
WifiPhy::NotifyTxBegin (Ptr<const Packet> packet)
{
  m_phyTxBeginTrace (packet);
}

void
WifiPhy::NotifyTxEnd (Ptr<const Packet> packet)
{
  m_phyTxEndTrace (packet);
}

void
WifiPhy::NotifyTxDrop (Ptr<const Packet> packet)
{
  m_phyTxDropTrace (packet);
}

void
WifiPhy::NotifyRxBegin (Ptr<const Packet> packet)
{
  m_phyRxBeginTrace (packet);
}

void
WifiPhy::NotifyRxEnd (Ptr<const Packet> packet)
{
  m_phyRxEndTrace (packet);
}

void
WifiPhy::NotifyRxDrop (Ptr<const Packet> packet)
{
  m_phyRxDropTrace (packet);
}

void
WifiPhy::NotifyMonitorSniffRx (Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, WifiPreamble preamble, WifiTxVector txVector, struct mpduInfo aMpdu, struct signalNoiseDbm signalNoise)
{
  m_phyMonitorSniffRxTrace (packet, channelFreqMhz, channelNumber, rate, preamble, txVector, aMpdu, signalNoise);
}

void
WifiPhy::NotifyMonitorSniffTx (Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, WifiPreamble preamble, WifiTxVector txVector, struct mpduInfo aMpdu)
{
  m_phyMonitorSniffTxTrace (packet, channelFreqMhz, channelNumber, rate, preamble, txVector, aMpdu);
}

// Clause 15 rates (DSSS)

WifiMode
WifiPhy::GetDsssRate1Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DsssRate1Mbps",
                                     WIFI_MOD_CLASS_DSSS,
                                     true,
                                     WIFI_CODE_RATE_UNDEFINED,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDsssRate2Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DsssRate2Mbps",
                                     WIFI_MOD_CLASS_DSSS,
                                     true,
                                     WIFI_CODE_RATE_UNDEFINED,
                                     4);
  return mode;
}


// Clause 18 rates (HR/DSSS)

WifiMode
WifiPhy::GetDsssRate5_5Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DsssRate5_5Mbps",
                                     WIFI_MOD_CLASS_HR_DSSS,
                                     true,
                                     WIFI_CODE_RATE_UNDEFINED,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetDsssRate11Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DsssRate11Mbps",
                                     WIFI_MOD_CLASS_HR_DSSS,
                                     true,
                                     WIFI_CODE_RATE_UNDEFINED,
                                     256);
  return mode;
}


// Clause 19.5 rates (ERP-OFDM)

WifiMode
WifiPhy::GetErpOfdmRate6Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate6Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate9Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate9Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate12Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate12Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate18Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate18Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate24Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate24Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate36Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate36Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate48Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate48Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     false,
                                     WIFI_CODE_RATE_2_3,
                                     64);
  return mode;
}

WifiMode
WifiPhy::GetErpOfdmRate54Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("ErpOfdmRate54Mbps",
                                     WIFI_MOD_CLASS_ERP_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     64);
  return mode;
}


// Clause 17 rates (OFDM)

WifiMode
WifiPhy::GetOfdmRate6Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate6Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate9Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate9Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate12Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate12Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate18Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate18Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate24Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate24Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate36Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate36Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate48Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate48Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_2_3,
                                     64);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate54Mbps ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate54Mbps",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     64);
  return mode;
}


// 10 MHz channel rates

WifiMode
WifiPhy::GetOfdmRate3MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate3MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate4_5MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate4_5MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate6MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate6MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate9MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate9MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate12MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate12MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate18MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate18MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate24MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate24MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_2_3,
                                     64);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate27MbpsBW10MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate27MbpsBW10MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     64);
  return mode;
}


// 5 MHz channel rates

WifiMode
WifiPhy::GetOfdmRate1_5MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate1_5MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate2_25MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate2_25MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate3MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate3MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate4_5MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate4_5MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate6MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate6MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     true,
                                     WIFI_CODE_RATE_1_2,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate9MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate9MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate12MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate12MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_2_3,
                                     64);
  return mode;
}

WifiMode
WifiPhy::GetOfdmRate13_5MbpsBW5MHz ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("OfdmRate13_5MbpsBW5MHz",
                                     WIFI_MOD_CLASS_OFDM,
                                     false,
                                     WIFI_CODE_RATE_3_4,
                                     64);
  return mode;
}


// Clause 20

WifiMode
WifiPhy::GetHtMcs0 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs0", 0, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs1 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs1", 1, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs2 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs2", 2, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs3 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs3", 3, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs4 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs4", 4, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs5 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs5", 5, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs6 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs6", 6, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs7 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs7", 7, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs8 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs8", 8, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs9 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs9", 9, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs10 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs10", 10, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs11 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs11", 11, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs12 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs12", 12, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs13 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs13", 13, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs14 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs14", 14, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs15 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs15", 15, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs16 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs16", 16, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs17 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs17", 17, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs18 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs18", 18, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs19 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs19", 19, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs20 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs20", 20, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs21 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs21", 21, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs22 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs22", 22, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs23 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs23", 23, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs24 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs24", 24, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs25 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs25", 25, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs26 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs26", 26, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs27 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs27", 27, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs28 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs28", 28, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs29 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs29", 29, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs30 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs30", 30, WIFI_MOD_CLASS_HT);
  return mcs;
}

WifiMode
WifiPhy::GetHtMcs31 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("HtMcs31", 31, WIFI_MOD_CLASS_HT);
  return mcs;
}


// Clause 22

WifiMode
WifiPhy::GetVhtMcs0 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs0", 0, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs1 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs1", 1, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs2 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs2", 2, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs3 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs3", 3, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs4 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs4", 4, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs5 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs5", 5, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs6 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs6", 6, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs7 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs7", 7, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs8 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs8", 8, WIFI_MOD_CLASS_VHT);
  return mcs;
}

WifiMode
WifiPhy::GetVhtMcs9 ()
{
  static WifiMode mcs =
    WifiModeFactory::CreateWifiMcs ("VhtMcs9", 9, WIFI_MOD_CLASS_VHT);
  return mcs;
}

bool
WifiPhy::IsValidTxVector (WifiTxVector txVector)
{
  uint32_t chWidth = txVector.GetChannelWidth();
  uint8_t nss = txVector.GetNss();
  std::string modeName = txVector.GetMode().GetUniqueName();

  if (chWidth == 20)
    {
      if (nss != 3 && nss != 6)
        {
          return (modeName != "VhtMcs9");
        }
    }
  else if (chWidth == 80)
    {
      if (nss == 3 || nss == 7)
        {
          return (modeName != "VhtMcs6");
        }
      else if (nss == 6)
        {
          return (modeName != "VhtMcs9");
        }
    }
  else if (chWidth == 160)
    {
      if (nss == 3)
        {
          return (modeName != "VhtMcs9");
        }
    }

  return true;
}

/*
 * 802.11ad PHY Layer Rates (Clause 21 Rates)
 */

/* DMG Control PHY */
WifiMode
WifiPhy::GetDMG_MCS0 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS0",
                                     WIFI_MOD_CLASS_DMG_CTRL,
                                     true,
                                     1880000000, 27500000,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

/* DMG SC PHY */
WifiMode
WifiPhy::GetDMG_MCS1 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS1",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     true,
                                     1880000000, 385000000,
                                     WIFI_CODE_RATE_1_4, /* 2 repetition */
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS2 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS2",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     true,
                                     1880000000, 770000000,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS3 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS3",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     true,
                                     1880000000, 962500000,
                                     WIFI_CODE_RATE_5_8,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS4 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS4",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     true, /* VHT SC MCS1-4 mandatory*/
                                     1880000000, 1155000000,
                                     WIFI_CODE_RATE_3_4,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS5 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS5",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 1251250000,
                                     WIFI_CODE_RATE_13_16,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS6 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS6",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 1540000000,
                                     WIFI_CODE_RATE_1_2,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS7 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS7",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 1925000000,
                                     WIFI_CODE_RATE_5_8,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS8 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS8",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 2310000000ULL,
                                     WIFI_CODE_RATE_3_4,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS9 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS9",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 2502500000ULL,
                                     WIFI_CODE_RATE_13_16,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS10 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS10",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 3080000000ULL,
                                     WIFI_CODE_RATE_1_2,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS11 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS11",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 3850000000ULL,
                                     WIFI_CODE_RATE_5_8,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS12 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS12",
                                     WIFI_MOD_CLASS_DMG_SC,
                                     false,
                                     1880000000, 4620000000ULL,
                                     WIFI_CODE_RATE_3_4,
                                     16);
  return mode;
}

/**** OFDM MCSs BELOW ****/
WifiMode
WifiPhy::GetDMG_MCS13 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS13",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     true,
                                     1880000000, 693000000ULL,
                                     WIFI_CODE_RATE_1_2,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS14 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS14",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 866250000ULL,
                                     WIFI_CODE_RATE_5_8,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS15 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS15",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 1386000000ULL,
                                     WIFI_CODE_RATE_1_2,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS16 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS16",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 1732500000ULL,
                                     WIFI_CODE_RATE_5_8,
                                     4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS17 ()
{
  static WifiMode mode =
  WifiModeFactory::CreateWifiMode ("DMG_MCS17",
                                   WIFI_MOD_CLASS_DMG_OFDM,
                                   false,
                                   1880000000, 2079000000ULL,
                                   WIFI_CODE_RATE_3_4,
                                   4);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS18 ()
{
  static WifiMode mode =
  WifiModeFactory::CreateWifiMode ("DMG_MCS18",
                                   WIFI_MOD_CLASS_DMG_OFDM,
                                   false,
                                   1880000000, 2772000000ULL,
                                   WIFI_CODE_RATE_1_2,
                                   16);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS19 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS19",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 3465000000ULL,
                                     WIFI_CODE_RATE_5_8,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS20 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS20",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 4158000000ULL,
                                     WIFI_CODE_RATE_3_4,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS21 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS21",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 4504500000ULL,
                                     WIFI_CODE_RATE_13_16,
                                     16);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS22 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS22",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 5197500000ULL,
                                     WIFI_CODE_RATE_5_8,
                                     64);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS23 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS23",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 6237000000ULL,
                                     WIFI_CODE_RATE_3_4,
                                     64);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS24 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS24",
                                     WIFI_MOD_CLASS_DMG_OFDM,
                                     false,
                                     1880000000, 6756750000ULL,
                                     WIFI_CODE_RATE_13_16,
                                     64);
  return mode;
}

/**** Low Power SC MCSs ****/
WifiMode
WifiPhy::GetDMG_MCS25 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS25",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 626000000,
                                     WIFI_CODE_RATE_13_28,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS26 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS26",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 834000000,
                                     WIFI_CODE_RATE_13_21,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS27 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS27",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 1112000000ULL,
                                     WIFI_CODE_RATE_52_63,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS28 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS28",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 1251000000ULL,
                                     WIFI_CODE_RATE_13_28,
                                     2);
  return mode;
}

WifiMode
WifiPhy::GetDMG_MCS29 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS29",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 1668000000ULL,
                                     WIFI_CODE_RATE_13_21,
                                     4);
  return mode;
}


WifiMode
WifiPhy::GetDMG_MCS30 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS30",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 2224000000ULL,
                                     WIFI_CODE_RATE_52_63,
                                     4);
  return mode;
}


WifiMode
WifiPhy::GetDMG_MCS31 ()
{
  static WifiMode mode =
    WifiModeFactory::CreateWifiMode ("DMG_MCS31",
                                     WIFI_MOD_CLASS_DMG_LP_SC,
                                     false,
                                     1880000000, 2503000000ULL,
                                     WIFI_CODE_RATE_13_14,
                                     4);
  return mode;
}

std::ostream& operator<< (std::ostream& os, enum WifiPhy::State state)
{
  switch (state)
    {
    case WifiPhy::IDLE:
      return (os << "IDLE");
    case WifiPhy::CCA_BUSY:
      return (os << "CCA_BUSY");
    case WifiPhy::TX:
      return (os << "TX");
    case WifiPhy::RX:
      return (os << "RX");
    case WifiPhy::SWITCHING:
      return (os << "SWITCHING");
    case WifiPhy::SLEEP:
      return (os << "SLEEP");
    default:
      NS_FATAL_ERROR ("Invalid WifiPhy state");
      return (os << "INVALID");
    }
}

} //namespace ns3

namespace {

static class Constructor
{
public:
  Constructor ()
  {
    ns3::WifiPhy::GetDsssRate1Mbps ();
    ns3::WifiPhy::GetDsssRate2Mbps ();
    ns3::WifiPhy::GetDsssRate5_5Mbps ();
    ns3::WifiPhy::GetDsssRate11Mbps ();
    ns3::WifiPhy::GetErpOfdmRate6Mbps ();
    ns3::WifiPhy::GetErpOfdmRate9Mbps ();
    ns3::WifiPhy::GetErpOfdmRate12Mbps ();
    ns3::WifiPhy::GetErpOfdmRate18Mbps ();
    ns3::WifiPhy::GetErpOfdmRate24Mbps ();
    ns3::WifiPhy::GetErpOfdmRate36Mbps ();
    ns3::WifiPhy::GetErpOfdmRate48Mbps ();
    ns3::WifiPhy::GetErpOfdmRate54Mbps ();
    ns3::WifiPhy::GetOfdmRate6Mbps ();
    ns3::WifiPhy::GetOfdmRate9Mbps ();
    ns3::WifiPhy::GetOfdmRate12Mbps ();
    ns3::WifiPhy::GetOfdmRate18Mbps ();
    ns3::WifiPhy::GetOfdmRate24Mbps ();
    ns3::WifiPhy::GetOfdmRate36Mbps ();
    ns3::WifiPhy::GetOfdmRate48Mbps ();
    ns3::WifiPhy::GetOfdmRate54Mbps ();
    ns3::WifiPhy::GetOfdmRate3MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate4_5MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate6MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate9MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate12MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate18MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate24MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate27MbpsBW10MHz ();
    ns3::WifiPhy::GetOfdmRate1_5MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate2_25MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate3MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate4_5MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate6MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate9MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate12MbpsBW5MHz ();
    ns3::WifiPhy::GetOfdmRate13_5MbpsBW5MHz ();
    ns3::WifiPhy::GetHtMcs0 ();
    ns3::WifiPhy::GetHtMcs1 ();
    ns3::WifiPhy::GetHtMcs2 ();
    ns3::WifiPhy::GetHtMcs3 ();
    ns3::WifiPhy::GetHtMcs4 ();
    ns3::WifiPhy::GetHtMcs5 ();
    ns3::WifiPhy::GetHtMcs6 ();
    ns3::WifiPhy::GetHtMcs7 ();
    ns3::WifiPhy::GetHtMcs8 ();
    ns3::WifiPhy::GetHtMcs9 ();
    ns3::WifiPhy::GetHtMcs10 ();
    ns3::WifiPhy::GetHtMcs11 ();
    ns3::WifiPhy::GetHtMcs12 ();
    ns3::WifiPhy::GetHtMcs13 ();
    ns3::WifiPhy::GetHtMcs14 ();
    ns3::WifiPhy::GetHtMcs15 ();
    ns3::WifiPhy::GetHtMcs16 ();
    ns3::WifiPhy::GetHtMcs17 ();
    ns3::WifiPhy::GetHtMcs18 ();
    ns3::WifiPhy::GetHtMcs19 ();
    ns3::WifiPhy::GetHtMcs20 ();
    ns3::WifiPhy::GetHtMcs21 ();
    ns3::WifiPhy::GetHtMcs22 ();
    ns3::WifiPhy::GetHtMcs23 ();
    ns3::WifiPhy::GetHtMcs24 ();
    ns3::WifiPhy::GetHtMcs25 ();
    ns3::WifiPhy::GetHtMcs26 ();
    ns3::WifiPhy::GetHtMcs27 ();
    ns3::WifiPhy::GetHtMcs28 ();
    ns3::WifiPhy::GetHtMcs29 ();
    ns3::WifiPhy::GetHtMcs30 ();
    ns3::WifiPhy::GetHtMcs31 ();
    ns3::WifiPhy::GetVhtMcs0 ();
    ns3::WifiPhy::GetVhtMcs1 ();
    ns3::WifiPhy::GetVhtMcs2 ();
    ns3::WifiPhy::GetVhtMcs3 ();
    ns3::WifiPhy::GetVhtMcs4 ();
    ns3::WifiPhy::GetVhtMcs5 ();
    ns3::WifiPhy::GetVhtMcs6 ();
    ns3::WifiPhy::GetVhtMcs7 ();
    ns3::WifiPhy::GetVhtMcs8 ();
    ns3::WifiPhy::GetVhtMcs9 ();
	
    /* Data Rates for 802.11ad PHY*/
    ns3::WifiPhy::GetDMG_MCS0 ();
    ns3::WifiPhy::GetDMG_MCS1 ();
    ns3::WifiPhy::GetDMG_MCS2 ();
    ns3::WifiPhy::GetDMG_MCS3 ();
    ns3::WifiPhy::GetDMG_MCS4 ();
    ns3::WifiPhy::GetDMG_MCS5 ();
    ns3::WifiPhy::GetDMG_MCS6 ();
    ns3::WifiPhy::GetDMG_MCS7 ();
    ns3::WifiPhy::GetDMG_MCS8 ();
    ns3::WifiPhy::GetDMG_MCS9 ();
    ns3::WifiPhy::GetDMG_MCS10 ();
    ns3::WifiPhy::GetDMG_MCS11 ();
    ns3::WifiPhy::GetDMG_MCS12 ();
    ns3::WifiPhy::GetDMG_MCS13 ();
    ns3::WifiPhy::GetDMG_MCS14 ();
    ns3::WifiPhy::GetDMG_MCS15 ();
    ns3::WifiPhy::GetDMG_MCS16 ();
    ns3::WifiPhy::GetDMG_MCS17 ();
    ns3::WifiPhy::GetDMG_MCS18 ();
    ns3::WifiPhy::GetDMG_MCS19 ();
    ns3::WifiPhy::GetDMG_MCS20 ();
    ns3::WifiPhy::GetDMG_MCS21 ();
    ns3::WifiPhy::GetDMG_MCS22 ();
    ns3::WifiPhy::GetDMG_MCS23 ();
    ns3::WifiPhy::GetDMG_MCS24 ();
  }
} g_constructor;

}
