#include "stdafx.h"
#include "File.h"
#include "OnDemandBitmapManager.h"


std::wstring OnDemandBitmapManager::GetCacheDir()
{
	if (cacheDir.empty())
	{
		cacheDir = File::JoinPath(File::GetAppDir(), L"BitmapCache\\");
	}
	return cacheDir;
}

std::string OnDemandBitmapManager::Add(std::shared_ptr<Bitmap> bitmap)
{
	std::string guid(CreateGuidString());

	if (saveToDisk)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(guid));

		File::WriteProtected(path, bitmap->Serialize());
	}

	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		bitmapCache[guid] = bitmap;
	}

	UpdateUsage(guid);
	RunCleanupAsync();

	return guid;
}

std::weak_ptr<Bitmap> OnDemandBitmapManager::Get(std::string guid)
{
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		auto it = bitmapCache.find(guid);
		if (it != bitmapCache.end())
		{
			return it->second;
		}
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(guid));

	// Load outside the lock so file I/O does not block other cache users.
	std::shared_ptr<Bitmap> bitmap = std::make_shared<Bitmap>();
	bool wasPlaintext = false;
	bitmap->Deserialize(File::ReadProtected(path, wasPlaintext));
	if (wasPlaintext && saveToDisk)
	{
		// Cache file from an older version: replace it with an encrypted one.
		File::WriteProtected(path, bitmap->Serialize());
	}

	std::lock_guard<std::mutex> lock(cacheMutex);
	auto inserted = bitmapCache.emplace(guid, bitmap);
	if (inserted.second)
	{
		usageList.push_front(guid);
	}
	return inserted.first->second;
}

void OnDemandBitmapManager::Remove(std::string guid)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(guid));

	// The thread must own its copy of the path; a pointer into the
	// local string would dangle once this function returns.
	std::thread([path]() { File::Delete(path.c_str()); }).detach();

	std::lock_guard<std::mutex> lock(cacheMutex);
	bitmapCache.erase(guid);

	if (!usageList.empty())
	{
		auto findValue = std::find(usageList.begin(), usageList.end(), guid);
		if (findValue != usageList.end())
		{
			usageList.erase(findValue);
		}
	}
}

bool OnDemandBitmapManager::Exists(std::string guid)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(guid));

	return File::Exists(path.c_str());
}

unsigned long long OnDemandBitmapManager::MaxBytes() noexcept
{
	return maxBytes;
}

void OnDemandBitmapManager::SetMaxBytes(unsigned long long newMaxBytes)
{
	if (newMaxBytes != maxBytes)
	{
		maxBytes = newMaxBytes;
		RunCleanupAsync();
	}
}

void OnDemandBitmapManager::SetSaveToDisk(bool newSaveToDisk)
{
	if (newSaveToDisk != saveToDisk)
	{
		saveToDisk = newSaveToDisk;

		// Wait for a running cleanup to finish.
		{
			std::lock_guard<std::mutex> cleanupLock(cleanupMutex);
		}

		if (saveToDisk)
		{
			// Iterate a snapshot: UpdateUsage takes cacheMutex itself.
			std::map<std::string, std::shared_ptr<Bitmap>> snapshot;
			{
				std::lock_guard<std::mutex> lock(cacheMutex);
				snapshot = bitmapCache;
			}
			for (const auto &pair : snapshot)
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
				auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(pair.first));
				File::WriteProtected(path, pair.second->Serialize());
				UpdateUsage(pair.first);
			}
		}
	}
}

void OnDemandBitmapManager::DeleteAllFromDisk()
{
	File::DeleteDir(GetCacheDir());
}

