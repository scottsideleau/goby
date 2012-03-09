// Copyright 2009-2012 Toby Schneider (https://launchpad.net/~tes)
//                     Massachusetts Institute of Technology (2007-)
//                     Woods Hole Oceanographic Institution (2007-)
//                     Goby Developers Team (https://launchpad.net/~goby-dev)
// 
//
// This file is part of the Goby Underwater Autonomy Project Libraries
// ("The Goby Libraries").
//
// The Goby Libraries are free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The Goby Libraries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.


#include "flex_ostream.h"
#include "logger_manipulators.h"

using namespace goby::common::logger;


// boost::shared_ptr<goby::common::FlexOstream> goby::common::FlexOstream::inst_;

// goby::common::FlexOstream& goby::common::glogger()
// {    
//     if(!FlexOstream::inst_) FlexOstream::inst_.reset(new FlexOstream());    
//     return(*FlexOstream::inst_);
// }

int goby::common::FlexOstream::instances_ = 0 ;

goby::common::FlexOstream goby::glog;

void goby::common::FlexOstream::add_group(const std::string& name,
                                        Colors::Color color /*= Colors::nocolor*/,
                                        const std::string& description /*= ""*/)
{

    if(description.empty())
    {
        Group ng(name, name, color);
        sb_.add_group(name, ng);
    }
    else
    {
        Group ng(name, description, color);
        sb_.add_group(name, ng);
    }
    
    
    *this << "Adding FlexOstream group: " << sb_.color2esc_code(color) << name
          << sb_.color2esc_code(Colors::nocolor) << " (" << description << ")" << std::endl;
}


std::ostream& goby::common::FlexOstream::operator<<(std::ostream& (*pf) (std::ostream&))
{
    if(pf == die)   sb_.set_die_flag(true);
    return std::ostream::operator<<(pf);
}            

bool goby::common::FlexOstream::is(logger::Verbosity verbosity,
                                 logger_lock::LockAction lock_action /*= none*/)
{
        
    bool display = (sb_.highest_verbosity() >= verbosity) || (verbosity == logger::DIE);

    if(display)
    {
        if(lock_action == logger_lock::lock)
        {
            goby::common::logger::mutex.lock(); 
        }
            
        switch(verbosity)
        {
            case QUIET: break;
            case WARN: *this << warn; break;
            case VERBOSE: *this << verbose; break;
            case DEBUG1: *this << debug1; break;
            case DEBUG2: *this << debug2; break;
            case DEBUG3: *this << debug3; break;
            case DIE: *this << die; break;
        }

        sb_.set_verbosity_depth(verbosity);
    }
                
    return display;
}
