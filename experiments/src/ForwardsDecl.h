//******************************************************************************
//* File:   ForwardsDecl.h
//* Author: Jon Newman <jpnewman snail mit dot edu>
//*
//* Copyright (c) Jon Newman (jpnewman snail mit dot edu)
//* All right reserved.
//* This file is part of the Oat project.
//* This is free software: you can redistribute it and/or modify
//* it under the terms of the GNU General Public License as published by
//* the Free Software Foundation, either version 3 of the License, or
//* (at your option) any later version.
//* This software is distributed in the hope that it will be useful,
//* but WITHOUT ANY WARRANTY; without even the implied warranty of
//* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//* GNU General Public License for more details.
//* You should have received a copy of the GNU General Public License
//* along with this source code.  If not, see <http://www.gnu.org/licenses/>.
//******************************************************************************

#ifndef FORWARDSDECL_H
#define	FORWARDSDECL_H

#include <boost/interprocess/interprocess_fwd.hpp>

namespace oat {

    namespace bip = boost::interprocess;

    //class bip::managed_shared_memory::handle_t;
    //class boost::posix_time::milliseconds;
    //class bip::managed_shared_memory;

    using shmem_t = bip::managed_shared_memory;
    using handle_t = bip::managed_shared_memory::handle_t;
    using msec_t = boost::posix_time::milliseconds;

} // namespace oat

#endif	/* FORWARDSDECL_H */
