#include "stdafx.h"
#include "Bitmap.h"


Bitmap::Bitmap(std::shared_ptr<BITMAPINFOHEADER> pBmiHeader, std::vector<RGBQUAD> quads, std::shared_ptr<BYTE> bits)
{
	pDibBitmapInfoHeader = pBmiHeader;
	rgbQuadCollection = quads;
	pDibBitmapBits = bits;
}

Bitmap::Bitmap() noexcept
{
}

Bitmap::~Bitmap()
{
	if (hBitmap != nullptr)
	{
		DeleteObject(hBitmap);
	}
}

/* ISerializable member function implementations */

std::string Bitmap::Serialize()
{
	std::string data;

	const DWORD bmSize = Size();
	BITMAPFILEHEADER header;
	header.bfType = 0x4d42;
	header.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + pDibBitmapInfoHeader->biSize + (rgbQuadCollection.size() * sizeof(RGBQUAD)));
	header.bfReserved1 = 0;
	header.bfReserved2 = 0;
	header.bfSize = header.bfOffBits + bmSize;

	// reinterpret_cast should be fine in these situations
	// since we are reinterpreting the values as binary data.
	data.append(reinterpret_cast<const char *>(&header),
		reinterpret_cast<const char *>(&header) + sizeof(BITMAPFILEHEADER));
	data.append(reinterpret_cast<const char *>(pDibBitmapInfoHeader.get()),
		reinterpret_cast<const char *>(pDibBitmapInfoHeader.get()) + sizeof(BITMAPINFOHEADER));

	if (rgbQuadCollection.size() > 0)
	{
		data.append(reinterpret_cast<const char *>(rgbQuadCollection.data()),
				reinterpret_cast<const char *>(rgbQuadCollection.data()) + (rgbQuadCollection.size() * sizeof(RGBQUAD)));
	}

	data.append(reinterpret_cast<const char *>(pDibBitmapBits.get()),
		reinterpret_cast<const char *>(pDibBitmapBits.get()) + bmSize);

	return data;
}

