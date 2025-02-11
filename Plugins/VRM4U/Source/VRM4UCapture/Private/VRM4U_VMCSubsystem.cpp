// Fill out your copyright notice in the Description page of Project Settings.


#include "VRM4U_VMCSubsystem.h"

#include "Engine/Engine.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeLock.h"
#include "OSCManager.h"
#include "OSCServer.h"

void UVRM4U_VMCSubsystem::OSCReceivedMessageEvent(const FOSCMessage& Message, const FString& IPAddress, uint16 Port) {

	UVRM4U_VMCSubsystem* subsystem = GEngine->GetEngineSubsystem<UVRM4U_VMCSubsystem>();
	if (subsystem == nullptr) return;

	auto data = subsystem->FindOrAddVMCData(IPAddress, (int)Port);
	if (data == nullptr) return;

	FOSCAddress a = UOSCManager::GetOSCMessageAddress(Message);
	FString addressPath = UOSCManager::GetOSCAddressFullPath(a);

	TArray<FString> str;
	UOSCManager::GetAllStrings(Message, str);

	TArray<float> curve;
	UOSCManager::GetAllFloats(Message, curve);
	FTransform t;
	float f = 0;
	if (curve.Num() == 1) {
		f = curve[0];
	}
	if (curve.Num() >= 7) {
		t.SetLocation(FVector(-curve[0], curve[2], curve[1]) * 100.f);
		t.SetRotation(FQuat(-curve[3], curve[5], curve[4], curve[6]));
	}
	if (curve.Num() >= 10) {
		t.SetScale3D(FVector(curve[7], curve[9], curve[8]));
	}

	if (addressPath == TEXT("/VMC/Ext/Root/Pos")) {
		data->BoneData.FindOrAdd(str[0]) = t;
	}
	if (addressPath == TEXT("/VMC/Ext/Bone/Pos")) {
		data->BoneData.FindOrAdd(str[0]) = t;
	}
	if (addressPath == TEXT("/VMC/Ext/Blend/Val")) {
		data->CurveData.FindOrAdd(str[0]) = f;
	}

	if (addressPath == TEXT("/VMC/Ext/OK")) {
		FScopeLock lock(&subsystem->cs);
		subsystem->ServerDataList_Cache = subsystem->ServerDataList_Latest;
	}


	// 
	//GetAllFloats
}

bool UVRM4U_VMCSubsystem::CopyVMCData(FVMCData &data, FString serverName, int port) {
	for (auto& s : ServerDataList_Cache) {
		if (s.ServerAddress == serverName) {
			FScopeLock lock(&cs);
			data = s;
			return true;
		}
	}
	return false;
}
bool UVRM4U_VMCSubsystem::CopyVMCDataAll(FVMCData& data) {
	FScopeLock lock(&cs);
	for (auto& s : ServerDataList_Cache) {
		if (data.Port == 0) {
			data = s;
			continue;
		}
		data.BoneData.Append(s.BoneData);
		data.CurveData.Append(s.CurveData);
	}
	return true;
}

FVMCData* UVRM4U_VMCSubsystem::FindOrAddVMCData(FString serverName, int port) {
	for (auto& s : ServerDataList_Latest) {
		if (s.ServerAddress == serverName) {
			return &s;
		}
	}
	int ind = ServerDataList_Latest.AddDefaulted();
	ServerDataList_Latest[ind].ServerAddress = serverName;
	ServerDataList_Latest[ind].Port = port;
	return &ServerDataList_Latest[ind];
}

UOSCServer* UVRM4U_VMCSubsystem::FindOrAddServer(const FString ServerAddress, int port) {
	if (port == 0) {
		return nullptr;
	}
	int ServerListNo = -1;
	{
		auto t = OSCServerList.FindByPredicate([&ServerAddress, &port](const TStrongObjectPtr<UOSCServer> & a) {
			if (a->GetIpAddress(false) != ServerAddress) return false;
			if (a->GetPort() != port) return false;
			return true;
		});

		if (t) return t->Get();

	}
	if (ServerListNo < 0) {
		ServerListNo = OSCServerList.AddDefaulted();

		{
#if	UE_VERSION_OLDER_THAN(4,25,0)
#else
			OSCServerList[ServerListNo].Reset(UOSCManager::CreateOSCServer(ServerAddress, port, false, true, FString(), this));

			OSCServerList[ServerListNo]->OnOscMessageReceivedNative.RemoveAll(nullptr);
			OSCServerList[ServerListNo]->OnOscMessageReceivedNative.AddStatic(&UVRM4U_VMCSubsystem::OSCReceivedMessageEvent);

#if WITH_EDITOR
			// Allow it to tick in editor, so that messages are parsed.
			// Only doing it upon creation so that the user can make it non-tickable if desired (and manage that thereafter).
			if (OSCServerList[ServerListNo])
			{
				OSCServerList[ServerListNo]->SetTickInEditor(true);
			}
#endif // WITH_EDITOR
#endif
		}
	}

	return OSCServerList[ServerListNo].Get();
}

void UVRM4U_VMCSubsystem::DestroyVMCServer(const FString ServerAddress, int port) {
	/*
	auto OSCServer = FindOrAddServerRef(ServerAddress, port);
	if (OSCServer.Get()) {
		OSCServer->Stop();
	}
	OSCServer = nullptr;
	*/
}


bool UVRM4U_VMCSubsystem::CreateVMCServer(const FString ServerAddress, int port) {

	decltype(auto) OSCServer = FindOrAddServer(ServerAddress, port);

	return true;
}

