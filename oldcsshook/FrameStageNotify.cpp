#include "Main.h"
#define clamp(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))


int shotsfired[64], shotsfired_old[64], angleindex[64][3];
float angle[64][3], angle_old[64][3];
float flResAngle[64];
int bestrecord[64];

float LerpTime()
{
	static ConVar* cvar_cl_interp = g_pCvar->FindVar( /*cl_interp*/XorStr<0x24, 10, 0x7F9B18B0>("\x47\x49\x79\x4E\x46\x5D\x4F\x59\x5C" + 0x7F9B18B0).s);
	static ConVar* cvar_cl_updaterate = g_pCvar->FindVar( /*cl_updaterate*/XorStr<0xC6, 14, 0xD9FFF99F>("\xA5\xAB\x97\xBC\xBA\xAF\xAD\xB9\xAB\xBD\xB1\xA5\xB7" + 0xD9FFF99F).s);
	static ConVar* cvar_cl_interp_ratio = g_pCvar->FindVar( /*cl_interp_ratio*/XorStr<0xBF, 16, 0x298E884B>("\xDC\xAC\x9E\xAB\xAD\xB0\xA0\xB4\xB7\x97\xBB\xAB\xBF\xA5\xA2" + 0x298E884B).s);
	static ConVar* cvar_sv_minupdaterate = g_pCvar->FindVar( /*sv_minupdaterate*/XorStr<0xF6, 17, 0x99ECD573>("\x85\x81\xA7\x94\x93\x95\x89\x8D\x9A\x9E\x74\x64\x70\x62\x70\x60" + 0x99ECD573).s);
	static ConVar* cvar_sv_maxupdaterate = g_pCvar->FindVar( /*sv_maxupdaterate*/XorStr<0x6A, 17, 0x6C62999F>("\x19\x1D\x33\x00\x0F\x17\x05\x01\x16\x12\x00\x10\x04\x16\x0C\x1C" + 0x6C62999F).s);
	static ConVar* cvar_sv_client_min_interp_ratio = g_pCvar->FindVar( /*sv_client_min_interp_ratio*/XorStr<0x9B, 27, 0x783554E3>("\xE8\xEA\xC2\xFD\xF3\xC9\xC4\xCC\xD7\xFB\xC8\xCF\xC9\xF7\xC0\xC4\xDF\xC9\xDF\xDE\xF0\xC2\xD0\xC6\xDA\xDB" + 0x783554E3).s);
	static ConVar* cvar_sv_client_max_interp_ratio = g_pCvar->FindVar( /*sv_client_max_interp_ratio*/XorStr<0xAF, 27, 0xED44D950>("\xDC\xC6\xEE\xD1\xDF\xDD\xD0\xD8\xC3\xE7\xD4\xDB\xC3\xE3\xD4\xD0\xCB\xA5\xB3\xB2\x9C\xB6\xA4\xB2\xAE\xA7" + 0xED44D950).s);

	float lerp = cvar_cl_interp->GetFloat();
	int cl_updaterate = cvar_cl_updaterate->GetInt(),
		sv_maxupdaterate = cvar_sv_maxupdaterate->GetInt(),
		sv_minupdaterate = cvar_sv_minupdaterate->GetInt(),
		cl_interp_ratio = cvar_cl_interp_ratio->GetInt(),
		sv_client_min_interp_ratio = cvar_sv_client_min_interp_ratio->GetInt(),
		sv_client_max_interp_ratio = cvar_sv_client_max_interp_ratio->GetInt();

	if (cvar_sv_minupdaterate && cvar_sv_maxupdaterate) cl_updaterate = sv_maxupdaterate;

	if (cl_interp_ratio == 0.f) cl_interp_ratio = 1.f;
	if (cvar_sv_client_min_interp_ratio && cvar_sv_client_max_interp_ratio && sv_client_min_interp_ratio != 1.f) cl_interp_ratio = clamp(cl_interp_ratio, sv_client_min_interp_ratio, sv_client_max_interp_ratio);

	return max(lerp, (cl_interp_ratio / cl_updaterate));
}

int ServerTick()
{
	float out, in;
	int servertick;

	in = g_pEngineClient->GetNetChannelInfo()->GetLatency(1);
	out = g_pEngineClient->GetNetChannelInfo()->GetLatency(0);

	servertick = g_pGlobals->tickcount + 1 + TIME_TO_TICKS(in + out);

	return servertick;
}


