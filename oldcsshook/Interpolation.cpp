#include "Main.h"

typedef bool(__thiscall* Interpolate_t)(void*, float);
typedef void(__thiscall* Interpolation_t)(void*, float, float);
typedef void(__thiscall* SetInterpolationAmount_t)(void*, float);
typedef int(__thiscall* BaseInterpolatePart1_t)(void*, float, Vector, QAngle, int);

Interpolate_t Interpolate;
Interpolation_t Interpolation;
BaseInterpolatePart1_t BaseInterpolatePart1;
SetInterpolationAmount_t SetInterpolationAmount;
//NOTE: not needed
bool __fastcall Hooked_Interpolate(void* ptr, int edx, float currentTime)
{
	return Interpolate(ptr, currentTime);
}
void __fastcall Hooked_SetInterpolationAmount(void* ptr, int edx, float seconds)
{
	SetInterpolationAmount(ptr, -(g_pGlobals->interval_per_tick));
}

//NOTE: needed
void __fastcall Hooked_ClientInterpolation(void* ptr, int edx, float currentTime, float interpolation_amount)
{
	Interpolation(ptr, currentTime, -(g_pGlobals->interval_per_tick));
}
//NOTE: correct offset of baseinterpolatepart1 0x00046240 - 0x10000
//NOTE: it wont work cuz its not static function, u cant hook it, traitcore gop evire and i cant do this
int __fastcall Hooked_BaseInterpolatePart1(void* thisptr, int edx, float& currentTime, Vector& oldOrigin, QAngle& oldAngles, int& bNoMoreChanges)
{
	BaseEntity* thisent = (BaseEntity*)thisptr;
	return 0;
	BaseInterpolatePart1( thisptr, currentTime, oldOrigin, oldAngles, bNoMoreChanges ); //this is BaseInterpolatePart2 but not correct
}

void ClientInterpolation(void)
{
	//Interpolate = ( Interpolate_t ) g_Detour.DetourFunction( ( ( DWORD ) BASE_CLIENT + 0x4C250 ), Hooked_Interpolate ); // server interpolation may crash game
	Interpolation = (Interpolation_t)g_Detour.DetourFunction(((DWORD)BASE_CLIENT + 0x39D40), Hooked_ClientInterpolation);	// client interpolation shitty
	BaseInterpolatePart1 = (BaseInterpolatePart1_t)g_Detour.DetourFunction(((DWORD)BASE_CLIENT + 0x47170), Hooked_BaseInterpolatePart1); // p100 fix of interpolation
	//SetInterpolationAmount = ( SetInterpolationAmount_t ) g_Detour.DetourFunction( ( ( DWORD ) BASE_CLIENT + 0x34670 ), Hooked_SetInterpolationAmount );
}
//TODO: find a way how to make BaseInterpolatePart1 work