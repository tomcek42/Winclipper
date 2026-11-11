#include "stdafx.h"
#include "ClipsCollection.h"
#include <algorithm>


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
			// Serialize ends each clip with CR LF but getline only strips LF.
			if (!readData.empty() && readData.back() == '\r')
			{
				readData.pop_back();
			}
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

		NormalizePinOrder();

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

// clips.dat is untrusted: pin records may appear in any order, so restore the
// pinned-block invariant after loading instead of relying on the file's order.
void ClipsCollection::NormalizePinOrder()
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	std::stable_partition(clips.begin(), clips.end(),
		[](const std::shared_ptr<Clip>& clip) { return clip->IsPinned(); });
}

/* End ISerializable implementation */

void ClipsCollection::AddFront(std::shared_ptr<Clip> clip)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	const size_t pinnedCount = PinnedCountLocked();

	// New clips go in front of the unpinned clips, below the pinned block.
	// Skip the clip if it repeats the newest unpinned clip or any pinned clip.
	bool duplicate = false;
	for (size_t i = 0; i < clips.size() && i <= pinnedCount && !duplicate; i++)
	{
		try
		{
			duplicate = clip->Equals(clips.at(i));
		}
		catch (const std::exception&)
		{
			// A clip whose cached bitmap cannot be loaded is not a duplicate.
		}
	}

	if (duplicate)
	{
		clip->MarkForDelete();
		return;
	}

	clips.insert(clips.begin() + pinnedCount, clip);
	TrimToMaxSize();
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

// Pins an unpinned clip (it goes to the end of the pinned block) or unpins a
// pinned one (it becomes the newest unpinned clip). Returns false if the
// index is out of range.
bool ClipsCollection::TogglePinAt(unsigned int index)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	if (index >= clips.size())
	{
		return false;
	}

	std::shared_ptr<Clip> clip = clips.at(index);
	clips.erase(clips.begin() + index);
	clip->SetPinned(!clip->IsPinned());
	clips.insert(clips.begin() + PinnedCountLocked(), clip);
	return true;
}

unsigned int ClipsCollection::PinnedCount()
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	return static_cast<unsigned int>(PinnedCountLocked());
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

void ClipsCollection::Clear(bool includePinned)
{
	std::lock_guard<std::mutex> lock(clipsMutex);
	const size_t pinnedCount = includePinned ? 0 : PinnedCountLocked();
	for (size_t i = pinnedCount; i < clips.size(); i++)
	{
		clips.at(i)->MarkForDelete();
	}
	clips.erase(clips.begin() + pinnedCount, clips.end());
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

// Caller must hold clipsMutex. Pinned clips are never trimmed; if they alone
// reach the limit, the newest unpinned clip is still kept.
void ClipsCollection::TrimToMaxSize()
{
	const size_t limit = (std::max)(static_cast<size_t>(maxSize), PinnedCountLocked() + 1);
	while (clips.size() > limit)
	{
		clips.back()->MarkForDelete();
		clips.pop_back();
	}
}

// Caller must hold clipsMutex.
size_t ClipsCollection::PinnedCountLocked() const
{
	size_t count = 0;
	while (count < clips.size() && clips.at(count)->IsPinned())
	{
		count++;
	}
	return count;
}