Vector get_hitbox(int iHitbox, BasePlayer* Entity)
{
	matrix3x4_t matrix[128];
	if (!(Entity->SetupBones(matrix, 128, 0x100, g_pGlobals->curtime))) return Vector(0, 0, 0);
	void* pModel = Entity->GetModel();
	if (!pModel) return Vector(0, 0, 0);
	studiohdr_t* studiohdr = g_pModelInfo->GetStudiomodel(pModel);
	mstudiohitboxset_t* studiohitboxset = studiohdr->pHitboxSet(Entity->m_nHitboxSet());
	if (!studiohitboxset) return Vector(0, 0, 0);
	mstudiobbox_t* studiobbox = studiohitboxset->pHitbox(iHitbox);
	if (!studiobbox) return Vector(0, 0, 0);
	Vector points = ((studiobbox->bbmin + studiobbox->bbmax) * .5f), ret;
	VectorTransform(points, matrix[studiobbox->bone], ret);
	return ret;
}

bool CheckDelta(CTickRecord* record)
{
	float out, dt, correct;

	out = g_pEngineClient->GetNetChannelInfo()->GetLatency(0);

	correct = out + TICKS_TO_TIME(TIME_TO_TICKS(LerpTime()));
	correct = clamp(correct, 0.f, 1.f);

	/*
		auto PredictedCmdArrivalTick = cmdTickcount + 1 + TIME_TO_TICKS(netChannelInfo->GetAvgLatency(FLOW_INCOMING) + netChannelInfo->GetAvgLatency(FLOW_OUTGOING));
		auto Correct = clamp(lerpTime + netChannelInfo->GetLatency(FLOW_OUTGOING), 0.f, 1.f) - TICKS_TO_TIME(PredictedCmdArrivalTick + TIME_TO_TICKS(lerpTime) - GetTickcountForEntity(tickrecord.m_SimulationTime));
	*/

	int entitytick = TIME_TO_TICKS(record->m_SimulationTime) + TIME_TO_TICKS(LerpTime());
	dt = correct - TICKS_TO_TIME(ServerTick() + TIME_TO_TICKS(LerpTime()) - entitytick);

	return (fabs(dt) <= 0.2f);
}

bool is_visible(BaseEntity* LocalPlayer, CSWeapon* Weapon, BasePlayer* Entity, Vector& vecAbsStart, Vector& vecAbsEnd)
{
	if (!LocalPlayer) return false;
	if (!Weapon) return false;

	trace_t Trace;
	Ray_t Ray;
	TraceFilterSkipTwoEntities TraceFilter(Entity, 0);

	Ray.Init(vecAbsStart, vecAbsEnd);
	g_pEngineTrace->TraceRay(Ray, 0x46004003, (ITraceFilter*)&TraceFilter, &Trace);
	return (Trace.fraction == 1.f);
}

int GetBestRecord(BasePlayer* LocalPlayer, CSWeapon* Weapon, BasePlayer* Entity)
{
	int i = Entity->entindex();
	static int best_record[64];
	static int maxdmg[64];

	for (int k = 16; --k; k >= 0)
	{
		CTickRecord backup;
		CTickRecord* record = &pPlayerHistory[i][k];
		CTickRecord* prevrecord = &pPlayerHistory[i][k + 1];

		if (!CheckDelta(record))
		{
			best_record[i] = 0;
			break;
		}

		if ((record->m_Origin - prevrecord->m_Origin).LengthSqr() > 4096.f)
		{
			best_record[i] = 0;
			break;
		}

		g_Stuff.StoreTickRecord(Entity, &backup);
		g_Stuff.ApplyTickRecord(Entity, record);

		Vector dir = get_hitbox(g_CVars.Aimbot.Hitbox, Entity);
		int temp = INT_MAX, rate;
		rate = LocalPlayer->GetAbsOrigin().DistTo(Entity->GetAbsOrigin());

		if (rate <= temp)
		{
			if (is_visible(LocalPlayer, Weapon, Entity, EyePosition, dir))
			{
				best_record[i] = k;
				temp = rate;
			}
		}

		g_Stuff.ApplyTickRecord(Entity, &backup);
	}

	return best_record[i];
}

