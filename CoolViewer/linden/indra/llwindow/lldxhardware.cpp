/**
 * @file lldxhardware.cpp
 * @brief LLDXHardware implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifdef LL_WINDOWS

// Culled from some Microsoft sample code

#include "linden_common.h"

#define INITGUID
#include <dxdiag.h>
#undef INITGUID
#include <wbemidl.h>
#include <stdlib.h>

#include "boost/tokenizer.hpp"

#include "lldxhardware.h"

#include "llstring.h"
#include "llstl.h"

void (*gWriteDebug)(const char* msg) = NULL;
LLDXHardware gDXHardware;

//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)	   { if (p) { delete (p);	 (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#define SAFE_RELEASE(p)	  { if (p) { (p)->Release(); (p)=NULL; } }

typedef BOOL (WINAPI* PfnCoSetProxyBlanket)(IUnknown* pProxy,
											DWORD dwAuthnSvc,
											DWORD dwAuthzSvc,
											OLECHAR* pServerPrincName,
											DWORD dwAuthnLevel,
											DWORD dwImpLevel,
											RPC_AUTH_IDENTITY_HANDLE pAuthInfo,
											DWORD dwCapabilities);

HRESULT GetVideoMemoryViaWMI(WCHAR* strInputDeviceID, DWORD* pdwAdapterRam)
{
	HRESULT hr;
	bool got_memory = false;
	HRESULT hrCoInitialize = S_OK;
	IWbemLocator* pIWbemLocator = NULL;
	IWbemServices* pIWbemServices = NULL;
	BSTR pNamespace = NULL;

	*pdwAdapterRam = 0;
	hrCoInitialize = CoInitialize(NULL);

	hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
						  IID_IWbemLocator, (LPVOID*)&pIWbemLocator);
	if (SUCCEEDED(hr) && pIWbemLocator)
	{
		// Using the locator, connect to WMI in the given namespace.
		pNamespace = SysAllocString(L"\\\\.\\root\\cimv2");

		hr = pIWbemLocator->ConnectServer(pNamespace, NULL, NULL, 0L,
										  0L, NULL, NULL, &pIWbemServices);
		if (SUCCEEDED(hr) && pIWbemServices != 0)
		{
			HINSTANCE hinstOle32 = NULL;

			hinstOle32 = LoadLibraryW(L"ole32.dll");
			if (hinstOle32)
			{
				PfnCoSetProxyBlanket pfnCoSetProxyBlanket = NULL;
				pfnCoSetProxyBlanket = (PfnCoSetProxyBlanket)GetProcAddress(hinstOle32,
																			"CoSetProxyBlanket");
				if (pfnCoSetProxyBlanket != 0)
				{
					// Switch security level to IMPERSONATE.
					pfnCoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT,
										 RPC_C_AUTHZ_NONE, NULL,
										 RPC_C_AUTHN_LEVEL_CALL,
										 RPC_C_IMP_LEVEL_IMPERSONATE, NULL,
										 0);
				}
				FreeLibrary(hinstOle32);
			}

			IEnumWbemClassObject* pEnumVideoControllers = NULL;
			BSTR pClassName = NULL;
			pClassName = SysAllocString(L"Win32_VideoController");
			hr = pIWbemServices->CreateInstanceEnum(pClassName, 0, NULL,
													&pEnumVideoControllers);
			if (SUCCEEDED(hr) && pEnumVideoControllers)
			{
				IWbemClassObject* pVideoControllers[10] = { 0 };
				DWORD uReturned = 0;
				BSTR pPropName = NULL;

				// Get the first one in the list
				pEnumVideoControllers->Reset();
				hr = pEnumVideoControllers->Next(5000,	// timeout in 5 seconds
												 10,	// return the first 10
												 pVideoControllers,
												 &uReturned);
				VARIANT var;
				if (SUCCEEDED(hr))
				{
					bool found_one = false;
					for (UINT iController = 0; iController < uReturned;
						 ++iController)
					{
						if (!pVideoControllers[iController])
						{
							continue;
						}

						// If strInputDeviceID is set find this specific device
						// and return memory or specific device; if not set
						// return the best device.
						if (strInputDeviceID)
						{
							pPropName = SysAllocString(L"PNPDeviceID");
							hr = pVideoControllers[iController]->Get(pPropName,
																	 0L, &var,
																	 NULL,
																	 NULL);
							if (SUCCEEDED(hr) && strInputDeviceID &&
								wcsstr(var.bstrVal, strInputDeviceID) != 0)
							{
								found_one = true;
							}
							VariantClear(&var);
							if (pPropName)
							{
								SysFreeString(pPropName);
							}
						}

						if (found_one || !strInputDeviceID)
						{
							pPropName = SysAllocString(L"AdapterRAM");
							hr = pVideoControllers[iController]->Get(pPropName,
																	 0L, &var,
																	 NULL,
																	 NULL);
							if (SUCCEEDED(hr))
							{
								got_memory = true;
								*pdwAdapterRam = llmax(var.ulVal,
													   *pdwAdapterRam);
							}
							VariantClear(&var);
							if (pPropName)
							{
								SysFreeString(pPropName);
							}
						}

						SAFE_RELEASE(pVideoControllers[iController]);

						if (found_one)
						{
							break;
						}
					}
				}
			}

			if (pClassName)
			{
				SysFreeString(pClassName);
			}

			SAFE_RELEASE(pEnumVideoControllers);
		}

		if (pNamespace)
		{
			SysFreeString(pNamespace);
		}

		SAFE_RELEASE(pIWbemServices);
	}

	SAFE_RELEASE(pIWbemLocator);

	if (SUCCEEDED(hrCoInitialize))
	{
		CoUninitialize();
	}

	return got_memory ? S_OK : E_FAIL;
}

void get_wstring(IDxDiagContainer* containerp, WCHAR* wsz_prop_name,
				 WCHAR* wsz_prop_value, int outputSize)
{
	HRESULT hr;
	VARIANT var;

	VariantInit(&var);
	hr = containerp->GetProp(wsz_prop_name, &var);
	if (SUCCEEDED(hr))
	{
		// Switch off the type.  There's 4 different types:
		switch (var.vt)
		{
			case VT_UI4:
				swprintf(wsz_prop_value, L"%d", var.ulVal);
				break;
			case VT_I4:
				swprintf(wsz_prop_value, L"%d", var.lVal);
				break;
			case VT_BOOL:
				wcscpy(wsz_prop_value, var.boolVal ? L"true" : L"false");
				break;
			case VT_BSTR:
				wcsncpy(wsz_prop_value, var.bstrVal, outputSize - 1);
				wsz_prop_value[outputSize - 1] = 0;
				break;
		}
	}
	// Clear the variant (this is needed to free BSTR memory)
	VariantClear(&var);
}

std::string get_string(IDxDiagContainer* containerp, WCHAR* wsz_prop_name)
{
	WCHAR wsz_prop_value[256];
	get_wstring(containerp, wsz_prop_name, wsz_prop_value, 256);
	return ll_convert_wide_to_string(wsz_prop_value);
}

LLVersion::LLVersion()
{
	mValid = false;
	for (S32 i = 0; i < 4; ++i)
	{
		mFields[i] = 0;
	}
}

bool LLVersion::set(const std::string& version_string)
{
	for (S32 i = 0; i < 4; ++i)
	{
		mFields[i] = 0;
	}
	// Split the version string.
	std::string str(version_string);
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(".", "", boost::keep_empty_tokens);
	tokenizer tokens(str, sep);

	S32 count = 0;
	for (tokenizer::iterator iter = tokens.begin();
		 iter != tokens.end() && count < 4; ++iter)
	{
		mFields[count++] = atoi(iter->c_str());
	}
	if (count < 4)
	{
		for (S32 i = 0; i < 4; ++i)
		{
			mFields[i] = 0;
		}
		mValid = false;
	}
	else
	{
		mValid = true;
	}
	return mValid;
}

S32 LLVersion::getField(S32 field_num)
{
	return mValid ? mFields[field_num] : -1;
}

std::string LLDXDriverFile::dump()
{
	if (gWriteDebug)
	{
		gWriteDebug("Filename:");
		gWriteDebug(mName.c_str());
		gWriteDebug("\n");
		gWriteDebug("Ver:");
		gWriteDebug(mVersionString.c_str());
		gWriteDebug("\n");
		gWriteDebug("Date:");
		gWriteDebug(mDateString.c_str());
		gWriteDebug("\n");
	}
	llinfos << mFilepath << llendl;
	llinfos << mName << llendl;
	llinfos << mVersionString << llendl;
	llinfos << mDateString << llendl;

	return "";
}

LLDXDevice::~LLDXDevice()
{
	for_each(mDriverFiles.begin(), mDriverFiles.end(), DeletePairedPointer());
	mDriverFiles.clear();
}

std::string LLDXDevice::dump()
{
	if (gWriteDebug)
	{
		gWriteDebug("StartDevice\n");
		gWriteDebug("DeviceName:");
		gWriteDebug(mName.c_str());
		gWriteDebug("\n");
		gWriteDebug("PCIString:");
		gWriteDebug(mPCIString.c_str());
		gWriteDebug("\n");
	}
	llinfos << llendl;
	llinfos << "DeviceName:" << mName << llendl;
	llinfos << "PCIString:" << mPCIString << llendl;
	llinfos << "Drivers" << llendl;
	llinfos << "-------" << llendl;
	for (driver_file_map_t::iterator iter = mDriverFiles.begin(),
									 end = mDriverFiles.end();
		 iter != end; ++iter)
	{
		LLDXDriverFile *filep = iter->second;
		filep->dump();
	}
	if (gWriteDebug)
	{
		gWriteDebug("EndDevice\n");
	}

	return "";
}

LLDXDriverFile* LLDXDevice::findDriver(const std::string& driver)
{
	for (driver_file_map_t::iterator iter = mDriverFiles.begin(),
									 end = mDriverFiles.end();
		 iter != end; ++iter)
	{
		LLDXDriverFile *filep = iter->second;
		if (!utf8str_compare_insensitive(filep->mName, driver))
		{
			return filep;
		}
	}

	return NULL;
}

LLDXHardware::LLDXHardware()
{
	mVRAM = 0;
	gWriteDebug = NULL;
}

void LLDXHardware::cleanup()
{
#if 0
	for_each(mDevices.begin(), mDevices.end(), DeletePairedPointer());
	mDevices.clear();
#endif
}

#if 0
std::string LLDXHardware::dumpDevices()
{
	if (gWriteDebug)
	{
		gWriteDebug("\n");
		gWriteDebug("StartAllDevices\n");
	}
	for (device_map_t::iterator iter = mDevices.begin(), end = mDevices.end();
		 iter != end; ++iter)
	{
		LLDXDevice* devicep = iter->second;
		devicep->dump();
	}
	if (gWriteDebug)
	{
		gWriteDebug("EndAllDevices\n\n");
	}
	return "";
}

LLDXDevice* LLDXHardware::findDevice(const std::string& vendor,
									 const std::string& devices)
{
	// Iterate through different devices tokenized in devices string
	std::string str(devices);
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
	tokenizer tokens(str, sep);

	for (tokenizer::iterator iter = tokens.begin();
		 iter != tokens.end(); ++iter)
	{
		std::string dev_str = *iter;
		for (device_map_t::iterator iter = mDevices.begin(),
									end = mDevices.end();
			 iter != end; ++iter)
		{
			LLDXDevice* devicep = iter->second;
			if (devicep->mVendorID == vendor && devicep->mDeviceID == dev_str)
			{
				return devicep;
			}
		}
	}

	return NULL;
}
#endif

//static
S32 LLDXHardware::getMBVideoMemoryViaWMI()
{
	DWORD vram = 0;
	if (SUCCEEDED(GetVideoMemoryViaWMI(NULL, &vram)))
	{
		return vram / (1024 * 1024);
	}
	return 0;
}

bool LLDXHardware::getInfo(bool vram_only)
{
	if (vram_only)
	{
		// Let the user override the detection in case it fails on their system.
		// They can specify the amount of VRAM in megabytes, via the LL_VRAM_MB
		// environment variable.
		char* vram_override = getenv("LL_VRAM_MB");
		if (vram_override)
		{
			S32 vram = atoi(vram_override);
			if (vram > 0)
			{
				mVRAM = vram;
				llinfos << "Amount of VRAM overridden via the LL_VRAM_MB environment variable; detection step skipped. VRAM amount: "
						<< mVRAM << "MB" << llendl;
				return true;
			}
		}
	}

	mVRAM = 0;

	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		llwarns << "COM library initialization failed !" << llendl;
		gWriteDebug("COM library initialization failed !\n");
		return false;
	}

	bool ok = false;

	IDxDiagProvider* dx_diag_providerp = NULL;
	IDxDiagContainer* dx_diag_rootp = NULL;
	IDxDiagContainer* devices_containerp = NULL;
#if 0
	IDxDiagContainer* system_device_containerp = NULL;
#endif
	IDxDiagContainer* device_containerp = NULL;
	IDxDiagContainer* file_containerp = NULL;
	IDxDiagContainer* driver_containerp = NULL;

	// CoCreate a IDxDiagProvider*
	LL_DEBUGS("AppInit") << "CoCreateInstance IID_IDxDiagProvider" << LL_ENDL;
	hr = CoCreateInstance(CLSID_DxDiagProvider, NULL, CLSCTX_INPROC_SERVER,
						  IID_IDxDiagProvider, (LPVOID*)&dx_diag_providerp);
	if (FAILED(hr))
	{
		llwarns << "No DXDiag provider found !  DirectX 9 not installed !"
				<< llendl;
		gWriteDebug("No DXDiag provider found !  DirectX 9 not installed !\n");
		goto exit_cleanup;
	}
	if (SUCCEEDED(hr)) // if FAILED(hr) then dx9 is not installed
	{
		// Fill out a DXDIAG_INIT_PARAMS struct and pass it to
		// IDxDiagContainer::Initialize(). Passing in TRUE for
		// bAllowWHQLChecks, allows dxdiag to check if drivers are digital
		// signed as logo'd by WHQL which may connect via internet to update
		// WHQL certificates.
		DXDIAG_INIT_PARAMS dx_diag_init_params;
		ZeroMemory(&dx_diag_init_params, sizeof(DXDIAG_INIT_PARAMS));

		dx_diag_init_params.dwSize = sizeof(DXDIAG_INIT_PARAMS);
		dx_diag_init_params.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
		dx_diag_init_params.bAllowWHQLChecks = TRUE;
		dx_diag_init_params.pReserved = NULL;

		LL_DEBUGS("AppInit") << "dx_diag_providerp->Initialize" << LL_ENDL;
		hr = dx_diag_providerp->Initialize(&dx_diag_init_params);
		if (FAILED(hr))
		{
			goto exit_cleanup;
		}

		LL_DEBUGS("AppInit") << "dx_diag_providerp->GetRootContainer"
							 << LL_ENDL;
		hr = dx_diag_providerp->GetRootContainer(&dx_diag_rootp);
		if (FAILED(hr) || !dx_diag_rootp)
		{
			goto exit_cleanup;
		}

		HRESULT hr;

		// Get display driver information
		LL_DEBUGS("AppInit") << "dx_diag_rootp->GetChildContainer" << LL_ENDL;
		hr = dx_diag_rootp->GetChildContainer(L"DxDiag_DisplayDevices",
											  &devices_containerp);
		if (FAILED(hr) || !devices_containerp)
		{
			// Do not release 'dirty' devices_containerp at this stage, only
			// dx_diag_rootp
			devices_containerp = NULL;
			goto exit_cleanup;
		}

		// Make sure there is something inside
		DWORD dw_device_count;
		hr = devices_containerp->GetNumberOfChildContainers(&dw_device_count);
		if (FAILED(hr) || dw_device_count == 0)
		{
			goto exit_cleanup;
		}

		// Get device 0. By default 0 device is the primary one, however in
		// case of various hybrid graphics like itegrated AMD and PCI AMD GPUs
		// system might switch.
		LL_DEBUGS("AppInit") << "devices_containerp->GetChildContainer"
							 << LL_ENDL;
		hr = devices_containerp->GetChildContainer(L"0", &device_containerp);
		if (FAILED(hr) || !device_containerp)
		{
			goto exit_cleanup;
		}

		WCHAR deviceID[512];
		get_wstring(device_containerp, L"szDeviceID", deviceID, 512);
		// Example: searches Id like 1F06 in PNP string (aka VEN_10DE&DEV_1F06)
		// which does not seem to work on some systems since format is
		// unrecognizable but in such case keyDeviceID works.
		DWORD vram = 0;
		if (SUCCEEDED(GetVideoMemoryViaWMI(deviceID, &vram)))
		{
			mVRAM = vram / (1024 * 1024);
		}
		else
		{
			get_wstring(device_containerp, L"szDeviceID", deviceID, 512);
			// '+9' to avoid ENUM\\PCI\\ prefix.  Returns string like
			// Enum\\PCI\\VEN_10DE&DEV_1F06&SUBSYS...  and since
			// GetVideoMemoryViaWMI searches by PNPDeviceID it is sufficient
			if (SUCCEEDED(GetVideoMemoryViaWMI(deviceID + 9, &vram)))
			{
				mVRAM = vram / (1024 * 1024);
			}
		}

		if (mVRAM == 0)
		{
			// Get the English VRAM string
			std::string ram_str = get_string(device_containerp,
											 L"szDisplayMemoryEnglish");

			// We do not need the device any more
			SAFE_RELEASE(device_containerp);

			// Dump the string as an int into the structure
			char* stopstring;
			mVRAM = strtol(ram_str.c_str(), &stopstring, 10);
			llinfos << "DX9 string: " << ram_str << llendl;
		}

		if (mVRAM)
		{
			llinfos << "VRAM amount: " << mVRAM << "MB" << llendl;
		}

		if (vram_only)
		{
			ok = true;
			goto exit_cleanup;
		}

#if 0	// For now, we ONLY do vram_only.
		// Now let's get device and driver information
		// Get the IDxDiagContainer object called "DxDiag_SystemDevices".
		// This call may take some time while dxdiag gathers the info.
		DWORD num_devices = 0;
		WCHAR exit_cleanup[256];
		LL_DEBUGS("AppInit") << "dx_diag_rootp->GetChildContainer DxDiag_SystemDevices"
							 << LL_ENDL;
		hr = dx_diag_rootp->GetChildContainer(L"DxDiag_SystemDevices",
											  &system_device_containerp);
		if (FAILED(hr))
		{
			goto exit_cleanup;
		}

		hr = system_device_containerp->GetNumberOfChildContainers(&num_devices);
		if (FAILED(hr))
		{
			goto exit_cleanup;
		}

		LL_DEBUGS("AppInit") << "DX9 iterating over devices" << LL_ENDL;
		S32 device_num = 0;
		for (device_num = 0; device_num < (S32)num_devices; ++device_num)
		{
			hr = system_device_containerp->EnumChildContainerNames(device_num,
																   wc_container,
																   256);
			if (FAILED(hr))
			{
				goto exit_cleanup;
			}

			hr = system_device_containerp->GetChildContainer(wc_container,
															 &device_containerp);
			if (FAILED(hr) || device_containerp == NULL)
			{
				goto exit_cleanup;
			}

			std::string device_name = get_string(device_containerp,
												 L"szDescription");

			std::string device_id = get_string(device_containerp,
											   L"szDeviceID");

			LLDXDevice *dxdevicep = new LLDXDevice;
			dxdevicep->mName = device_name;
			dxdevicep->mPCIString = device_id;
			mDevices[dxdevicep->mPCIString] = dxdevicep;

			// Split the PCI string based on vendor, device, subsys, rev.
			std::string str(device_id);
			typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
			boost::char_separator<char> sep("&\\", "",
											boost::keep_empty_tokens);
			tokenizer tokens(str, sep);

			S32 count = 0;
			bool valid = true;
			for (tokenizer::iterator iter = tokens.begin();
				 iter != tokens.end() && count < 3; ++iter)
			{
				switch (count)
				{
					case 0:
						if (strcmp(iter->c_str(), "PCI"))
						{
							valid = false;
						}
						break;
					case 1:
						dxdevicep->mVendorID = iter->c_str();
						break;
					case 2:
						dxdevicep->mDeviceID = iter->c_str();
						break;
					default:
						// Ignore it
						break;
				}
				++count;
			}

			// Now, iterate through the related drivers
			hr = device_containerp->GetChildContainer(L"Drivers",
													  &driver_containerp);
			if (FAILED(hr) || !driver_containerp)
			{
				goto exit_cleanup;
			}

			DWORD num_files = 0;
			hr = driver_containerp->GetNumberOfChildContainers(&num_files);
			if (FAILED(hr))
			{
				goto exit_cleanup;
			}

			S32 file_num = 0;
			for (file_num = 0; file_num < (S32)num_files; ++file_num)
			{

				hr = driver_containerp->EnumChildContainerNames(file_num,
																wc_container,
																256);
				if (FAILED(hr))
				{
					goto exit_cleanup;
				}

				hr = driver_containerp->GetChildContainer(wc_container,
														  &file_containerp);
				if (FAILED(hr) || file_containerp == NULL)
				{
					goto exit_cleanup;
				}

				std::string driver_path = get_string(file_containerp,
													 L"szPath");
				std::string driver_name = get_string(file_containerp,
													 L"szName");
				std::string driver_version = get_string(file_containerp,
														L"szVersion");
				std::string driver_date = get_string(file_containerp,
													 L"szDatestampEnglish");

				LLDXDriverFile* dxdriverfilep = new LLDXDriverFile;
				dxdriverfilep->mName = driver_name;
				dxdriverfilep->mFilepath= driver_path;
				dxdriverfilep->mVersionString = driver_version;
				dxdriverfilep->mVersion.set(driver_version);
				dxdriverfilep->mDateString = driver_date;

				dxdevicep->mDriverFiles[driver_name] = dxdriverfilep;

				SAFE_RELEASE(file_containerp);
			}
			SAFE_RELEASE(device_containerp);
		}
#endif
	}

	// dumpDevices();
	ok = true;

exit_cleanup:
	if (!ok)
	{
		llwarns << "DX9 probe failed" << llendl;
		gWriteDebug("DX9 probe failed\n");
	}

	SAFE_RELEASE(file_containerp);
	SAFE_RELEASE(driver_containerp);
	SAFE_RELEASE(device_containerp);
	SAFE_RELEASE(devices_containerp);
	SAFE_RELEASE(dx_diag_rootp);
	SAFE_RELEASE(dx_diag_providerp);

	CoUninitialize();

	return ok;
}

LLSD LLDXHardware::getDisplayInfo()
{
	LLSD ret;

	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		llwarns << "COM library initialization failed !" << llendl;
		gWriteDebug("COM library initialization failed !\n");
		return ret;
	}

	IDxDiagProvider* dx_diag_providerp = NULL;
	IDxDiagContainer* dx_diag_rootp = NULL;
	IDxDiagContainer* devices_containerp = NULL;
	IDxDiagContainer* device_containerp = NULL;
	IDxDiagContainer* file_containerp = NULL;
	IDxDiagContainer* driver_containerp = NULL;

	// CoCreate a IDxDiagProvider*
	llinfos << "CoCreateInstance IID_IDxDiagProvider" << llendl;
	hr = CoCreateInstance(CLSID_DxDiagProvider, NULL, CLSCTX_INPROC_SERVER,
						  IID_IDxDiagProvider, (LPVOID*)&dx_diag_providerp);
	if (FAILED(hr))
	{
		llwarns << "No DXDiag provider found !  DirectX 9 not installed !"
				<< llendl;
		gWriteDebug("No DXDiag provider found !  DirectX 9 not installed !\n");
		goto exit_cleanup;
	}
	if (SUCCEEDED(hr)) // if FAILED(hr) then dx9 is not installed
	{
		// Fill out a DXDIAG_INIT_PARAMS struct and pass it to
		// IDxDiagContainer::Initialize(). Passing in TRUE for bAllowWHQLChecks
		// allows dxdiag to check if drivers are digital signed as logo'd by
		// WHQL which may connect via internet to update WHQL certificates.
		DXDIAG_INIT_PARAMS dx_diag_init_params;
		ZeroMemory(&dx_diag_init_params, sizeof(DXDIAG_INIT_PARAMS));

		dx_diag_init_params.dwSize = sizeof(DXDIAG_INIT_PARAMS);
		dx_diag_init_params.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
		dx_diag_init_params.bAllowWHQLChecks = TRUE;
		dx_diag_init_params.pReserved = NULL;

		LL_DEBUGS("AppInit") << "dx_diag_providerp->Initialize" << LL_ENDL;
		hr = dx_diag_providerp->Initialize(&dx_diag_init_params);
		if (FAILED(hr))
		{
			goto exit_cleanup;
		}

		LL_DEBUGS("AppInit") << "dx_diag_providerp->GetRootContainer"
							 << LL_ENDL;
		hr = dx_diag_providerp->GetRootContainer(&dx_diag_rootp);
		if (FAILED(hr) || !dx_diag_rootp)
		{
			goto exit_cleanup;
		}

		HRESULT hr;

		// Get display driver information
		LL_DEBUGS("AppInit") << "dx_diag_rootp->GetChildContainer" << LL_ENDL;
		hr = dx_diag_rootp->GetChildContainer(L"DxDiag_DisplayDevices",
												&devices_containerp);
		if (FAILED(hr) || !devices_containerp)
		{
			// Do not release 'dirty' devices_containerp at this stage, only
			// dx_diag_rootp
			devices_containerp = NULL;
			goto exit_cleanup;
		}

		DWORD dw_device_count;
		// Make sure there is something inside
		hr = devices_containerp->GetNumberOfChildContainers(&dw_device_count);
		if (FAILED(hr) || dw_device_count == 0)
		{
			goto exit_cleanup;
		}

		// Get device 0
		LL_DEBUGS("AppInit") << "devices_containerp->GetChildContainer"
							 << LL_ENDL;
		hr = devices_containerp->GetChildContainer(L"0", &device_containerp);
		if (FAILED(hr) || !device_containerp)
		{
			goto exit_cleanup;
		}

		// Get the English VRAM string
		std::string ram_str = get_string(device_containerp,
										 L"szDisplayMemoryEnglish");

		// Dump the string as an int into the structure
		char* stopstring;
		ret["VRAM"] = (S32)(strtol(ram_str.c_str(), &stopstring, 10) / (1024 * 1024));
		std::string device_name = get_string(device_containerp,
											 L"szDescription");
		ret["DeviceName"] = device_name;
		std::string device_driver=  get_string(device_containerp,
											   L"szDriverVersion");
		ret["DriverVersion"] = device_driver;

		// ATI has a slightly different version string
		if (device_name.length() >= 4 && device_name.substr(0, 4) == "ATI ")
		{
			// Get the key
			HKEY hKey;
			const DWORD RV_SIZE = 100;
			WCHAR release_version[RV_SIZE];

			// Hard coded registry entry. Using this since it is simpler for
			// now. And using EnumDisplayDevices to get a registry key also
			// requires a hard coded Query value.
			if (ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE,
											TEXT("SOFTWARE\\ATI Technologies\\CBT"),
											&hKey))
			{
				// Get the value
				DWORD dwType = REG_SZ;
				DWORD dwSize = sizeof(WCHAR) * RV_SIZE;
				if (ERROR_SUCCESS == RegQueryValueEx(hKey,
													 TEXT("ReleaseVersion"),
				 									 NULL, &dwType,
													 (LPBYTE)release_version,
													 &dwSize))
				{
					// Print the value; Windows does not guarantee to be nul
					// terminated
					release_version[RV_SIZE - 1] = 0;
					ret["DriverVersion"] =
						ll_convert_wide_to_string(release_version);

				}
				RegCloseKey(hKey);
			}
		}
	}

exit_cleanup:
	if (!ret.size())
	{
		llinfos << "Failed to get data, cleaning up..." << llendl;
	}
	SAFE_RELEASE(file_containerp);
	SAFE_RELEASE(driver_containerp);
	SAFE_RELEASE(device_containerp);
	SAFE_RELEASE(devices_containerp);
	SAFE_RELEASE(dx_diag_rootp);
	SAFE_RELEASE(dx_diag_providerp);

	CoUninitialize();
	return ret;
}

void LLDXHardware::setWriteDebugFunc(void (*func)(const char*))
{
	gWriteDebug = func;
}

#endif