// Older versions stored every bitmap and its thumbnail as plaintext BMP.
// Get only migrates a bitmap when it is loaded, which may never happen,
// so encrypt all remaining plaintext files and drop the thumbnails once
// at startup. The bytes are rewritten as-is and not parsed here.
void OnDemandBitmapManager::ProtectDiskCache()
{
	const std::wstring dir = GetCacheDir();
	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(File::JoinPath(dir, L"*").c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}

		const std::wstring name(findData.cFileName);
		const std::wstring path = File::JoinPath(dir, name);
		const std::wstring thumbSuffix(L"-thumb");

		if (name.size() > thumbSuffix.size()
			&& name.compare(name.size() - thumbSuffix.size(), thumbSuffix.size(), thumbSuffix) == 0)
		{
			File::Delete(path.c_str());
			continue;
		}

		try
		{
			bool wasPlaintext = false;
			std::string data = File::ReadProtected(path, wasPlaintext);
			if (wasPlaintext)
			{
				File::WriteProtected(path, data);
			}
		}
		catch (const std::exception&)
		{
			// Undecryptable files are rejected when the clip loads them.
		}
	} while (FindNextFileW(hFind, &findData));

	FindClose(hFind);
}

// Thumbnails are kept in memory only: a file on disk would be an
// unencrypted copy of the clip image.
HBITMAP OnDemandBitmapManager::CreateThumbnail(std::shared_ptr<Bitmap> bitmap)
{
	HBITMAP retVal = nullptr;

	const UINT height = GetSystemMetrics(SM_CXMENUCHECK);
	const UINT width = GetSystemMetrics(SM_CXMENUCHECK);


	HBITMAP hBitmap = bitmap->GetHbitmap();
	HRESULT hr = hBitmap ? S_OK : E_FAIL;

	IWICBitmap * pBitmap = nullptr;
	IWICBitmapScaler * piScaler = nullptr;

	if (SUCCEEDED(hr))
	{
		hr = GetWICFactory()->CreateBitmapFromHBITMAP(
			hBitmap,
			nullptr,
			bitmap->DibBitmapInfoHeader()->biBitCount == 32 ? WICBitmapUseAlpha : WICBitmapIgnoreAlpha,
			&pBitmap
		);
	}

	if (SUCCEEDED(hr))
	{
		hr = GetWICFactory()->CreateBitmapScaler(&piScaler);
	}

	if (SUCCEEDED(hr))
	{

		hr = piScaler->Initialize(
			pBitmap,
			width,
			height,
			WICBitmapInterpolationModeFant);
	}

	IWICFormatConverter * pConvertedSourceBitmap = nullptr;

	if (SUCCEEDED(hr))
	{
		hr = pIWICFactory->CreateFormatConverter(&pConvertedSourceBitmap);
	}

	// Just to make sure it's 32 bits
	if (SUCCEEDED(hr))
	{
		hr = pConvertedSourceBitmap->Initialize(
			piScaler,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.f,
			WICBitmapPaletteTypeCustom
		);
	}

	if (SUCCEEDED(hr))
	{
		UINT bufferSize = width * height * 4;
		std::vector<BYTE> buffer;
		buffer.resize(size_t{ bufferSize });
		pConvertedSourceBitmap->CopyPixels(nullptr, width * 4, bufferSize, buffer.data());

		retVal = CreateBitmap(width, height, 1, 32, buffer.data());
	}

	SafeRelease(&pConvertedSourceBitmap);
	SafeRelease(&pBitmap);
	SafeRelease(&piScaler);

	return retVal;
}

HBITMAP OnDemandBitmapManager::GetThumbnail(std::string guid)
{
	HBITMAP retVal = nullptr;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(guid));

	// Older versions saved thumbnails as plaintext BMP files.
	path += L"-thumb";
	if (File::Exists(path.c_str()))
	{
		File::Delete(path.c_str());
	}

	std::shared_ptr<Bitmap> pBitmap = OnDemandBitmapManager::Get(guid).lock();
	if (pBitmap != nullptr)
	{
		retVal = CreateThumbnail(pBitmap);
	}

	return retVal;
}

void OnDemandBitmapManager::RemoveThumbnail(std::string guid, bool async)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	auto path = File::JoinPath(GetCacheDir(), converter.from_bytes(guid));
	
	if (async)
	{
		std::thread([thumbPath = path + L"-thumb"]() { File::Delete(thumbPath.c_str()); }).detach();
	}
	else
	{
		File::Delete((path + L"-thumb").c_str());
	}
}

