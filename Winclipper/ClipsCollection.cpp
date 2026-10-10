#include "stdafx.h"
#include "ClipsCollection.h"


ClipsCollection::ClipsCollection() noexcept
{
}


ClipsCollection::~ClipsCollection()
{
	clips.clear();
}

/* ISerializable member function implementations */

std::string ClipsCollection::Serialize()
{
	// Serialize a copy so the UI thread is not blocked while clips are encoded.
	std::string data;
	for (auto clip : Snapshot())
	{
		try
		{
			data.append(clip->Serialize());
			data.append("\r\n");
		}
		catch (const std::exception&)
		{
			// no op because maybe we just ran into a weird data edge case.
			// we wouldn't want to save that to the clipboard anyway.
		}	
	}
	return data;
}

void ClipsCollection::Deserialize(std::string serializationData)
{
	const size_t dataLength = serializationData.length();
	if (dataLength != 0)
	{
		std::istringstream stream(serializationData);

		std::string readData;

		unsigned int corruptedClips = 0;

		while (std::getline(stream, readData))
		{
			std::shared_ptr<Clip> clip = std::make_shared<Clip>();

			try
			{
				clip->Deserialize(readData);
				if (clip->AnyFormats())
				{
					std::lock_guard<std::mutex> lock(clipsMutex);
					clips.push_back(clip);
				}
			}
			catch (const std::exception&)
			{
				clip->MarkForDelete();
				corruptedClips++;
			}
		}

		if (corruptedClips > 0)
		{
			std::string message(std::to_string(corruptedClips));
			if (corruptedClips == 1)
			{
				message += " clip was corrupt and was cleaned up.";
			}
			else
			{
				message += " clips were corrupt and were cleaned up.";
			}
			throw std::exception(message.c_str());
		}
	}
}

/* End ISerializable implementation */

void ClipsCollection::AddFront(std::shared_ptr<Clip> clip)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	if (clips.empty())
	{
		clips.push_front(clip);
	}
	else
	{
		if (!clip->Equals(clips.front()))
		{
			clips.push_front(clip);
			TrimToMaxSize();
		}
		else
		{
			clip->MarkForDelete();
		}
	}
}

void ClipsCollection::RemoveAllOfFormat(DWORD format)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	if (format != 0 && !clips.empty())
	{
		std::deque<std::shared_ptr<Clip>>::iterator it = clips.begin()++;
		while (it != clips.end())
		{
			std::shared_ptr<Clip> clip = *it;
			if (clip->ContainsFormat(format))
			{
				clip->MarkForDelete();
				it = clips.erase(it);
			}
			else
			{
				it++;
			}
		}
	}
}

std::shared_ptr<Clip> ClipsCollection::RemoveAt(unsigned int index)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	std::shared_ptr<Clip> clip(nullptr);

	if (!clips.empty() && index < clips.size())
	{
		clip = clips.at(index);
		clips.erase(clips.begin() + index);
	}

	return clip;
}

std::shared_ptr<Clip> ClipsCollection::At(size_t index)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	std::shared_ptr<Clip> clip(nullptr);

	if (!clips.empty() && index < clips.size())
	{
		clip = clips.at(index);
	}

	return clip;
}

std::shared_ptr<Clip> ClipsCollection::Front()
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	return clips.empty() ? nullptr : clips.front();
}

unsigned int ClipsCollection::Size() noexcept
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	return static_cast<unsigned int>(clips.size());
}

unsigned int ClipsCollection::MaxSize() noexcept
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	return maxSize;
}

void ClipsCollection::SetMaxSize(unsigned int newMaxSize)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	if (newMaxSize != maxSize && newMaxSize > 1)
	{
		maxSize = newMaxSize;
		TrimToMaxSize();
	}
}

void ClipsCollection::Clear()
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	const size_t size = clips.size();
	for (size_t i = 0; i < size; i++)
	{
		clips.at(i)->MarkForDelete();
	}
	clips.clear();
}

void ClipsCollection::PurgeThumbnails()
{
	for (auto clip : Snapshot())
	{
		clip->PurgeThumbnail();
	}
}

void ClipsCollection::LoadAllResources()
{
	// Work on a copy: loading reads from disk and must not hold the lock.
	for (auto clip : Snapshot())
	{
		if (clip->ContainsFormat(CF_DIB))
		{
			auto bitmap = clip->EnsureBitmap();
			clip->GetThumbnail();
		}
	}
}

std::deque<std::shared_ptr<Clip>> ClipsCollection::Snapshot()
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	return clips;
}

// Caller must hold clipsMutex.
void ClipsCollection::TrimToMaxSize()
{
	while (clips.size() > maxSize)
	{
		clips.back()->MarkForDelete();
		clips.pop_back();
	}
}
