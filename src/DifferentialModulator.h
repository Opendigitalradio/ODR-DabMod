/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2026
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "ModPlugin.h"
#include "RemoteControl.h"
#include <vector>
#include <cstdint>


class DifferentialModulator : public ModMux, public RemoteControllable
{
public:
    DifferentialModulator(size_t carriers, bool fixedPoint, bool withNeon);
    virtual ~DifferentialModulator();
    DifferentialModulator(const DifferentialModulator&);
    DifferentialModulator& operator=(const DifferentialModulator&);


    int process(std::vector<Buffer*> dataIn, Buffer* dataOut) override;
    const char* name() override { return "DifferentialModulator"; }

    /******* REMOTE CONTROL ********/
    virtual void set_parameter(const std::string& parameter, const std::string& value) override;
    virtual const std::string get_parameter(const std::string& parameter) const override;
    virtual const json::map_t get_all_values() const override;

protected:
    size_t m_carriers;
    size_t m_fixedPoint;
    bool m_withNeon;
};

