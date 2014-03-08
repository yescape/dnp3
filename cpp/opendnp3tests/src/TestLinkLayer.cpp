/**
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */
#include <boost/test/unit_test.hpp>

#include <opendnp3/DNPConstants.h>

#include "Exception.h"
#include "TestHelpers.h"
#include "BufferHelpers.h"
#include "LinkLayerTest.h"

#include <iostream>

using namespace openpal;
using namespace opendnp3;

BOOST_AUTO_TEST_SUITE(LinkLayerTestSuite)

// All operations should fail except for OnLowerLayerUp, just a representative
// number of them
BOOST_AUTO_TEST_CASE(ClosedState)
{
	LinkLayerTest t;
	t.upper.SendDown("00");
	BOOST_REQUIRE(t.log.PopOneEntry(LogLevel::Error));
	t.link.OnLowerLayerDown();
	BOOST_REQUIRE(t.log.PopOneEntry(LogLevel::Error));
	t.link.Ack(false, false, 1, 2);
	BOOST_REQUIRE(t.log.PopOneEntry(LogLevel::Error));
}

// Prove that the upper layer is notified when the lower layer comes online
BOOST_AUTO_TEST_CASE(ForwardsOnLowerLayerUp)
{
	LinkLayerTest t;
	BOOST_REQUIRE_FALSE(t.upper.IsLowerLayerUp());
	t.link.OnLowerLayerUp();
	BOOST_REQUIRE(t.upper.IsLowerLayerUp());
	t.link.OnLowerLayerUp();
	BOOST_REQUIRE(t.log.PopOneEntry(LogLevel::Error));	
}

// Check that once the layer comes up, validation errors can occur
BOOST_AUTO_TEST_CASE(ValidatesMasterSlaveBit)
{
	LinkLayerTest t; t.link.OnLowerLayerUp();
	t.link.Ack(true, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_MASTER_BIT_MATCH);
}

// Only process frames from your designated remote address
BOOST_AUTO_TEST_CASE(ValidatesSourceAddress)
{
	LinkLayerTest t; t.link.OnLowerLayerUp();
	t.link.Ack(false, false, 1, 1023);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNKNOWN_SOURCE);
}

// This should actually never happen when using the LinkLayerRouter
// Only process frame addressed to you
BOOST_AUTO_TEST_CASE(ValidatesDestinationAddress)
{
	LinkLayerTest t;  t.link.OnLowerLayerUp();
	t.link.Ack(false, false, 2, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNKNOWN_DESTINATION);
}

// Show that the base state of idle logs SecToPri frames as errors
BOOST_AUTO_TEST_CASE(SecToPriNoContext)
{
	LinkLayerTest t; t.link.OnLowerLayerUp();

	BOOST_REQUIRE(t.log.IsLogErrorFree());
	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNEXPECTED_FRAME);


	BOOST_REQUIRE(t.log.IsLogErrorFree());
	t.link.Nack(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNEXPECTED_FRAME);


	BOOST_REQUIRE(t.log.IsLogErrorFree());
	t.link.LinkStatus(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNEXPECTED_FRAME);


	BOOST_REQUIRE(t.log.IsLogErrorFree());
	t.link.NotSupported(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNEXPECTED_FRAME);
}

// Show that the base state of idle forwards unconfirmed user data
BOOST_AUTO_TEST_CASE(UnconfirmedDataPassedUpFromIdleUnreset)
{
	LinkLayerTest t; t.link.OnLowerLayerUp();
	ByteStr bs(250, 0);
	t.link.UnconfirmedUserData(false, 1, 1024, bs.ToReadOnly());
	BOOST_REQUIRE(t.log.IsLogErrorFree());
	BOOST_REQUIRE(t.upper.BufferEquals(bs, bs.Size()));
}

// Show that the base state of idle forwards unconfirmed user data
BOOST_AUTO_TEST_CASE(ConfirmedDataIgnoredFromIdleUnreset)
{
	LinkLayerTest t; t.link.OnLowerLayerUp();
	ByteStr bs(250, 0);
	t.link.ConfirmedUserData(false, false, 1, 1024, bs.ToReadOnly());
	BOOST_REQUIRE(t.upper.IsBufferEmpty());
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNEXPECTED_FRAME);
}

// Secondary Reset Links
BOOST_AUTO_TEST_CASE(SecondaryResetLink)
{
	LinkLayerTest t(LinkLayerTest::DefaultConfig(), LogLevel::Interpret, true);
	t.link.OnLowerLayerUp();
	t.link.ResetLinkStates(false, 1, 1024);
	LinkFrame f; f.FormatAck(true, false, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
}

BOOST_AUTO_TEST_CASE(SecAckWrongFCB)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg);
	t.link.OnLowerLayerUp();
	t.link.ResetLinkStates(false, 1, 1024);

	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);

	ByteStr b(250, 0);
	t.link.ConfirmedUserData(false, false, 1, 1024, b.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	LinkFrame f; f.FormatAck(true, false, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
	BOOST_REQUIRE(t.upper.IsBufferEmpty()); //data should not be passed up!
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_WRONG_FCB_ON_RECEIVE_DATA);
}