static float BruteAngles[] = { 45.f, -45.f, -65.f, 65.f };
float SelectAngle(BasePlayer* Entity, int flag)
{
	int i = Entity->entindex();

	static int old[64];

	if (old[i] > g_iBulletsFired[i]) old[i] = 0;

	float delta = g_Stuff.GuwopNormalize(Entity->m_angEyeAngles().y - g_CVars.PlayerList.ViewAngles[i].y);
	float bodyyawdelta = *(float*)((DWORD)Entity + 0x518 + (0x4 * 2));

	bool twoshots = ((g_iBulletsFired[i] - old[i]) >= 1);

	if (g_iBulletsFired[i] >= 1)
	{
		if (twoshots)
		{
			angleindex[i][flag]++;
			if (angleindex[i][flag] > 3) angleindex[i][flag] = 0;
			angle[i][flag] = BruteAngles[angleindex[i][flag]];
			angle_old[i][flag] = angle[i][flag];

			old[i] = g_iBulletsFired[i];
		}
		else angle[i][flag] = angle_old[i][flag];
	}
	else angle[i][flag] = angle_old[i][flag];

	return angle[i][flag];
}

void Think(BasePlayer* Entity)
{
	int i = Entity->entindex();
	int best_record = bestrecord[i];
	CTickRecord* current = &pPlayerHistory[i][best_record];
	CTickRecord* previous = &pPlayerHistory[i][best_record + 1];
	CTickRecord simulation = pSimulationData[i];

	float poseparam2 = -90.f + current->m_PoseParameter[2] * 180.f;
	float poseparam3 = -90.f + current->m_PoseParameter[3] * 180.f;
	float poseparam4 = -90.f + current->m_PoseParameter[4] * 180.f;

	float g_flLastBodyYaw = *(float*)((DWORD)0x243A2474);
	float m_flGoalFeetYaw = *(float*)((DWORD)Entity + 0xF80 + 0x20);


	for (int i = 0; i < 15; i++)
	{
		float weight = current->m_AnimationLayer[i].flWeight;
		float cycle = current->m_AnimationLayer[i].flCycle;

		//	if (weight == 0.f && cycle >= .97f) printconsole("yo");
	}


	float pitch = Entity->m_angEyeAngles().x;
	bool twitch = ((current->m_EyeAngles.x - previous->m_EyeAngles.x) > 40.f);
	bool lag = ((TIME_TO_TICKS(current->m_SimulationTime - previous->m_SimulationTime)) > 6);

	ShouldLogShots[i] = true;

	float temp = g_CVars.PlayerList.ViewAngles[i].y;
	static float temp_j[64], delta[64], angle, angletowardslocal[64];
	Vector eyedelta = Vector(EyePosition - Entity->EyePosition());
	QAngle aimangles;

	// --- calc delta for jitter ----------------------------------------------------------

	delta[i] = g_Stuff.GuwopNormalize(temp - temp_j[i]);
	delta[i] -= 180.f;
	if (delta[i] < 0.f) delta[i] = -delta[i];

	temp_j[i] = temp;

	// --- calc delta for static aa -------------------------------------------------------

	//stuff
	auto f = g_pGlobals->maxClients; f >= 1; --f;
	//
	float  deltayaw;
	float bodyyawdelta = *(float*)((DWORD)Entity + 0x518 + (0x4 * 2));
	deltayaw = Entity->m_angEyeAngles().y;
	int true_side = (deltayaw >= -1.f && deltayaw <= 1.f) ? 30 : -30;
	float backward = Entity->m_angEyeAngles().y += 180;


	int pidor = g_iBulletsFired[Entity->entindex()] & 1;
	switch (pidor)
	{
	case 0: Entity->m_angEyeAngles().y += true_side;
	case 1: Entity->m_angEyeAngles().y -= true_side;
	}

	// --- check for movement types -------------------------------------------------------

	int k = 0;

	if (Entity->m_fFlags() & FL_ONGROUND)
	{
		if (Entity->m_vecVelocity().Length2D() < 30)
		{
			//if (EdgeDetect(Entity) && Entity->m_vecVelocity().Length() < 300.f) k = 5;
			//else
			k = (Entity->m_fFlags() & FL_DUCKING) ? 1 : 2;
		}
		else k = 3;
	}
	else k = 4;

	// ------------------------------------------------------------------------------------

	angle = SelectAngle(Entity, k); // other cases
	flResAngle[i] = angle;
	//if (fabsf(delta[i]) < 15.f) angle = 180.f; // jitter

	QAngle orig = QAngle(Entity->m_angEyeAngles().x, temp, Entity->m_angEyeAngles().z);
	QAngle final = QAngle(Entity->m_angEyeAngles().x, temp + angle, Entity->m_angEyeAngles().z);

	//current->m_EyeAngles.x = final.y;
	simulation.m_EyeAngles.y = final.y;
	Entity->m_angEyeAngles().y = final.y;
	//Entity->SetAbsAngles(final);
	//Entity->SetLocalAngles(final);

	//Entity->UpdateClientSideAnimation();

	//Entity->m_angEyeAngles().y = temp;
	//Entity->SetAbsAngles(orig);
	//Entity->SetLocalAngles(orig);
	//Entity->UpdateClientSideAnimation();
}

