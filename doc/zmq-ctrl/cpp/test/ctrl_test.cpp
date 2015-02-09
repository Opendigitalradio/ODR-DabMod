/**
 * This is a test program for the zmq ctrl API of the odr-dabmod.
 *
 * Copyright (c) 2015 by Jörgen Scott (jorgen.scott@paneda.se)
 
 * This file is part of CtrlTest.
 *
 * ODR-DabMod is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * ODR-DabMod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 **/
#define BOOST_TEST_MODULE "C++ unit tests for odr-mod zmq ctrl"
#include <boost/test/unit_test.hpp>
#include "OdrModCtrl.hpp"


// Variables used in the test suite
struct TemplateVars
{
	std::string error;
	zmq::context_t context;
	COdrModCtrl modCtrl;

	// NOTE: Make sure the odr-dabmod is started before running the test and
	// that the zmq endpoint matches.
	TemplateVars() : context(1), modCtrl(&context, "tcp://127.0.0.1:9400", 1000) {}
	~TemplateVars() {}
};

// Note. The odr-mod do not validate parameters therefore there are no tests
// made for setting invalid parameters.
BOOST_FIXTURE_TEST_SUITE(test_template1, TemplateVars)

BOOST_AUTO_TEST_CASE (Ping)
{
	BOOST_CHECK(modCtrl.Ping() == true);
}

BOOST_AUTO_TEST_CASE (DigitalGain)
{
	BOOST_CHECK(modCtrl.SetDigitalGain(0.5, error) == true);
	double value;
	BOOST_CHECK(modCtrl.GetDigitalGain(value, error) == true);
	BOOST_CHECK(value == 0.5);
}

BOOST_AUTO_TEST_CASE (TxGain)
{
	BOOST_CHECK(modCtrl.SetTxGain(50, error) == true);
	double value;
	BOOST_CHECK(modCtrl.GetTxGain(value, error) == true);
	BOOST_CHECK(value == 50);
}

BOOST_AUTO_TEST_CASE (TxFrequency)
{
	BOOST_CHECK(modCtrl.SetTxFrequency(234208000, error) == true);
	double value;
	BOOST_CHECK(modCtrl.GetTxFrequency(value, error) == true);
	BOOST_CHECK(value == 234208000);
}

BOOST_AUTO_TEST_CASE (Muting)
{
	BOOST_CHECK(modCtrl.SetMuting(true, error) == true);
	bool value;
	BOOST_CHECK(modCtrl.GetMuting(value, error) == true);
	BOOST_CHECK(value == true);
	BOOST_CHECK(modCtrl.SetMuting(false, error) == true);
}

BOOST_AUTO_TEST_CASE (StaticDelay)
{
	// reset first (by setting out of range value) or else test
	// will fail on successive runs
	BOOST_CHECK(modCtrl.SetStaticDelay(100000, error) == true);
	BOOST_CHECK(modCtrl.SetStaticDelay(45000, error) == true);
	uint32_t value;
	BOOST_CHECK(modCtrl.GetStaticDelay(value, error) == true);
	BOOST_CHECK(value == 45000);
}


BOOST_AUTO_TEST_SUITE_END()