// When we get another reset links when we're already reset,
// ACK it and reset the link state
BOOST_AUTO_TEST_CASE(SecondaryResetResetLinkStates)
{
	LinkLayerTest t;
	t.link.OnLowerLayerUp();
	t.link.ResetLinkStates(false, 1, 1024);

	t.link.ResetLinkStates(false, 1, 1024);
	LinkFrame f; f.FormatAck(true, false, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
}

BOOST_AUTO_TEST_CASE(SecondaryResetConfirmedUserData)
{
	LinkLayerTest t;
	t.link.OnLowerLayerUp();
	t.link.ResetLinkStates(false, 1, 1024);

	ByteStr bytes(250, 0);
	t.link.ConfirmedUserData(false, true, 1, 1024, bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	BOOST_REQUIRE(t.upper.BufferEquals(bytes, bytes.Size()));
	BOOST_REQUIRE(t.log.IsLogErrorFree());
	t.upper.ClearBuffer();

	t.link.ConfirmedUserData(false, true, 1, 1024, bytes.ToReadOnly()); //send with wrong FCB
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3); //should still get an ACK
	BOOST_REQUIRE(t.upper.IsBufferEmpty()); //but no data
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_WRONG_FCB_ON_RECEIVE_DATA);
}

BOOST_AUTO_TEST_CASE(RequestStatusOfLink)
{
	LinkLayerTest t;
	t.link.OnLowerLayerUp();
	t.link.RequestLinkStatus(false, 1, 1024); //should be able to request this before the link is reset
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);
	LinkFrame f; f.FormatLinkStatus(true, false, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
	t.link.ResetLinkStates(false, 1, 1024);


	t.link.RequestLinkStatus(false, 1, 1024); //should be able to request this before the link is reset
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
}

BOOST_AUTO_TEST_CASE(TestLinkStates)
{
	LinkLayerTest t;
	t.link.OnLowerLayerUp();
	t.link.TestLinkStatus(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_UNEXPECTED_FRAME);

	t.link.ResetLinkStates(false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);

	t.link.TestLinkStatus(false, true, 1, 1024);
	LinkFrame f; f.FormatAck(true, false, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
}

BOOST_AUTO_TEST_CASE(SendUnconfirmed)
{
	LinkLayerTest t;
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);

	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);
	LinkFrame f; f.FormatUnconfirmedUserData(true, 1024, 1, bytes, bytes.Size());
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
	BOOST_REQUIRE_EQUAL(t.upper.GetState().mSuccessCnt, 1);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);
}

BOOST_AUTO_TEST_CASE(CloseBehavior)
{
	LinkLayerTest t;
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE(t.upper.CountersEqual(1, 0));
	t.link.OnLowerLayerDown(); //take it down during the middle of a send
	BOOST_REQUIRE_FALSE(t.upper.IsLowerLayerUp());


	t.link.OnLowerLayerUp();
	BOOST_REQUIRE(t.upper.IsLowerLayerUp());
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);

}

BOOST_AUTO_TEST_CASE(ResetLinkTimerExpiration)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg);
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);
	LinkFrame f; f.FormatResetLinkStates(true, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
	BOOST_REQUIRE(t.upper.CountersEqual(0, 0));

	BOOST_REQUIRE(t.log.IsLogErrorFree());
	BOOST_REQUIRE(t.mts.DispatchOne()); //trigger the timeout callback
	BOOST_REQUIRE(t.upper.CountersEqual(0, 1));
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_NO_RETRY);
}

