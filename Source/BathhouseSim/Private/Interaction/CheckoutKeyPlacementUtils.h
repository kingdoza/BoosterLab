#pragma once

class ABathhouseCounterActor;
class ABathhouseKeyActor;

namespace BathhouseCheckoutKeyPlacement
{
	bool TryPlaceKeyInFreeWorld(ABathhouseKeyActor& Key, const ABathhouseCounterActor& Counter);
}
