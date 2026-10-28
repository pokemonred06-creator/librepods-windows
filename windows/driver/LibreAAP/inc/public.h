/*++

Copyright (c) Microsoft Corporation.  All rights reserved.
Copyright (c) LibrePods Project.

Module Name:

    public.h

Abstract:

    Contains GUID definitions used both by the LibreAAP driver and LibrePods application.

--*/

#pragma once

//
// Apple AirPods Accessory Protocol (AAP) L2CAP Service GUID
//
/* 74ec2172-0bad-4d01-8f77-997b2be0722a */
DEFINE_GUID(BTHECHOSAMPLE_SVC_GUID, 0x74ec2172, 0x0bad, 0x4d01, 0x8f, 0x77, 0x99, 0x7b, 0x2b, 0xe0, 0x72, 0x2a);
extern __declspec(selectany) const PWSTR BthEchoSampleSvcName = L"LibreAAPService";

//
// Device interface exposed by LibreAAP driver for user-mode Win32 access (CreateFileW)
//
/* 74ec2172-0bad-4d01-8f77-997b2be0722a */
DEFINE_GUID(BTHECHOSAMPLE_DEVICE_INTERFACE, 0x74ec2172, 0x0bad, 0x4d01, 0x8f, 0x77, 0x99, 0x7b, 0x2b, 0xe0, 0x72, 0x2a);
DEFINE_GUID(GUID_DEVINTERFACE_MAGICAAP, 0x74ec2172, 0x0bad, 0x4d01, 0x8f, 0x77, 0x99, 0x7b, 0x2b, 0xe0, 0x72, 0x2a);