std::string OnDemandBitmapManager::CreateGuidString()
{
	GUID guid;
	char guidString[38];
	const HRESULT hr = CoCreateGuid(&guid);
	if (SUCCEEDED(hr) || hr == RPC_S_UUID_LOCAL_ONLY)
	{
		sprintf_s(guidString,
			38,
			"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			guid.Data1,
			guid.Data2,
			guid.Data3,
			guid.Data4[0],
			guid.Data4[1],
			guid.Data4[2],
			guid.Data4[3],
			guid.Data4[4],
			guid.Data4[5],
			guid.Data4[6],
			guid.Data4[7]);

		return std::string(guidString);
	}
	else
	{
		throw std::exception("GUID could not be created.");
	}
}

void OnDemandBitmapManager::UpdateUsage(std::string guid)
{
	bool changed = false;
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		auto findValue = std::find(usageList.begin(), usageList.end(), guid);

		if (findValue != usageList.begin())
		{
			if (findValue != usageList.end())
			{
				usageList.erase(findValue);
			}
			usageList.push_front(guid);
			changed = true;
		}
	}

	if (changed)
	{
		RunCleanupAsync();
	}
}

void OnDemandBitmapManager::SafeClose()
{
	safeClosing = true;

	// Wait for a running cleanup and any in-flight cache access.
	std::lock_guard<std::mutex> cleanupLock(cleanupMutex);
	std::lock_guard<std::mutex> lock(cacheMutex);
}

void OnDemandBitmapManager::RunCleanupAsync()
{
	if (saveToDisk)
	{
		// Only the caller that raises the count from 0 starts the worker.
		if (cleanupWaitCount++ == 0)
		{
			std::thread(RunCleanupInternal).detach();
		}
	}
}

void OnDemandBitmapManager::RunCleanupInternal()
{
	std::lock_guard<std::mutex> cleanupLock(cleanupMutex);
	while (cleanupWaitCount != 0 && !safeClosing)
	{
		// effective 5 second delay, but allows
		// us to exit in 1 second at worst if
		// program is in closing state.
		int i = 5;
		cleanupWaitCount = 1;
		while (i > 0 && !safeClosing)
		{
			Sleep(1000);
			i--;
		}
		cleanupWaitCount--;
	}
	
	std::lock_guard<std::mutex> lock(cacheMutex);
	if (!usageList.empty() && !safeClosing)
	{
		unsigned long long currentSize = 0;
		std::map<std::string, std::shared_ptr<Bitmap>>::iterator it;
		for (it = bitmapCache.begin(); it != bitmapCache.end(); it++)
		{
			// We multiply by two because each Bitmap is roughly the size of
			// itself plus the HBITMAP used in other parts of the program.
			currentSize += static_cast<unsigned long long>(it->second->Size()) * 2;
		}

		long references = 0;
		while (currentSize > maxBytes && references < 2 && !usageList.empty())
		{
			auto key = usageList.back();
			auto cached = bitmapCache.find(key);
			if (cached == bitmapCache.end())
			{
				usageList.pop_back();
				continue;
			}
			references = cached->second.use_count();
			if (references < 2)
			{
				currentSize -= static_cast<unsigned long long>(cached->second->Size()) * 2;
				bitmapCache.erase(cached);
				usageList.pop_back();
			}
		}
	}
}

IWICImagingFactory * OnDemandBitmapManager::GetWICFactory()
{
	if (pIWICFactory == nullptr)
	{
		const HRESULT hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pIWICFactory)
		);
	}

	return pIWICFactory;
}

IWICImagingFactory * OnDemandBitmapManager::pIWICFactory = nullptr;

std::wstring OnDemandBitmapManager::cacheDir;
std::map<std::string, std::shared_ptr<Bitmap>> OnDemandBitmapManager::bitmapCache;
std::deque<std::string> OnDemandBitmapManager::usageList;

std::atomic<unsigned long long> OnDemandBitmapManager::maxBytes{ 104857600 }; // 100 MB
std::atomic<unsigned int> OnDemandBitmapManager::cleanupWaitCount{ 0 };
std::atomic<bool> OnDemandBitmapManager::saveToDisk{ true };
std::atomic<bool> OnDemandBitmapManager::safeClosing{ false };
std::mutex OnDemandBitmapManager::cacheMutex;
std::mutex OnDemandBitmapManager::cleanupMutex;