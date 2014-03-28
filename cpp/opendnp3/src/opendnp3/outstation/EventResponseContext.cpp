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

#include "EventResponseContext.h"

#include <openpal/Serialization.h>

#include "opendnp3/objects/Group2.h"
#include "opendnp3/objects/Group22.h"
#include "opendnp3/objects/Group23.h"
#include "opendnp3/objects/Group32.h"

using namespace openpal;

namespace opendnp3
{

EventResponseContext::EventResponseContext(OutstationEventBuffer& buffer_) : buffer(buffer_)
{}

bool EventResponseContext::IsComplete() const
{
	return !criteria.HasSelection();
}

IINField EventResponseContext::ReadAll(const GroupVariationRecord& record)
{
	switch(record.enumeration)
	{
		// ----------- class polls --------------

		case(GroupVariation::Group60Var2) :
			criteria.AddToClass1(events::ALL_TYPES);			
			return IINField::Empty;		
		case(GroupVariation::Group60Var3) :
			criteria.AddToClass2(events::ALL_TYPES);
			return IINField::Empty;
		case(GroupVariation::Group60Var4) :
			criteria.AddToClass3(events::ALL_TYPES);
			return IINField::Empty;

		// ----------- variation 0 --------------

		case(GroupVariation::Group2Var0) :
			criteria.AddToAllClasses(events::BINARY);
			return IINField::Empty;
		case(GroupVariation::Group22Var0) :
			criteria.AddToAllClasses(events::COUNTER);
			return IINField::Empty;
		case(GroupVariation::Group23Var0) :
			criteria.AddToAllClasses(events::FROZEN_COUNTER);
			return IINField::Empty;
		case(GroupVariation::Group32Var0) :
			criteria.AddToAllClasses(events::ANALOG);
			return IINField::Empty;

		default:
			return IINField(IINBit::FUNC_NOT_SUPPORTED);
	}
}

void EventResponseContext::Reset()
{	
	criteria.Clear();
}

bool EventResponseContext::Load(ObjectWriter& writer)
{
	if (criteria.HasSelection())
	{
		auto iterator = buffer.SelectEvents(criteria);
		if (Iterate(writer, iterator))
		{
			criteria.Clear();
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

bool EventResponseContext::Iterate(ObjectWriter& writer, SelectionIterator& iterator)
{		
	auto current = iterator.SeekNext();

	while (current.HasValue())
	{
		switch (current.Get())
		{
			case(EventType::Binary) :
				if (!this->WriteFullHeader<Binary>(writer, iterator, Group2Var1Serializer::Inst()))
				{
					return false;
				}
				break;
			case(EventType::Counter) :
				if (!this->WriteFullHeader<Counter>(writer, iterator, Group22Var1Serializer::Inst()))
				{
					return false;
				}
				break;
			case(EventType::FrozenCounter) :
				if (!this->WriteFullHeader<FrozenCounter>(writer, iterator, Group23Var1Serializer::Inst()))
				{
					return false;
				}
				break;
			case(EventType::Analog) :
				if (!this->WriteFullHeader<Analog>(writer, iterator, Group32Var1Serializer::Inst()))
				{
					return false;
				}
				break;
			default:
				iterator.SeekNext();
				break;
		}

		current = iterator.Current();
	}

	return true;
}

}