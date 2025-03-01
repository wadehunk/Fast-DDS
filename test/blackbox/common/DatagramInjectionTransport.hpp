// Copyright 2023 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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

#include <mutex>
#include <set>

#include <fastdds/rtps/transport/ChainingTransport.h>
#include <fastdds/rtps/transport/ChainingTransportDescriptor.h>
#include <fastdds/rtps/transport/SenderResource.h>

using SenderResource = eprosima::fastrtps::rtps::SenderResource;

namespace eprosima {
namespace fastdds {
namespace rtps {

class DatagramInjectionTransportDescriptor : public ChainingTransportDescriptor
{

public:

    DatagramInjectionTransportDescriptor(
            std::shared_ptr<TransportDescriptorInterface> low_level);

    TransportInterface* create_transport() const override;

    void add_receiver(
            TransportReceiverInterface* receiver_interface);

    std::set<TransportReceiverInterface*> get_receivers();

    void update_send_resource_list(
            const SendResourceList& send_resource_list);

    std::set<SenderResource*> get_send_resource_list();

private:

    std::mutex mtx_;
    std::set<TransportReceiverInterface*> receivers_;
    std::set<SenderResource*> send_resource_list_;
};

class DatagramInjectionTransport : public ChainingTransport
{
public:

    DatagramInjectionTransport(
            DatagramInjectionTransportDescriptor* parent)
        : ChainingTransport(*parent)
        , parent_(parent)
    {
    }

    TransportDescriptorInterface* get_configuration() override
    {
        return parent_;
    }

    bool send(
            eprosima::fastrtps::rtps::SenderResource* low_sender_resource,
            const eprosima::fastrtps::rtps::octet* send_buffer,
            uint32_t send_buffer_size,
            eprosima::fastrtps::rtps::LocatorsIterator* destination_locators_begin,
            eprosima::fastrtps::rtps::LocatorsIterator* destination_locators_end,
            const std::chrono::steady_clock::time_point& timeout) override
    {
        return low_sender_resource->send(send_buffer, send_buffer_size, destination_locators_begin,
                       destination_locators_end, timeout);
    }

    void receive(
            TransportReceiverInterface* next_receiver,
            const eprosima::fastrtps::rtps::octet* receive_buffer,
            uint32_t receive_buffer_size,
            const eprosima::fastrtps::rtps::Locator_t& local_locator,
            const eprosima::fastrtps::rtps::Locator_t& remote_locator) override
    {
        next_receiver->OnDataReceived(receive_buffer, receive_buffer_size, local_locator, remote_locator);
    }

    bool OpenInputChannel(
            const eprosima::fastrtps::rtps::Locator_t& loc,
            TransportReceiverInterface* receiver_interface,
            uint32_t max_message_size) override
    {
        bool ret_val = ChainingTransport::OpenInputChannel(loc, receiver_interface, max_message_size);
        if (ret_val)
        {
            parent_->add_receiver(receiver_interface);
        }
        return ret_val;
    }

    bool OpenOutputChannel(
            SendResourceList& send_resource_list,
            const Locator& loc) override
    {
        bool ret_val = ChainingTransport::OpenOutputChannel(send_resource_list, loc);
        parent_->update_send_resource_list(send_resource_list);
        return ret_val;
    }

private:

    DatagramInjectionTransportDescriptor* parent_ = nullptr;
};

} // namespace rtps
} // namespace fastdds
} // namespace eprosima
