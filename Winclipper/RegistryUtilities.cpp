#include "stdafx.h"
#include <string>
#include "RegistryUtilities.h"

// Reads a REG_DWORD value. RegGetValueW checks the type and the size, so a
// value of another type (any user process can write HKCU) cannot overflow data.
static bool ReadDword(HKEY root, const wchar_t * subKey, const wchar_t * name, DWORD & data)
{
	DWORD cbData = sizeof(data);
	return RegGetValueW(root, subKey, name, RRF_RT_REG_DWORD, nullptr, &data, &cbData) == ERROR_SUCCESS;
}

unsigned int RegistryUtilities::GetWindows10ReleaseId()
{
	unsigned int retVal = 0;
	wchar_t data[64];
	DWORD cbData = sizeof(data);

	// RRF_RT_REG_SZ guarantees a NUL-terminated string within cbData.
	if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", L"ReleaseId",
		RRF_RT_REG_SZ, nullptr, data, &cbData) == ERROR_SUCCESS)
	{
		try
		{
			retVal = std::stoi(data);
		}
		catch (const std::exception&)
		{
			retVal = 0;
		}
	}

	return retVal;
}

bool RegistryUtilities::AppsUseLightTheme()
{
	DWORD data = 0;
	return !ReadDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", data)
		|| data == 1;
}

bool RegistryUtilities::ClipboardHistoryEnabled()
{
	DWORD data = 0;
	return ReadDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Clipboard", L"EnableClipboardHistory", data) && data == 1;
}

bool RegistryUtilities::DisableClipboardHistory()
{
	HKEY hOpened;

	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Clipboard", 0, KEY_SET_VALUE, &hOpened) == ERROR_SUCCESS)
	{
		DWORD lpData = 0;
		RegSetKeyValueW(hOpened, nullptr, L"EnableClipboardHistory", REG_DWORD, reinterpret_cast<LPBYTE>(&lpData), sizeof(lpData));
	}
	else
	{
		return false;
	}

	RegCloseKey(hOpened);
	return true;
}

bool RegistryUtilities::ClipboardStartupWarningDisabled()
{
	DWORD data = 0;
	return ReadDword(HKEY_CURRENT_USER, L"Software\\Winclipper", L"DisableStartupWarning", data) && data == 1;
}

bool RegistryUtilities::DisableClipboardStartupWarning()
{
	HKEY hOpened;
	LPDWORD phkResult = nullptr;

	if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Winclipper", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hOpened, phkResult) == ERROR_SUCCESS)
	{
		DWORD lpData = 1;
		RegSetKeyValueW(hOpened, nullptr, L"DisableStartupWarning", REG_DWORD, reinterpret_cast<LPBYTE>(&lpData), sizeof(lpData));
	}
	else
	{
		return false;
	}

	RegCloseKey(hOpened);
	return true;
}

// Determines whether or not a particular value exists for a given key
bool RegistryUtilities::QueryKeyForValue(HKEY hKey, wchar_t* checkValue)
{
	wchar_t    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
	DWORD    cchClassName = MAX_PATH;  // size of class string 
	DWORD    cSubKeys = 0;               // number of subkeys 
	DWORD    cbMaxSubKey;              // longest subkey size 
	DWORD    cchMaxClass;              // longest class string 
	DWORD    cValues;              // number of values for key 
	DWORD    cchMaxValue;          // longest value name 
	DWORD    cbMaxValueData;       // longest value data 
	DWORD    cbSecurityDescriptor; // size of security descriptor 
	FILETIME ftLastWriteTime;      // last write time 

	DWORD i, retCode;

	std::unique_ptr<wchar_t> achValue(new wchar_t[MAX_REG_VALUE_NAME]);
	DWORD cchValue = MAX_REG_VALUE_NAME;

	// Get the class name and the value count. 
	retCode = RegQueryInfoKeyW(
		hKey,                    // key handle 
		achClass,                // buffer for class name 
		&cchClassName,           // size of class string 
		NULL,                    // reserved 
		&cSubKeys,               // number of subkeys 
		&cbMaxSubKey,            // longest subkey size 
		&cchMaxClass,            // longest class string 
		&cValues,                // number of values for this key 
		&cchMaxValue,            // longest value name 
		&cbMaxValueData,         // longest value data 
		&cbSecurityDescriptor,   // security descriptor 
		&ftLastWriteTime);       // last write time 

								 // Enumerate the key values. 

	if (cValues)
	{
		for (i = 0, retCode = ERROR_SUCCESS; i<cValues; i++)
		{
			DWORD cchValue = MAX_REG_VALUE_NAME;
			achValue.get()[0] = '\0';
			retCode = RegEnumValueW(hKey, i,
				achValue.get(),
				&cchValue,
				NULL,
				NULL,
				NULL,
				NULL);

			if (retCode == ERROR_SUCCESS)
			{
				if (wcscmp(checkValue, achValue.get()) == 0)
				{
					return true;
				}
			}
		}
	}
	return false;
}