BOOST_AUTO_TEST_CASE(ResetLinkTimerExpirationWithRetry)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.NumRetry = 1;
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg);
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);

	BOOST_REQUIRE(t.log.IsLogErrorFree());
	BOOST_REQUIRE(t.mts.DispatchOne());
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_RETRY);
	BOOST_REQUIRE(t.upper.CountersEqual(0, 0)); //check that the send is still occuring
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	LinkFrame f; f.FormatResetLinkStates(true, 1024, 1);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f); // check that reset links got sent again

	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);
	f.FormatConfirmedUserData(true, true, 1024, 1, bytes, bytes.Size());
	BOOST_REQUIRE_EQUAL(t.mLastSend, f); // check that reset links got sent again

	BOOST_REQUIRE(t.mts.DispatchOne()); //timeout the ACK
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_NO_RETRY);
	BOOST_REQUIRE(t.upper.CountersEqual(0, 1));

	// Test retry reset
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 4);

	BOOST_REQUIRE(t.log.IsLogErrorFree());
	BOOST_REQUIRE(t.mts.DispatchOne());
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_RETRY);
	BOOST_REQUIRE(t.upper.CountersEqual(0, 1)); //check that the send is still occuring
}
BOOST_AUTO_TEST_CASE(ResetLinkTimerExpirationWithRetryResetState)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.NumRetry = 1;
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg);
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	t.link.Ack(false, false, 1, 1024);
	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE(t.upper.CountersEqual(1, 0));

	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);

	BOOST_REQUIRE(t.log.IsLogErrorFree());
	BOOST_REQUIRE(t.mts.DispatchOne());
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_RETRY);
	BOOST_REQUIRE(t.upper.CountersEqual(1, 0)); //check that the send is still occuring
	BOOST_REQUIRE_EQUAL(t.mNumSend, 4);

	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE(t.upper.CountersEqual(2, 0));

	// Test retry reset
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 5);	// Should now be waiting for an ACK with active timer

	BOOST_REQUIRE(t.log.IsLogErrorFree());
	BOOST_REQUIRE(t.mts.DispatchOne());
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_RETRY);
	BOOST_REQUIRE(t.upper.CountersEqual(2, 0)); //check that the send is still occuring
}

BOOST_AUTO_TEST_CASE(ConfirmedDataRetry)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.NumRetry = 1;
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg); t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1); // Should now be waiting for an ACK with active timer

	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);

	BOOST_REQUIRE(t.mts.DispatchOne()); //timeout the ConfData, check that it retransmits
	BOOST_REQUIRE_EQUAL(t.log.NextErrorCode(), DLERR_TIMEOUT_RETRY);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);
	LinkFrame f; f.FormatConfirmedUserData(true, true, 1024, 1, bytes, bytes.Size());
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);

	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);
	BOOST_REQUIRE(t.upper.CountersEqual(1, 0));
}

BOOST_AUTO_TEST_CASE(ResetLinkRetries)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.NumRetry = 3;
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg); t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	for(int i = 1; i < 5; ++i) {
		BOOST_REQUIRE_EQUAL(t.mNumSend, i); // sends link retry
		LinkFrame f;
		f.FormatResetLinkStates(true, 1024, 1);
		BOOST_REQUIRE_EQUAL(f, t.mLastSend);
		BOOST_REQUIRE(t.mts.DispatchOne()); //timeout
	}
	BOOST_REQUIRE_EQUAL(t.mNumSend, 4);
}

BOOST_AUTO_TEST_CASE(ConfirmedDataNackDFCClear)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.NumRetry = 1;
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg); t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1); // Should now be waiting for an ACK with active timer

	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);  // num transmitting confirmed data

	t.link.Nack(false, false, 1, 1024);  // test that we try to reset the link again
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);
	BOOST_REQUIRE(t.mLastSend.GetFunc() == LinkFunction::PRI_RESET_LINK_STATES);
	t.link.Ack(false, false, 1, 1024); // ACK the link reset
	BOOST_REQUIRE_EQUAL(t.mNumSend, 4);
	BOOST_REQUIRE(t.mLastSend.GetFunc() == LinkFunction::PRI_CONFIRMED_USER_DATA);
}

BOOST_AUTO_TEST_CASE(SendDataTimerExpiration)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg);
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 1);
	t.link.Ack(false, false, 1, 1024); // ACK the reset links
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	LinkFrame f; f.FormatConfirmedUserData(true, true, 1024, 1, bytes, bytes.Size());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 2);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f); // check that data was sent
	BOOST_REQUIRE(t.mts.DispatchOne()); //trigger the timeout callback
	BOOST_REQUIRE(t.upper.CountersEqual(0, 1));
}

BOOST_AUTO_TEST_CASE(SendDataSuccess)
{
	LinkConfig cfg = LinkLayerTest::DefaultConfig();
	cfg.UseConfirms = true;

	LinkLayerTest t(cfg);
	t.link.OnLowerLayerUp();

	ByteStr bytes(250, 0);
	t.link.Send(bytes.ToReadOnly()); // Should now be waiting for an ACK
	t.link.Ack(false, false, 1, 1024); //this

	t.link.Ack(false, false, 1, 1024);
	BOOST_REQUIRE(t.upper.CountersEqual(1, 0));

	t.link.Send(bytes.ToReadOnly()); // now we should be directly sending w/o having to reset, and the FCB should flip
	LinkFrame f; f.FormatConfirmedUserData(true, false, 1024, 1, bytes, bytes.Size());
	BOOST_REQUIRE_EQUAL(t.mNumSend, 3);
	BOOST_REQUIRE_EQUAL(t.mLastSend, f);
}

BOOST_AUTO_TEST_SUITE_END()