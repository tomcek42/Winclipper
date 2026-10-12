#include "stdafx.h"
#include "File.h"
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")

// Files written by WriteProtected start with this marker, followed by a
// DPAPI blob bound to the current user. Files without it are from versions
// that stored clips in plaintext.
static const char PROTECTED_MAGIC[] = "WCDPAPI1";
static const size_t PROTECTED_MAGIC_LEN = sizeof(PROTECTED_MAGIC) - 1;

// Secondary entropy so blobs from other applications of the same user
// cannot be swapped in. It is not a secret.
static const char PROTECTED_ENTROPY[] = "Winclipper clip store v1";

// Gets the directory path from a given file path.
std::wstring File::GetDirName(const std::wstring& fname)
{
    std::wstring directory;
    const size_t sepIndex = fname.rfind('\\');
    if (std::wstring::npos != sepIndex)
    {
        directory = fname.substr(0, sepIndex);
    }

    return directory;
}

std::wstring File::JoinPath(const std::wstring & path1, const std::wstring & path2)
{
	// back()/front() on an empty string is undefined behavior.
	if (path1.empty())
	{
		return path2;
	}
	if (path2.empty())
	{
		return path1;
	}

	auto retVal = path1;
	auto retVal2 = path2;
	if (retVal.back() != DIR_SEP)
	{
		retVal += DIR_SEP;
	}
	
	if (retVal2.front() == DIR_SEP)
	{
		retVal2.erase(0, 1);
	}

	return retVal + retVal2;
}

// Returns location of Winclipper's Local AppData directory
const std::wstring File::GetAppDir()
{
	if (appDir.empty())
	{
		wchar_t * tempClipsPath = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &tempClipsPath)))
		{
			// Ends with DIR_SEP since 99% of the time we're appending a filename.
			appDir = JoinPath(tempClipsPath, L"\\Winclipper\\Winclipper\\");
			CoTaskMemFree(tempClipsPath);
		}
		else
		{
			throw std::exception("Your AppData folder could not be found.");
		}
	}
	
	return appDir;
}

// Determines whether or not a file eixists
bool File::Exists(const wchar_t *name) noexcept
{
    const int checkVal = PathFileExistsW(name);
    if (checkVal == 1)
    {
        return true;
    }
    return false;
}

// Returns true if the file was deleted.
bool File::Delete(const wchar_t * name) noexcept
{
    return DeleteFileW(name) != 0;
}

void File::DeleteDir(std::wstring dirName)
{
	// pFrom must be double-null terminated; c_str() supplies only one.
	// SHFileOperation does not accept a trailing separator.
	while (!dirName.empty() && dirName.back() == DIR_SEP)
	{
		dirName.pop_back();
	}
	if (dirName.empty())
	{
		return;
	}
	dirName.push_back(L'\0');

	SHFILEOPSTRUCT shFileOp = {
		nullptr,
		FO_DELETE,
		dirName.c_str(),
		nullptr,
		FOF_NOCONFIRMATION |
		FOF_NOERRORUI |
		FOF_SILENT,
		false,
		nullptr,
		nullptr};
	SHFileOperation(&shFileOp);
}

void File::Write(std::wstring filename, std::string data)
{
	if (!File::Exists(filename.c_str()))
	{
		SHCreateDirectoryExW(nullptr, (GetDirName(filename).c_str()), nullptr);
	}
	std::ofstream outStream(filename, std::ios::out | std::ios::binary);

	try
	{
		if (outStream)
		{
			outStream << data;
			outStream.close();
		}
	}
	catch (const std::exception&)
	{
		outStream.close();
		throw std::exception("Could not write clips.");
	}
}

std::string File::Read(std::wstring filename)
{
	// See: http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
	std::ifstream inStream(filename, std::ios::in | std::ios::binary);
	if (inStream)
	{
		std::ostringstream contents;
		contents << inStream.rdbuf();
		inStream.close();
		return contents.str();
	}
	return std::string();
}

// Encrypts data with DPAPI for the current user and writes it to filename.
void File::WriteProtected(std::wstring filename, const std::string & data)
{
	if (data.size() > MAXDWORD)
	{
		throw std::exception("Could not encrypt clips.");
	}

	DATA_BLOB in = { static_cast<DWORD>(data.size()), reinterpret_cast<BYTE *>(const_cast<char *>(data.data())) };
	DATA_BLOB entropy = { sizeof(PROTECTED_ENTROPY) - 1, reinterpret_cast<BYTE *>(const_cast<char *>(PROTECTED_ENTROPY)) };
	DATA_BLOB out = { 0, nullptr };

	if (!CryptProtectData(&in, L"Winclipper", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
	{
		throw std::exception("Could not encrypt clips.");
	}

	std::string file(PROTECTED_MAGIC, PROTECTED_MAGIC_LEN);
	file.append(reinterpret_cast<const char *>(out.pbData), out.cbData);
	LocalFree(out.pbData);

	Write(filename, file);
}

// Reads a file written by WriteProtected and returns the decrypted data.
// A file without the marker is returned as-is with wasPlaintext set, so
// the caller can rewrite it encrypted. Throws if decryption fails, e.g.
// because the file was tampered with or belongs to another user.
std::string File::ReadProtected(std::wstring filename, bool & wasPlaintext)
{
	std::string file = Read(filename);

	if (file.compare(0, PROTECTED_MAGIC_LEN, PROTECTED_MAGIC, PROTECTED_MAGIC_LEN) != 0)
	{
		wasPlaintext = !file.empty();
		return file;
	}
	wasPlaintext = false;

	const size_t blobSize = file.size() - PROTECTED_MAGIC_LEN;
	if (blobSize == 0 || blobSize > MAXDWORD)
	{
		throw std::exception("Encrypted clip data is corrupt.");
	}

	DATA_BLOB in = { static_cast<DWORD>(blobSize), reinterpret_cast<BYTE *>(&file[PROTECTED_MAGIC_LEN]) };
	DATA_BLOB entropy = { sizeof(PROTECTED_ENTROPY) - 1, reinterpret_cast<BYTE *>(const_cast<char *>(PROTECTED_ENTROPY)) };
	DATA_BLOB out = { 0, nullptr };

	if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
	{
		throw std::exception("Encrypted clip data could not be decrypted.");
	}

	std::string data(reinterpret_cast<const char *>(out.pbData), out.cbData);
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
	return data;
}

// Writes all lines in a vector to a text file
void File::WriteAllLines(const wchar_t* name, std::vector<std::wstring> lines)
{
    if (!File::Exists(name))
    {
        SHCreateDirectoryExW(nullptr, (GetDirName(name).c_str()), nullptr);
    }
    std::wofstream output_file(name);
    std::ostream_iterator<std::wstring, wchar_t> output_iterator(output_file, L"\n");
    std::copy(lines.begin(), lines.end(), output_iterator);
}

// Reads newline delimited values from a file into a vector
std::vector<std::wstring> File::ReadAllLines(const wchar_t * name)
{
    std::vector<std::wstring> lines;
    std::wifstream myfile(name);

    // Whole lines, not whitespace-separated words: values may contain spaces.
    std::wstring line;
    while (std::getline(myfile, line))
    {
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            lines.push_back(line);
        }
    }
    return lines;
}

std::wstring File::appDir;
