#include "stdafx.h"
#include "Updater.h"
#include "File.h"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <winhttp.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include "monocypher/monocypher.h"
#include "monocypher/monocypher-ed25519.h"
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Version.lib")

namespace
{
	const wchar_t MANIFEST_URL[] = L"https://github.com/tomcek42/Winclipper/releases/latest/download/winclipper-update.txt";
	const wchar_t SIGNATURE_URL[] = L"https://github.com/tomcek42/Winclipper/releases/latest/download/winclipper-update.txt.minisig";
	// The signed manifest may only point at release assets of this fork.
	const char ALLOWED_MSI_PREFIX[] = "https://github.com/tomcek42/Winclipper/releases/download/";
	const char MANIFEST_HEADER[] = "winclipper-update 1";

#ifdef _WIN64
	const char MANIFEST_ARCH[] = "x64";
#else
	const char MANIFEST_ARCH[] = "x86";
#endif

	const size_t MAX_MANIFEST_BYTES = 4096;
	const size_t MAX_SIGNATURE_BYTES = 1024;			// minisign signature file
	const size_t MAX_MSI_BYTES = 64 * 1024 * 1024;

	const std::chrono::minutes FIRST_CHECK_DELAY(2);
	const std::chrono::seconds ENABLED_CHECK_DELAY(10);
	const std::chrono::hours CHECK_INTERVAL(24);

	// Public release signing key in minisign format (Ed25519), the same scheme
	// the Tauri updater uses. The password-protected secret key is kept by the
	// project owner outside the repository; see docs/UPDATER.md.
	const char SIGNING_PUBLIC_KEY[] = "RWTz7+Iq64LIJPbV1wa0ZHkkDVs47kC4omqJRRQTLfT2U/Th4YOs67F+";

	std::mutex stateMutex;
	std::condition_variable stateChanged;
	HWND notifyWindow = nullptr;
	bool enabled = false;
	bool checkSoon = false;			// periodic check shortly after enabling
	bool manualCheck = false;		// CheckNow: report the result in any case
	bool installRequested = false;	// the user confirmed the available version
	bool installing = false;
	bool started = false;

	void Log(const std::wstring & message)
	{
		OutputDebugStringW((L"Winclipper updater: " + message + L"\r\n").c_str());
	}

	unsigned long long OwnVersion()
	{
		wchar_t path[MAX_PATH];
		const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
		{
			return 0;
		}
		DWORD ignored = 0;
		const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
		if (size == 0)
		{
			return 0;
		}
		std::vector<BYTE> info(size);
		VS_FIXEDFILEINFO * fixed = nullptr;
		UINT fixedLength = 0;
		if (!GetFileVersionInfoW(path, 0, size, info.data())
			|| !VerQueryValueW(info.data(), L"\\", reinterpret_cast<void **>(&fixed), &fixedLength)
			|| fixed == nullptr || fixedLength < sizeof(VS_FIXEDFILEINFO))
		{
			return 0;
		}
		return (static_cast<unsigned long long>(fixed->dwFileVersionMS) << 32) | fixed->dwFileVersionLS;
	}

	// Parses "a.b.c.d" (each part 0-65535) into the VS_FIXEDFILEINFO layout.
	bool ParseVersion(const std::string & text, unsigned long long & version)
	{
		version = 0;
		size_t start = 0;
		for (int part = 0; part < 4; part++)
		{
			const size_t end = part < 3 ? text.find('.', start) : text.size();
			if (end == std::string::npos || end == start || end - start > 5)
			{
				return false;
			}
			unsigned long value = 0;
			for (size_t i = start; i < end; i++)
			{
				if (text[i] < '0' || text[i] > '9')
				{
					return false;
				}
				value = value * 10 + (text[i] - '0');
			}
			if (value > 0xFFFF)
			{
				return false;
			}
			version = (version << 16) | value;
			start = end + 1;
		}
		return true;
	}