// ====================================
// i dont use that 
void animation_fix(BasePlayer* local_player)
{
	if (g_pEngineClient->IsInGame() && g_pEngineClient->IsConnected())
	{
		//CCSPlayerAnimState* animation_state = local_player->m_PlayerAnimState();
		float m_flGoalFeetYaw = *(float*)((DWORD)local_player + 0xF80 + 0x20);

		if (!local_player)
			return;

		local_player->m_bClientSideAnimation() = true;

		float old_curtime = g_pGlobals->curtime;
		float old_frametime = g_pGlobals->frametime;

		static ConVar* host_timescale = g_pCvar->FindVar("host_timescale");
		g_pGlobals->frametime = g_pGlobals->interval_per_tick * host_timescale->GetFloat();
		g_pGlobals->curtime = local_player->m_flSimulationTime() + g_pGlobals->interval_per_tick;

		local_player->UpdateClientSideAnimation();

		g_pGlobals->curtime = old_curtime;
		g_pGlobals->frametime = old_frametime;

		local_player->SetLocalAngles(QAngle(0.f, m_flGoalFeetYaw, 0.f));

		local_player->m_bClientSideAnimation() = false;
	}
}

float getspecialangle(float angle)
{
	return ((angle * angle) * 1.5f * 3.1415926535897932384626433832795f * 3.0f / angle);
}

float yawdelta(float value)
{
	float yawdelta = ((value * value) * 1.5 * 3.1415926535897932384626433832795 / value);

	return yawdelta;
}

