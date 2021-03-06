/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, 2016 IMDEA Networks Institute
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
 * Author: Hany Assasa <hany.assasa@gmail.com>
 */
#include "ns3/fatal-error.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include "common-header.h"
#include "dmg-information-elements.h"

#include "supported-rates.h"
#include "ht-capabilities.h"
#include "ht-operations.h"
#include "erp-information.h"
#include "vht-capabilities.h"
#include "dmg-capabilities.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CommonHeader");

/***********************************************************
 *          Generic Wifi Management Frame
 ***********************************************************/
void
MgtFrame::AddWifiInformationElement (Ptr<WifiInformationElement> element)
{
  m_map[element->ElementId ()] = element;
}

Ptr<WifiInformationElement>
MgtFrame::GetInformationElement (WifiInformationElementId id)
{
  return m_map[id];
}

uint32_t
MgtFrame::GetInformationElementsSerializedSize (void) const
{
  Ptr<WifiInformationElement> element;
  uint32_t size = 0;
  for (WifiInformationElementMap::const_iterator elem = m_map.begin (); elem != m_map.end (); elem++)
    {
      element = elem->second;
      size += element->GetSerializedSize ();
    }
  return size;
}

WifiInformationElementMap
MgtFrame::GetListOfInformationElement (void) const
{
  return m_map;
}

void
MgtFrame::PrintInformationElements (std::ostream &os) const
{

}

Buffer::Iterator
MgtFrame::SerializeInformationElements (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  Ptr<WifiInformationElement> element;
  for (WifiInformationElementMap::const_iterator elem = m_map.begin (); elem != m_map.end (); elem++)
    {
      element = elem->second;
      i = element->Serialize (i);
    }
  return i;
}

Buffer::Iterator
MgtFrame::DeserializeInformationElements (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  Ptr<WifiInformationElement> element;
  uint8_t id, length;
  while (!i.IsEnd ())
    {
      i = DeserializeElementID (i, id, length);
      switch (id)
        {
          case IE_SUPPORTED_RATES:
            {
              element = Create<SupportedRates> ();
              break;
            }
          case IE_EXTENDED_SUPPORTED_RATES:
            {
              element = Create<ExtendedSupportedRatesIE> ();
              break;
            }
          case IE_HT_CAPABILITIES:
            {
              element = Create<HtCapabilities> ();
              break;
            }
          case IE_VHT_CAPABILITIES:
            {
              element = Create<VhtCapabilities> ();
              break;
            }
          case IE_HT_OPERATIONS:
            {
              element = Create<HtOperations> ();
              break;
            }
          case IE_ERP_INFORMATION:
            {
              element = Create<ErpInformation> ();
              break;
            }
          case IE_DMG_CAPABILITIES:
            {
              element = Create<DmgCapabilities> ();
              break;
            }
          case IE_MULTI_BAND:
            {
              element = Create<MultiBandElement> ();
              break;
            }
          case IE_DMG_OPERATION:
            {
              element = Create<DmgOperationElement> ();
              break;
            }
          case IE_NEXT_DMG_ATI:
            {
              element = Create<NextDmgAti> ();
              break;
            }
          case IE_RELAY_CAPABILITIES:
            {
              element = Create<RelayCapabilitiesElement> ();
              break;
            }
          case IE_EXTENDED_SCHEDULE:
            {
              element = Create<ExtendedScheduleElement> ();
              break;
            }
        }

      i = element->DeserializeElementBody (i, length);
      m_map[id] = element;
    }

  return i;
}

}