	std::wstring VersionString(unsigned long long version)
	{
		return std::to_wstring((version >> 48) & 0xFFFF) + L"." + std::to_wstring((version >> 32) & 0xFFFF) + L"."
			+ std::to_wstring((version >> 16) & 0xFFFF) + L"." + std::to_wstring(version & 0xFFFF);
	}

	// Downloads an https URL into data. Fails on any non-200 status or if the
	// body exceeds maxBytes. Redirects are followed, but never to plain http.
	bool HttpGet(const std::wstring & url, size_t maxBytes, std::string & data)
	{
		data.clear();

		URL_COMPONENTS parts = { sizeof(parts) };
		parts.dwHostNameLength = static_cast<DWORD>(-1);
		parts.dwUrlPathLength = static_cast<DWORD>(-1);
		parts.dwExtraInfoLength = static_cast<DWORD>(-1);
		if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
		{
			return false;
		}
		const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
		const std::wstring path = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength)
			+ std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength);

		bool success = false;
		HINTERNET session = WinHttpOpen(L"Winclipper-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		HINTERNET connection = session ? WinHttpConnect(session, host.c_str(), parts.nPort, 0) : nullptr;
		HINTERNET request = connection
			? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
				WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
			: nullptr;

		if (request)
		{
			DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
			WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
			WinHttpSetTimeouts(request, 15000, 15000, 30000, 30000);

			DWORD status = 0;
			DWORD statusSize = sizeof(status);
			if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
				&& WinHttpReceiveResponse(request, nullptr)
				&& WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)
				&& status == 200)
			{
				success = true;
				char buffer[16384];
				DWORD read = 0;
				do
				{
					if (!WinHttpReadData(request, buffer, sizeof(buffer), &read) || data.size() + read > maxBytes)
					{
						success = false;
						break;
					}
					data.append(buffer, read);
				} while (read > 0);
			}
		}

		if (request) WinHttpCloseHandle(request);
		if (connection) WinHttpCloseHandle(connection);
		if (session) WinHttpCloseHandle(session);
		if (!success)
		{
			data.clear();
		}
		return success;
	}

	bool Sha256(const void * data, size_t size, BYTE (&hash)[32])
	{
		if (size > ULONG_MAX)
		{
			return false;
		}
		bool success = false;
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_HASH_HANDLE hashHandle = nullptr;
		if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))
			&& BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hashHandle, nullptr, 0, nullptr, 0, 0))
			&& BCRYPT_SUCCESS(BCryptHashData(hashHandle, reinterpret_cast<PUCHAR>(const_cast<void *>(data)), static_cast<ULONG>(size), 0))
			&& BCRYPT_SUCCESS(BCryptFinishHash(hashHandle, hash, sizeof(hash), 0)))
		{
			success = true;
		}
		if (hashHandle) BCryptDestroyHash(hashHandle);
		if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
		return success;
	}

	bool Base64Decode(const std::string & text, std::vector<BYTE> & out)
	{
		DWORD size = 0;
		if (text.empty() || text.size() > MAX_SIGNATURE_BYTES
			|| !CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr))
		{
			return false;
		}
		out.resize(size);
		return CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, out.data(), &size, nullptr, nullptr)
			&& size == out.size();
	}

	// Verifies a minisign signature file over the manifest:
	//   untrusted comment: ...
	//   base64("ED" || key id[8] || Ed25519 signature of BLAKE2b-512(manifest))
	//   trusted comment: ...
	//   base64(Ed25519 signature of (signature[64] || trusted comment))
	// Only the prehashed "ED" variant is accepted, and both signatures must be valid.
	bool VerifySignature(const std::string & manifest, const std::string & signatureFile)
	{
		const char TRUSTED_PREFIX[] = "trusted comment: ";
		const size_t TRUSTED_PREFIX_LEN = sizeof(TRUSTED_PREFIX) - 1;

		std::vector<std::string> lines;
		std::istringstream stream(signatureFile);
		std::string line;
		while (lines.size() < 4 && std::getline(stream, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			lines.push_back(line);
		}
		if (lines.size() != 4
			|| lines[0].compare(0, 19, "untrusted comment: ") != 0
			|| lines[2].compare(0, TRUSTED_PREFIX_LEN, TRUSTED_PREFIX) != 0)
		{
			return false;
		}

		std::vector<BYTE> publicKey, signature, globalSignature;
		if (!Base64Decode(SIGNING_PUBLIC_KEY, publicKey) || publicKey.size() != 42
			|| publicKey[0] != 'E' || publicKey[1] != 'd'
			|| !Base64Decode(lines[1], signature) || signature.size() != 74
			|| signature[0] != 'E' || signature[1] != 'D'
			|| memcmp(signature.data() + 2, publicKey.data() + 2, 8) != 0
			|| !Base64Decode(lines[3], globalSignature) || globalSignature.size() != 64)
		{
			return false;
		}
		const BYTE * key = publicKey.data() + 10;
		const BYTE * sig = signature.data() + 10;

		BYTE hash[64];
		crypto_blake2b(hash, sizeof(hash), reinterpret_cast<const BYTE *>(manifest.data()), manifest.size());
		if (crypto_ed25519_check(sig, key, hash, sizeof(hash)) != 0)
		{
			return false;
		}

		std::vector<BYTE> global(sig, sig + 64);
		global.insert(global.end(), lines[2].begin() + TRUSTED_PREFIX_LEN, lines[2].end());
		return crypto_ed25519_check(globalSignature.data(), key, global.data(), global.size()) == 0;
	}

	bool ParseHex32(const std::string & text, BYTE (&out)[32])
	{
		if (text.size() != 64)
		{
			return false;
		}
		for (size_t i = 0; i < 32; i++)
		{
			int value = 0;
			for (size_t j = 0; j < 2; j++)
			{
				const char c = text[i * 2 + j];
				int nibble;
				if (c >= '0' && c <= '9') nibble = c - '0';
				else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
				else return false;
				value = value * 16 + nibble;
			}
			out[i] = static_cast<BYTE>(value);
		}
		return true;
	}

	struct UpdateInfo
	{
		unsigned long long version = 0;
		BYTE sha256[32] = {};
		std::string url;
	};

	// Manifest format (LF or CRLF):
	//   winclipper-update 1
	//   version 1.6.0.0
	//   x64 <sha256 hex> <https url>
	//   x86 <sha256 hex> <https url>
	// Only called after the signature has been verified.
	bool ParseManifest(const std::string & manifest, UpdateInfo & info)
	{
		std::istringstream stream(manifest);
		std::string line;
		bool header = false;
		bool haveVersion = false;
		bool haveArch = false;
		while (std::getline(stream, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			if (!header)
			{
				if (line != MANIFEST_HEADER)
				{
					return false;
				}
				header = true;
				continue;
			}
			std::istringstream fields(line);
			std::string key, first, second;
			fields >> key >> first >> second;
			if (key == "version")
			{
				haveVersion = ParseVersion(first, info.version);
			}
			else if (key == MANIFEST_ARCH)
			{
				if (!ParseHex32(first, info.sha256)
					|| second.compare(0, sizeof(ALLOWED_MSI_PREFIX) - 1, ALLOWED_MSI_PREFIX) != 0
					|| std::any_of(second.begin(), second.end(),
						[](char c) { return c < 0x21 || c > 0x7E || c == '"' || c == '<' || c == '>'; }))
				{
					return false;
				}
				info.url = second;
				haveArch = true;
			}
		}
		return header && haveVersion && haveArch;
	}

	void PostState(Updater::State state)
	{
		HWND target;
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			target = notifyWindow;
		}
		PostMessageW(target, Updater::WMAPP_UPDATE_STATE, state, 0);
	}

	// Writes the verified MSI to a new file and keeps it open without write
	// or delete sharing, so it cannot be swapped between the hash check and
	// msiexec opening it. msiexec itself only needs read access.
	HANDLE WriteLockedMsi(const std::string & msi, const BYTE (&expected)[32], std::wstring & path)
	{
		const std::wstring dir = File::JoinPath(File::GetAppDir(), L"Update\\");
		SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);

		GUID guid;
		wchar_t guidText[64];
		if (FAILED(CoCreateGuid(&guid)) || StringFromGUID2(guid, guidText, 64) == 0)
		{
			return INVALID_HANDLE_VALUE;
		}
		path = dir + L"WinclipperSetup-" + guidText + L".msi";

		HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			return INVALID_HANDLE_VALUE;
		}

		DWORD written = 0;
		std::string reread(msi.size(), '\0');
		DWORD read = 0;
		BYTE actual[32];
		LARGE_INTEGER zero = {};
		if (WriteFile(file, msi.data(), static_cast<DWORD>(msi.size()), &written, nullptr) && written == msi.size()
			&& FlushFileBuffers(file)
			&& SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)
			&& ReadFile(file, &reread[0], static_cast<DWORD>(reread.size()), &read, nullptr) && read == msi.size()
			&& Sha256(reread.data(), reread.size(), actual)
			&& memcmp(actual, expected, sizeof(actual)) == 0)
		{
			return file;
		}
		CloseHandle(file);
		DeleteFileW(path.c_str());
		return INVALID_HANDLE_VALUE;
	}

	// Returns the msiexec exit code, or -1 if it could not be started.
	long RunMsiexec(const std::wstring & msiPath)
	{
		wchar_t system[MAX_PATH];
		const UINT length = GetSystemDirectoryW(system, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
		{
			return -1;
		}
		const std::wstring msiexec = File::JoinPath(system, L"msiexec.exe");
		const std::wstring parameters = L"/i \"" + msiPath + L"\" /passive /norestart";

		SHELLEXECUTEINFOW sei = { sizeof(sei) };
		sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
		sei.lpFile = msiexec.c_str();
		sei.lpParameters = parameters.c_str();
		sei.nShow = SW_SHOWNORMAL;
		if (!ShellExecuteExW(&sei) || sei.hProcess == nullptr)
		{
			return -1;
		}
		WaitForSingleObject(sei.hProcess, INFINITE);
		DWORD exitCode = static_cast<DWORD>(-1);
		GetExitCodeProcess(sei.hProcess, &exitCode);
		CloseHandle(sei.hProcess);
		return static_cast<long>(exitCode);
	}

	// Set by Check after the signature and manifest were verified. Install
	// only ever uses this, never data fetched after the user's confirmation.
	UpdateInfo available;

	void Check(bool manual)
	{
		const unsigned long long current = OwnVersion();
		if (current == 0)
		{
			Log(L"own version unknown, skipping");
			if (manual) PostState(Updater::STATE_CHECK_FAILED);
			return;
		}

		std::string manifest, signature;
		UpdateInfo info;
		if (!HttpGet(MANIFEST_URL, MAX_MANIFEST_BYTES, manifest) || !HttpGet(SIGNATURE_URL, MAX_SIGNATURE_BYTES, signature))
		{
			Log(L"no update manifest available");
			if (manual) PostState(Updater::STATE_CHECK_FAILED);
			return;
		}
		if (!VerifySignature(manifest, signature) || !ParseManifest(manifest, info))
		{
			Log(L"manifest signature is invalid or the manifest is malformed, ignoring it");
			if (manual) PostState(Updater::STATE_CHECK_FAILED);
			return;
		}
		// Never downgrade: an old, validly signed manifest must not roll back fixes.
		if (info.version <= current)
		{
			if (manual) PostState(Updater::STATE_UP_TO_DATE);
			return;
		}
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			available = info;
		}
		Log(L"version " + VersionString(info.version) + L" is available");
		PostState(Updater::STATE_AVAILABLE);
	}

	void Install()
	{
		UpdateInfo info;
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			info = available;
		}
		if (info.version == 0)
		{
			return;
		}

		Log(L"installing " + VersionString(info.version));
		PostState(Updater::STATE_INSTALLING);

		std::string msi;
		std::wstring wideUrl(info.url.begin(), info.url.end());
		HANDLE file = INVALID_HANDLE_VALUE;
		std::wstring msiPath;
		if (HttpGet(wideUrl, MAX_MSI_BYTES, msi))
		{
			file = WriteLockedMsi(msi, info.sha256, msiPath);
		}
		if (file == INVALID_HANDLE_VALUE)
		{
			Log(L"download failed or hash mismatch");
			PostState(Updater::STATE_IDLE);
			return;
		}

		// On success the Restart Manager closes Winclipper and starts the new
		// version. If the user cancels the elevation prompt, the update stays
		// available in the tray menu.
		const long exitCode = RunMsiexec(msiPath);
		CloseHandle(file);
		DeleteFileW(msiPath.c_str());
		if (exitCode != ERROR_SUCCESS && exitCode != ERROR_SUCCESS_REBOOT_REQUIRED)
		{
			Log(L"msiexec failed with " + std::to_wstring(exitCode));
		}
		PostState(Updater::STATE_IDLE);
	}

	void Worker()
	{
		auto next = std::chrono::steady_clock::now() + FIRST_CHECK_DELAY;
		for (;;)
		{
			bool doInstall = false;
			bool doCheck = false;
			bool manual = false;
			{
				std::unique_lock<std::mutex> lock(stateMutex);
				stateChanged.wait_until(lock, next, [] { return checkSoon || manualCheck || installRequested; });
				if (installRequested)
				{
					installRequested = false;
					installing = true;
					doInstall = true;
				}
				else if (manualCheck)
				{
					manualCheck = false;
					doCheck = true;
					manual = true;
				}
				else if (checkSoon)
				{
					checkSoon = false;
					next = std::chrono::steady_clock::now() + ENABLED_CHECK_DELAY;
					continue;
				}
				else
				{
					next = std::chrono::steady_clock::now() + CHECK_INTERVAL;
					doCheck = enabled;
				}
			}
			try
			{
				if (doInstall)
				{
					Install();
				}
				else if (doCheck)
				{
					Check(manual);
				}
			}
			catch (const std::exception&)
			{
				Log(L"unexpected error");
				PostState(manual ? Updater::STATE_CHECK_FAILED : Updater::STATE_IDLE);
			}
			std::lock_guard<std::mutex> lock(stateMutex);
			installing = false;
		}
	}

	// Drops a trailing ".0" like the release tags do (1.6.1.0 -> 1.6.1).
	std::wstring DisplayVersion(unsigned long long version)
	{
		std::wstring text = VersionString(version);
		if (text.size() > 2 && text.compare(text.size() - 2, 2, L".0") == 0)
		{
			text.erase(text.size() - 2);
		}
		return text;
	}
}