typedef void(__thiscall* FrameStageNotify_t)(void*, ClientFrameStage_t);
void __fastcall Hooked_FrameStageNotify(void* ecx, void* edx, ClientFrameStage_t curStage)
{
	BasePlayer* LocalPlayer = (BasePlayer*)g_pClientEntityList->GetClientEntity(g_pEngineClient->GetLocalPlayer());
	QAngle* pPunchAngle;
	QAngle PunchAngle, PunchAngle_old;

	if (g_pEngineClient->IsInGame())
	{
		if (curStage == FRAME_UNDEFINED) return;
	}

	if (curStage == FRAME_RENDER_START)
	{
		if (LocalPlayer && LocalPlayer->m_lifeState() == 0)
		{
			static bool boole = false;
			if (GetAsyncKeyState(4) & 1)
				boole = !boole;

			if (boole)
			{
				*(float*)((DWORD)LocalPlayer + 0xD14) = g_qThirdPerson.y;
				g_pEngineClient->ExecuteClientCmd("thirdperson");
			}
			else
				g_pEngineClient->ExecuteClientCmd("firstperson");
		}
	//	animation_fix(LocalPlayer);
	}


	CreateMoveVMT->Function< FrameStageNotify_t >(32)(ecx, curStage);

	if (!g_pEngineClient->IsInGame()) return;

	static float tempYaw[64];

	if (curStage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{
		for (auto Index = g_pGlobals->maxClients; Index >= 1; --Index)
		{
			BasePlayer* Entity = (BasePlayer*)g_pClientEntityList->GetClientEntity(Index);
			if (Entity == 0) continue;
			if (Index == g_pEngineClient->GetLocalPlayer()) continue;
			if (Entity->m_lifeState() != 0) continue;
			if (Entity->m_iHealth() > 0 && Entity->m_iHealth() < 500);
			if (!g_CVars.Aimbot.FriendlyFire);
			{
				if (Entity->m_iTeamNum() == LocalPlayer->m_iTeamNum()) continue;
			}
			if (Entity->IsDormant()) continue;

			if (!g_Whitelist.List(Index) && g_CVars.Aimbot.Resolver.Active)
			{
				tempYaw[Index] = g_CVars.PlayerList.ViewAngles[Index].y;

				bool ret = true;
				if (g_CVars.Aimbot.Resolver.Mode == 1 && g_CVars.PlayerList.Yaw[Index] != 1) ret = false;

				// todo: not random, adaptive side resolvers
				// those below suck dick

				if (ret)
				{
					if (g_CVars.Aimbot.Resolver.Smart)
					{
						Vector resultLocal = EyePosition;
						Vector resultentity = Entity->EyePosition();
						Vector m_vTraceVector = Vector(resultLocal - resultentity);
						QAngle m_vAimAngles;
						static float yawDelta[64];
						static int yawMode[64];

						if (!resultLocal.IsValid() || !resultentity.IsValid() || !m_vTraceVector.IsValid()) continue;

						VectorAngles(m_vTraceVector, m_vAimAngles);
						m_vAimAngles.x *= -1;

						if (!m_vAimAngles.IsValid()) continue;

						yawDelta[Index] = m_vAimAngles.y - tempYaw[Index];
						yawDelta[Index] = g_Stuff.GuwopNormalize(yawDelta[Index]);
						if (yawDelta[Index] < 0.f) yawDelta[Index] += 360.f;

						if (yawDelta[Index] <= 20.f || yawDelta[Index] >= 340.f) yawMode[Index] = 1;
						else if ((yawDelta[Index] >= 70.f && yawDelta[Index] <= 110.f) || (yawDelta[Index] >= 250.f && yawDelta[Index] <= 290.f)) yawMode[Index] = 2;
						else if (yawDelta[Index] >= 160.f && yawDelta[Index] <= 200.f) yawMode[Index] = 3;
						else yawMode[Index] = 0;

						if (yawMode[Index] == 2) g_CVars.Aimbot.AutoHeightMode[Index] = 1;
						else g_CVars.Aimbot.AutoHeightMode[Index] = 0;

						if ((g_CVars.PlayerList.ViewAngles[Index].x == 89.f || g_CVars.PlayerList.ViewAngles[Index].x == -89.f) && yawMode[Index] != 2)
						{
							if (g_CVars.Aimbot.Resolver.Type == 0)
							{
								Entity->m_angEyeAngles().y = Entity->m_angEyeAngles().y + ((g_iBulletsFired[Entity->entindex()] % 9) * 45.f);
							}
							else if (g_CVars.Aimbot.Resolver.Type == 1)
							{
								float TmpDistance = 999999.f;
								QAngle tmp;

								Vector dst = Entity->EyePosition();

								float Distance = (dst - EyePosition).Length();
								if (Distance < TmpDistance)
								{
									TmpDistance = Distance;
									g_Stuff.CalculateAngles(EyePosition, dst, tmp);
									Normalize(tmp);
									Entity->m_angEyeAngles().y = tmp.y;
								}

								//Entity->m_angEyeAngles().y = 180.f;
							}
							else if (g_CVars.Aimbot.Resolver.Type == 2)//HFH
							{
								float yawDelta;
								yawDelta = Entity->m_angEyeAngles().y;
								Entity->m_angEyeAngles().y = yawDelta * ((g_iBulletsFired[Entity->entindex()] % 2 - 9) * 45.f);

								switch (g_iGameTicks % 4)
								{
								case 0: Entity->m_angEyeAngles().y = yawDelta; break;
								case 1: Entity->m_angEyeAngles().y = 90.f; break;
								case 2: Entity->m_angEyeAngles().y = 180.f; break;
								case 3: Entity->m_angEyeAngles().y = yawDelta; break;
								}

							}
							else if (g_CVars.Aimbot.Resolver.Type == 3)//jitma
							{
								//stuff
								auto i = g_pGlobals->maxClients; i >= 1; --i;
								//
								float  deltayaw;
								float  bodydelta;
								bodydelta = g_Stuff.GuwopNormalize(Entity->m_angEyeAngles().y - g_CVars.PlayerList.ViewAngles[i].y / 1.5);
								deltayaw = Entity->m_angEyeAngles().y;
								int true_side = (deltayaw >= -1.f && deltayaw <= 1.f) ? g_CVars.Aimbot.Resolver.angle : g_CVars.Aimbot.Resolver.angle;
								float backward = Entity->m_angEyeAngles().y += 180;


								int pidor = g_iGameTicks & 2;
								switch (pidor)
								{
								case 0: Entity->m_angEyeAngles().y += true_side;
								case 1: Entity->m_angEyeAngles().y -= true_side;
								}
							}
							else if (g_CVars.Aimbot.Resolver.Type == 4)//v6
							{
								Think(Entity);
							}
							else if (g_CVars.Aimbot.Resolver.Type == 5)//v6
							{
								Think(Entity);
							}
						}
					}
					else
					{
						g_CVars.Aimbot.AutoHeightMode[Index] = 0;

						if (g_CVars.Aimbot.Resolver.Type == 0)
						{
							Entity->m_angEyeAngles().y = Entity->m_angEyeAngles().y + ((g_iBulletsFired[Entity->entindex()] % 9) * 45.f);
						}
						else if (g_CVars.Aimbot.Resolver.Type == 1)
						{
							float TmpDistance = 999999.f;
							QAngle tmp;

							Vector dst = Entity->EyePosition();

							float Distance = (dst - EyePosition).Length();
							if (Distance < TmpDistance)
							{
								TmpDistance = Distance;
								g_Stuff.CalculateAngles(EyePosition, dst, tmp);
								Normalize(tmp);
								Entity->m_angEyeAngles().y = tmp.y;
							}

							//Entity->m_angEyeAngles().y = 180.f;
						}
						else if (g_CVars.Aimbot.Resolver.Type == 2)
						{
							float yawDelta;
							yawDelta = Entity->m_angEyeAngles().y;
							Entity->m_angEyeAngles().y = yawDelta * ((g_iBulletsFired[Entity->entindex()] % 2 - 9) * 45.f);

							switch (g_iGameTicks % 4)
							{
							case 0: Entity->m_angEyeAngles().y = yawDelta; break;
							case 1: Entity->m_angEyeAngles().y = 90.f; break;
							case 2: Entity->m_angEyeAngles().y = 180.f; break;
							case 3: Entity->m_angEyeAngles().y = yawDelta; break;
							}

						}
						else if (g_CVars.Aimbot.Resolver.Type == 3)
						{
							//stuff
							auto i = g_pGlobals->maxClients; i >= 1; --i;
							//
							float  deltayaw;
							float  bodydelta;
							bodydelta = g_Stuff.GuwopNormalize(Entity->m_angEyeAngles().y - g_CVars.PlayerList.ViewAngles[i].y / 1.5);
							deltayaw = Entity->m_angEyeAngles().y;
							int true_side = (deltayaw >= -1.f && deltayaw <= 1.f) ? g_CVars.Aimbot.Resolver.angle : g_CVars.Aimbot.Resolver.angle;
							float backward = Entity->m_angEyeAngles().y += 180;


							int pidor = g_iGameTicks & 2;
							switch (pidor)
							{
							case 0: Entity->m_angEyeAngles().y += true_side;
							case 1: Entity->m_angEyeAngles().y -= true_side;
							}
						}
						else if (g_CVars.Aimbot.Resolver.Type == 4)
						{
							Think(Entity);
						}
					}
				}
			}

			if (pPlayerHistory[Index][0].m_SimulationTime != Entity->m_flSimulationTime())
			{
				
				if (g_CVars.Aimbot.Interpolation.PingReducer)
				{
					float ping = g_pEngineClient->GetNetChannelInfo()->GetLatency(0) * 1000;
					for (int tick = ping * g_pGlobals->interval_per_tick / TICKS_TO_TIME(14); tick > 0; tick--) pPlayerHistory[Index][tick] = pPlayerHistory[Index][tick - 1];
				}
				else
				{
					for (int tick = 25; tick > 0; tick--) pPlayerHistory[Index][tick] = pPlayerHistory[Index][tick - 1];
				}
				g_Stuff.StoreTickRecord(Entity, &pPlayerHistory[Index][0]);
			}
		}
	}
}