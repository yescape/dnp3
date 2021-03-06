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

#include "AssignClassTask.h"

#include "opendnp3/master/IMasterApplication.h"

using namespace openpal;

namespace opendnp3
{

AssignClassTask::AssignClassTask(IMasterApplication& application, openpal::TimeDuration retryPeriod_, openpal::Logger logger) :
	IMasterTask(application, 0, logger, nullptr, -1),	
	retryPeriod(retryPeriod_)
{}

void AssignClassTask::BuildRequest(APDURequest& request, uint8_t seq)
{
	request.SetControl(AppControlField(true, true, false, false, seq));
	request.SetFunction(FunctionCode::ASSIGN_CLASS);
	auto writer = request.GetWriter();
	pApplication->ConfigureAssignClassRequest(writer);
}

bool AssignClassTask::IsEnabled() const
{
	return pApplication->AssignClassDuringStartup();
}

IMasterTask::ResponseResult AssignClassTask::_OnResponse(const opendnp3::APDUResponseHeader& header, const openpal::ReadBufferView& objects)
{	
	return ValidateNullResponse(header, objects) ? ResponseResult::OK_FINAL : ResponseResult::ERROR_BAD_RESPONSE;
}
	
void AssignClassTask::_OnLowerLayerClose(openpal::MonotonicTimestamp)
{
	expiration = 0;
}
 
void AssignClassTask::_OnResponseTimeout(openpal::MonotonicTimestamp now)
{
	expiration = now.Add(retryPeriod);
}

void AssignClassTask::OnResponseOK(openpal::MonotonicTimestamp now)
{
	expiration = MonotonicTimestamp::Max();
}

void AssignClassTask::OnResponseError(openpal::MonotonicTimestamp now)
{
	expiration = MonotonicTimestamp::Max();
}

} //end ns