void Updater::Start(HWND window)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	notifyWindow = window;
	if (!started)
	{
		started = true;
		std::thread(Worker).detach();
	}
}

void Updater::SetEnabled(bool newEnabled)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	if (newEnabled && !enabled)
	{
		checkSoon = true;
	}
	enabled = newEnabled;
	stateChanged.notify_all();
}

void Updater::CheckNow()
{
	std::lock_guard<std::mutex> lock(stateMutex);
	manualCheck = true;
	stateChanged.notify_all();
}

void Updater::InstallAvailable()
{
	std::lock_guard<std::mutex> lock(stateMutex);
	if (available.version != 0 && !installing)
	{
		installRequested = true;
		stateChanged.notify_all();
	}
}

std::wstring Updater::AvailableVersion()
{
	std::lock_guard<std::mutex> lock(stateMutex);
	return available.version == 0 ? std::wstring() : DisplayVersion(available.version);
}

std::wstring Updater::ReleaseNotesUrl()
{
	const std::wstring version = AvailableVersion();
	return version.empty()
		? L"https://github.com/tomcek42/Winclipper/releases"
		: L"https://github.com/tomcek42/Winclipper/releases/tag/v" + version;
}

std::wstring Updater::CurrentVersion()
{
	return DisplayVersion(OwnVersion());
}
