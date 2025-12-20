#include "XPGemVisualConfig.h"

FXPGemData UXPGemVisualConfig::GetVisualForValue(int32 Value) const
{
	if (const FXPGemData* Found = GemVisuals.Find(Value))
	{
		return *Found;
	}
	return DefaultVisual;
}
