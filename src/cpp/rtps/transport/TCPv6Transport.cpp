// Copyright 2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <rtps/transport/TCPv6Transport.h>

#include <utility>
#include <cstring>
#include <algorithm>

#include <asio.hpp>
#include <fastdds/dds/log/Log.hpp>
#include <fastdds/rtps/transport/TCPv6TransportDescriptor.h>
#include <fastrtps/utils/IPLocator.h>

using namespace std;
using namespace asio;

namespace eprosima {
namespace fastdds {
namespace rtps {

using IPFinder = fastrtps::rtps::IPFinder;
using octet = fastrtps::rtps::octet;
using IPLocator = fastrtps::rtps::IPLocator;
using octet = fastrtps::rtps::octet;
using Log = fastdds::dds::Log;

static void get_ipv6s(
        std::vector<IPFinder::info_IP>& locNames,
        bool return_loopback = false)
{
    IPFinder::getIPs(&locNames, return_loopback);
    auto new_end = remove_if(locNames.begin(),
                    locNames.end(),
                    [](IPFinder::info_IP ip)
                    {
                        return ip.type != IPFinder::IP6 && ip.type != IPFinder::IP6_LOCAL;
                    });
    locNames.erase(new_end, locNames.end());
    std::for_each(locNames.begin(), locNames.end(), [](IPFinder::info_IP& loc)
            {
                loc.locator.kind = LOCATOR_KIND_TCPv6;
            });
}

static asio::ip::address_v6::bytes_type locator_to_native(
        Locator& locator)
{
    return{ { IPLocator::getIPv6(locator)[0],
        IPLocator::getIPv6(locator)[1],
        IPLocator::getIPv6(locator)[2],
        IPLocator::getIPv6(locator)[3],
        IPLocator::getIPv6(locator)[4],
        IPLocator::getIPv6(locator)[5],
        IPLocator::getIPv6(locator)[6],
        IPLocator::getIPv6(locator)[7],
        IPLocator::getIPv6(locator)[8],
        IPLocator::getIPv6(locator)[9],
        IPLocator::getIPv6(locator)[10],
        IPLocator::getIPv6(locator)[11],
        IPLocator::getIPv6(locator)[12],
        IPLocator::getIPv6(locator)[13],
        IPLocator::getIPv6(locator)[14],
        IPLocator::getIPv6(locator)[15] } };
}

TCPv6Transport::TCPv6Transport(
        const TCPv6TransportDescriptor& descriptor)
    : TCPTransportInterface(LOCATOR_KIND_TCPv6)
    , configuration_(descriptor)
{
    if (!descriptor.interfaceWhiteList.empty())
    {
        const auto white_begin = descriptor.interfaceWhiteList.begin();
        const auto white_end = descriptor.interfaceWhiteList.end();

        std::vector<IPFinder::info_IP> local_interfaces;
        get_ipv6s(local_interfaces, true);
        for (const IPFinder::info_IP& infoIP : local_interfaces)
        {
            if (std::find_if(white_begin, white_end, [this, infoIP](const std::string& white_list_element)
                    {
                        return white_list_element == infoIP.dev || compare_ips(white_list_element, infoIP.name);
                    }) != white_end )
            {
                interface_whitelist_.emplace_back(ip::address_v6::from_string(infoIP.name));
            }
        }

        if (interface_whitelist_.empty())
        {
            EPROSIMA_LOG_ERROR(TRANSPORT, "All whitelist interfaces were filtered out");
            interface_whitelist_.emplace_back(ip::address_v6::from_string("2001:db8::"));
        }
    }

    if (!configuration_.listening_ports.empty())
    {
        if (configuration_.listening_ports.size() > 1)
        {
            EPROSIMA_LOG_ERROR(RTCP,
                    "Only one listening port is allowed for TCP transport. Only the first port will be used.");
            configuration_.listening_ports.erase(
                configuration_.listening_ports.begin() + 1, configuration_.listening_ports.end());
        }
        Locator locator(LOCATOR_KIND_TCPv6, configuration_.listening_ports.front());
        configuration_.listening_ports.front() = create_acceptor_socket(locator);
    }

#if !TLS_FOUND
    if (descriptor.apply_security)
    {
        EPROSIMA_LOG_ERROR(RTCP_TLS, "Trying to use TCP Transport with TLS but TLS was not found.");
    }
#endif // if !TLS_FOUND
}

TCPv6Transport::TCPv6Transport()
    : TCPTransportInterface(LOCATOR_KIND_TCPv6)
{
}

TCPv6Transport::~TCPv6Transport()
{
    clean();
}

TCPv6TransportDescriptor::TCPv6TransportDescriptor()
    : TCPTransportDescriptor()
{
}

TCPv6TransportDescriptor::TCPv6TransportDescriptor(
        const TCPv6TransportDescriptor& t)
    : TCPTransportDescriptor(t)
{
}

bool TCPv6TransportDescriptor::operator ==(
        const TCPv6TransportDescriptor& t) const
{
    return TCPTransportDescriptor::operator ==(t);
}

TransportInterface* TCPv6TransportDescriptor::create_transport() const
{
    return new TCPv6Transport(*this);
}

void TCPv6Transport::AddDefaultOutputLocator(
        LocatorList& /*defaultList*/)
{
}

const TCPTransportDescriptor* TCPv6Transport::configuration() const
{
    return &configuration_;
}

TCPTransportDescriptor* TCPv6Transport::configuration()
{
    return &configuration_;
}

void TCPv6Transport::get_ips(
        std::vector<IPFinder::info_IP>& locNames,
        bool return_loopback) const
{
    get_ipv6s(locNames, return_loopback);
}

uint16_t TCPv6Transport::GetLogicalPortRange() const
{
    return configuration_.logical_port_range;
}

uint16_t TCPv6Transport::GetLogicalPortIncrement() const
{
    return configuration_.logical_port_increment;
}

uint16_t TCPv6Transport::GetMaxLogicalPort() const
{
    return configuration_.max_logical_port;
}

std::vector<std::string> TCPv6Transport::get_binding_interfaces_list()
{
    std::vector<std::string> vOutputInterfaces;
    if (is_interface_whitelist_empty())
    {
        vOutputInterfaces.push_back(s_IPv6AddressAny);
    }
    else
    {
        for (auto& ip : interface_whitelist_)
        {
            vOutputInterfaces.push_back(ip.to_string());
        }
    }

    return vOutputInterfaces;
}

bool TCPv6Transport::is_locator_allowed(
        const Locator& locator) const
{
    if (!IsLocatorSupported(locator))
    {
        return false;
    }
    if (interface_whitelist_.empty())
    {
        return true;
    }
    return is_interface_allowed(IPLocator::toIPv6string(locator));
}

bool TCPv6Transport::is_interface_whitelist_empty() const
{
    return interface_whitelist_.empty();
}

bool TCPv6Transport::is_interface_allowed(
        const std::string& interface) const
{
    return is_interface_allowed(asio::ip::address_v6::from_string(interface));
}

bool TCPv6Transport::is_interface_allowed(
        const ip::address_v6& ip) const
{
    if (interface_whitelist_.empty())
    {
        return true;
    }

    if (ip == ip::address_v6::any())
    {
        return true;
    }

    for (auto& whitelist : interface_whitelist_)
    {
        if (compare_ips(whitelist.to_string(), ip.to_string()))
        {
            return true;
        }
    }

    return false;
}

LocatorList TCPv6Transport::NormalizeLocator(
        const Locator& locator)
{
    LocatorList list;
    if (IPLocator::isAny(locator))
    {
        std::vector<IPFinder::info_IP> locNames;
        get_ipv6s(locNames);
        for (const auto& infoIP : locNames)
        {
            if (is_interface_allowed(infoIP.name))
            {
                Locator newloc(locator);
                IPLocator::setIPv6(newloc, infoIP.locator);
                list.push_back(newloc);
            }
        }

        if (list.empty())
        {
            Locator newloc(locator);
            IPLocator::setIPv6(newloc, "::1");
            list.push_back(newloc);
        }
    }
    else
    {
        list.push_back(locator);
    }

    return list;
}

bool TCPv6Transport::is_local_locator(
        const Locator& locator) const
{
    assert(locator.kind == LOCATOR_KIND_TCPv6);

    if (IPLocator::isLocal(locator))
    {
        return true;
    }

    for (const IPFinder::info_IP& localInterface : current_interfaces_)
    {
        if (IPLocator::compareAddress(locator, localInterface.locator))
        {
            return true;
        }
    }

    return false;
}

bool TCPv6Transport::compare_locator_ip(
        const Locator& lh,
        const Locator& rh) const
{
    return IPLocator::compareAddress(lh, rh);
}

bool TCPv6Transport::compare_locator_ip_and_port(
        const Locator& lh,
        const Locator& rh) const
{
    return IPLocator::compareAddressAndPhysicalPort(lh, rh);
}

void TCPv6Transport::fill_local_ip(
        Locator& loc) const
{
    loc.kind = kind();
    IPLocator::setIPv6(loc, "::1");
}

ip::tcp::endpoint TCPv6Transport::generate_endpoint(
        const Locator& loc,
        uint16_t port) const
{
    asio::ip::address_v6::bytes_type remoteAddress;
    IPLocator::copyIPv6(loc, remoteAddress.data());
    return ip::tcp::endpoint(asio::ip::address_v6(remoteAddress), port);
}

ip::tcp::endpoint TCPv6Transport::generate_local_endpoint(
        Locator& loc,
        uint16_t port) const
{
    return ip::tcp::endpoint(asio::ip::address_v6(locator_to_native(loc)), port);
}

ip::tcp::endpoint TCPv6Transport::generate_endpoint(
        uint16_t port) const
{
    return asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port);
}

asio::ip::tcp TCPv6Transport::generate_protocol() const
{
    return asio::ip::tcp::v6();
}

bool TCPv6Transport::is_interface_allowed(
        const Locator& loc) const
{
    asio::ip::address_v6 ip = asio::ip::address_v6::from_string(IPLocator::toIPv6string(loc));
    return is_interface_allowed(ip);
}

void TCPv6Transport::set_receive_buffer_size(
        uint32_t size)
{
    configuration_.receiveBufferSize = size;
}

void TCPv6Transport::set_send_buffer_size(
        uint32_t size)
{
    configuration_.sendBufferSize = size;
}

void TCPv6Transport::endpoint_to_locator(
        const ip::tcp::endpoint& endpoint,
        Locator& locator) const
{
    locator.kind = LOCATOR_KIND_TCPv6;
    IPLocator::setPhysicalPort(locator, endpoint.port());
    auto ipBytes = endpoint.address().to_v6().to_bytes();
    IPLocator::setIPv6(locator, ipBytes.data());
}

bool TCPv6Transport::compare_ips(
        const std::string& ip1,
        const std::string& ip2) const
{
    // string::find returns string::npos if the character is not found
    // If the second parameter is string::npos value, it indicates to take all characters until the end of the string
    std::string substr1 = ip1.substr(0, ip1.find('%'));
    std::string substr2 = ip2.substr(0, ip2.find('%'));

    if (substr1.compare(substr2) == 0)
    {
        return true;
    }
    return false;
}

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima
