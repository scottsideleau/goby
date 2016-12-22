// Copyright 2009-2016 Toby Schneider (http://gobysoft.org/index.wt/people/toby)
//                     GobySoft, LLC (2013-)
//                     Massachusetts Institute of Technology (2007-2014)
//                     Community contributors (see AUTHORS file)
//
//
// This file is part of the Goby Underwater Autonomy Project Libraries
// ("The Goby Libraries").
//
// The Goby Libraries are free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// The Goby Libraries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.

#include "benthos_atm900_driver.h"
#include "driver_exception.h"

#include "goby/common/logger.h"
#include "goby/util/binary.h"

using goby::util::hex_encode;
using goby::util::hex_decode;
using goby::glog;
using namespace goby::common::logger;
using goby::common::goby_time;

const std::string goby::acomms::BenthosATM900Driver::SERIAL_DELIMITER = "\r\n";

goby::acomms::BenthosATM900Driver::BenthosATM900Driver()
    : fsm_(driver_cfg_),
      next_frame_(0)
{
}

void goby::acomms::BenthosATM900Driver::startup(const protobuf::DriverConfig& cfg)
{
    driver_cfg_ = cfg;

    if(!cfg.has_line_delimiter())
        driver_cfg_.set_line_delimiter(SERIAL_DELIMITER);
    
    if(!driver_cfg_.has_serial_baud())
        driver_cfg_.set_serial_baud(DEFAULT_BAUD);

    glog.is(DEBUG1) && glog << group(glog_out_group()) << "BenthosATM900Driver: Starting modem..." << std::endl;
    ModemDriverBase::modem_start(driver_cfg_);
    fsm_.initiate();

       
    int i = 0;
    while(fsm_.state_cast<const benthos_fsm::Ready *>() == 0)
    {
        do_work();
        
        const int pause_ms = 10;
        usleep(pause_ms*1000);
        ++i;

        const int start_timeout = driver_cfg_.GetExtension(BenthosATM900DriverConfig::start_timeout);
        if(i / (1000/pause_ms) > start_timeout)
            throw(ModemDriverException("Failed to startup.", protobuf::ModemDriverStatus::STARTUP_FAILED));
    }
}

void goby::acomms::BenthosATM900Driver::shutdown()
{
    fsm_.process_event(benthos_fsm::EvReset());

    while(fsm_.state_cast<const benthos_fsm::Ready *>() == 0)
    {
        do_work();
        usleep(10000);
    }
    
    fsm_.process_event(benthos_fsm::EvRequestLowPower());

    while(fsm_.state_cast<const benthos_fsm::LowPower *>() == 0)
    {
        do_work();
        usleep(10000);
    }
    
    ModemDriverBase::modem_close();
} 

void goby::acomms::BenthosATM900Driver::handle_initiate_transmission(const protobuf::ModemTransmission& orig_msg)
{
    protobuf::ModemTransmission msg = orig_msg;
    signal_modify_transmission(&msg);
 
    if(!msg.has_frame_start())
        msg.set_frame_start(next_frame_);

    // set the frame size, if not set or if it exceeds the max configured
    if(!msg.has_max_frame_bytes() || msg.max_frame_bytes() > driver_cfg_.GetExtension(BenthosATM900DriverConfig::max_frame_size))
        msg.set_max_frame_bytes(driver_cfg_.GetExtension(BenthosATM900DriverConfig::max_frame_size));
    
    signal_data_request(&msg);

    next_frame_ += msg.frame_size();
    
    if(!(msg.frame_size() == 0 || msg.frame(0).empty()))
    {
        fsm_.process_event(benthos_fsm::EvDial(msg.dest()));
        send(msg);
    }
} 

void goby::acomms::BenthosATM900Driver::do_work()
{
    try_serial_tx();
    
    std::string in;
    while(modem_read(&in))
    {
        benthos_fsm::EvRxSerial data_event;
        data_event.line = in;

        
        glog.is(DEBUG1) && glog << group(glog_in_group())
                                << (boost::algorithm::all(in, boost::is_print() ||
                                                          boost::is_any_of("\r\n")) ?
                                    boost::trim_copy(in) :
                                    goby::util::hex_encode(in)) << std::endl;
        
        fsm_.process_event(data_event);
    }
    
    while(!fsm_.received().empty())
    {
        receive(fsm_.received().front());
        fsm_.received().pop_front();
    }
    
    try_serial_tx();
}


void goby::acomms::BenthosATM900Driver::receive(const protobuf::ModemTransmission& msg)
{
    glog.is(DEBUG2) && glog << group(glog_in_group()) << msg << std::endl;

    if(msg.type() == protobuf::ModemTransmission::DATA &&
       msg.ack_requested() && msg.dest() == driver_cfg_.modem_id())
    {
        // make any acks
        protobuf::ModemTransmission ack;
        ack.set_type(goby::acomms::protobuf::ModemTransmission::ACK);
        ack.set_src(msg.dest());
        ack.set_dest(msg.src());
        ack.set_rate(msg.rate());
        for(int i = msg.frame_start(), n = msg.frame_size() + msg.frame_start(); i < n; ++i)
            ack.add_acked_frame(i);
        send(ack);
    }
    
    signal_receive(msg);
}

void goby::acomms::BenthosATM900Driver::send(const protobuf::ModemTransmission& msg)
{
    glog.is(DEBUG2) && glog << group(glog_out_group()) << msg << std::endl;
    fsm_.buffer_data_out(msg);
}


void goby::acomms::BenthosATM900Driver::try_serial_tx()
{
    fsm_.process_event(benthos_fsm::EvTxSerial());

    while(!fsm_.serial_tx_buffer().empty())
    {
        const std::string& line = fsm_.serial_tx_buffer().front();
        
        glog.is(DEBUG1) && glog << group(glog_out_group())
                                << (boost::algorithm::all(line, boost::is_print() ||
                                                          boost::is_any_of("\r\n")) ?
                                    boost::trim_copy(line) :
                                    goby::util::hex_encode(line)) << std::endl;
        
        modem_write(line);
        
        fsm_.serial_tx_buffer().pop_front();
    }
}