void Bitmap::Deserialize(std::string serializationData)
{
	// The data comes from BitmapCache\ and is untrusted: every field is
	// validated before it drives an allocation or a read.
	const size_t dataLength = serializationData.length();
	if (dataLength == 0)
	{
		throw std::exception("Image data is empty or missing.");
	}

	const char * data = serializationData.data();
	size_t offset = 0;

	BITMAPFILEHEADER header;
	auto bmih = std::make_shared<BITMAPINFOHEADER>();
	if (dataLength < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
	{
		throw std::exception("Image data is corrupt and could not be loaded.");
	}
	memcpy(&header, data + offset, sizeof(BITMAPFILEHEADER));
	offset += sizeof(BITMAPFILEHEADER);
	memcpy(bmih.get(), data + offset, sizeof(BITMAPINFOHEADER));
	offset += sizeof(BITMAPINFOHEADER);

	// Only a plain BITMAPINFOHEADER is stored; a larger biSize from the file
	// would make CreateDIBitmap read past it.
	bmih->biSize = sizeof(BITMAPINFOHEADER);

	const WORD bitCount = bmih->biBitCount;
	if (header.bfType != 0x4d42
		|| bmih->biWidth <= 0
		|| bmih->biHeight == 0
		|| bmih->biHeight == LONG_MIN
		|| bmih->biPlanes != 1
		|| (bitCount != 1 && bitCount != 4 && bitCount != 8
			&& bitCount != 16 && bitCount != 24 && bitCount != 32)
		|| (bmih->biCompression != BI_RGB && bmih->biCompression != BI_BITFIELDS))
	{
		throw std::exception("Image data is corrupt and could not be loaded.");
	}

	unsigned int colorBytes = bmih->biClrUsed;
	if (!colorBytes)
	{
		if (bitCount <= 8)
		{
			colorBytes = 1u << bitCount;
		}
		else if (bitCount != 24 && bmih->biCompression == BI_BITFIELDS)
		{
			colorBytes = 3;
		}
	}
	const unsigned int maxColors = bitCount <= 8 ? (1u << bitCount) : 256u;
	if (colorBytes > maxColors
		|| colorBytes * sizeof(RGBQUAD) > dataLength - offset)
	{
		throw std::exception("Image data is corrupt and could not be loaded.");
	}

	std::vector<RGBQUAD> quads(colorBytes);
	if (colorBytes > 0)
	{
		memcpy(quads.data(), data + offset, colorBytes * sizeof(RGBQUAD));
		offset += colorBytes * sizeof(RGBQUAD);
	}

	// The buffer must cover what CreateDIBitmap reads (derived from the
	// header), and the file must actually contain that many bytes.
	const unsigned long long minimumSize = ComputeImageSize(*bmih);
	const unsigned long long bmSize = bmih->biSizeImage > minimumSize ? bmih->biSizeImage : minimumSize;
	if (minimumSize == 0 || bmSize > dataLength - offset)
	{
		throw std::exception("Image data is corrupt and could not be loaded.");
	}
	bmih->biSizeImage = static_cast<DWORD>(bmSize);

	std::shared_ptr<BYTE> bits(new BYTE[static_cast<size_t>(bmSize)], array_deleter<BYTE>());
	memcpy(bits.get(), data + offset, static_cast<size_t>(bmSize));

	pDibBitmapInfoHeader = bmih;
	rgbQuadCollection = quads;
	pDibBitmapBits = bits;

	GetHbitmap();
}

/* End ISerializable implementation */

const std::shared_ptr<BITMAPINFOHEADER> Bitmap::DibBitmapInfoHeader() noexcept
{
	return pDibBitmapInfoHeader;
}

const std::vector<RGBQUAD> Bitmap::RgbQuadCollection()
{
	if (rgbQuadCollection.empty())
	{
		return std::vector<RGBQUAD>();
	}
	return rgbQuadCollection;
}

const std::shared_ptr<BYTE> Bitmap::DibBitmapBits() noexcept
{
	return pDibBitmapBits;
}

// Creates an HBITMAP for the bitmap.
// It is the responibility of the caller
// to call DeleteObject on the HBITMAP.
HBITMAP Bitmap::GetHbitmap()
{
	if (hBitmap == nullptr)
	{
		HDC screen = GetDC(nullptr);
		BITMAPINFO * bmi = reinterpret_cast<BITMAPINFO *>(new BYTE[pDibBitmapInfoHeader->biSize + rgbQuadCollection.size() * sizeof(RGBQUAD)]);

		bmi->bmiHeader = *pDibBitmapInfoHeader;
		for (size_t i = 0; i < rgbQuadCollection.size(); i++)
		{
			bmi->bmiColors[i] = rgbQuadCollection[i];
		}

		hBitmap = CreateDIBitmap(
			screen,
			pDibBitmapInfoHeader.get(),
			CBM_INIT,
			pDibBitmapBits.get(),
			bmi,
			DIB_RGB_COLORS
		);

		delete[] reinterpret_cast<BYTE *>(bmi);
		ReleaseDC(nullptr, screen);
	}
	return hBitmap;
}

// Returns the uncompressed size of the pixel data described by the header,
// or 0 if it does not fit in a DWORD.
unsigned long long Bitmap::ComputeImageSize(const BITMAPINFOHEADER & bmih) noexcept
{
	if (bmih.biWidth <= 0 || bmih.biHeight == 0 || bmih.biHeight == LONG_MIN)
	{
		return 0;
	}
	const unsigned long long stride = ((static_cast<unsigned long long>(bmih.biWidth) * bmih.biBitCount + 31) & ~31ull) >> 3;
	const unsigned long long height = bmih.biHeight < 0
		? static_cast<unsigned long long>(-static_cast<long long>(bmih.biHeight))
		: static_cast<unsigned long long>(bmih.biHeight);
	const unsigned long long size = stride * height;
	return size > MAXDWORD ? 0 : size;
}

// Returns the size of the image in bytes.
DWORD Bitmap::Size() noexcept
{
	DWORD retVal = 0;
	if (pDibBitmapInfoHeader != nullptr)
	{
		retVal = pDibBitmapInfoHeader->biSizeImage
			? pDibBitmapInfoHeader->biSizeImage
			: static_cast<DWORD>(ComputeImageSize(*pDibBitmapInfoHeader));
	}

	return retVal;
}
