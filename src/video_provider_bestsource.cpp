// Copyright (c) 2022, arch1t3cht <arch1t3cht@gmail.com>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

/// @file video_provider_bestsource.cpp
/// @brief BestSource-based video provider
/// @ingroup video_input bestsource
///

#ifdef WITH_BESTSOURCE
#include "include/aegisub/video_provider.h"

#include "videosource.h"
#include "audiosource.h"
#include "BSRational.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

/* #include "utils.h" */
#include "options.h"
#include "compat.h"
#include "video_frame.h"
namespace agi { class BackgroundRunner; }

#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/background_runner.h>
#include <libaegisub/log.h>

#include <boost/crc.hpp>
#include <boost/filesystem/path.hpp>

namespace {

/// @class BSVideoProvider
/// @brief Implements video loading through BestSource.
class BSVideoProvider final : public VideoProvider {
	std::map<std::string, std::string> bsopts;
	BestVideoSource bs;
	VideoProperties properties;

	std::vector<int> Keyframes;
	agi::vfr::Framerate Timecodes;
	std::string colorspace;
	bool has_audio = false;

	std::string GetCacheFile(agi::fs::path const& filename);

public:
	BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br);

	void GetFrame(int n, VideoFrame &out) override;

	void SetColorSpace(std::string const& matrix) override { } 	// TODO Follow Aegisub's colorspace forcing?

	int GetFrameCount() const override { return properties.NumFrames; };

	int GetWidth() const override { return properties.Width; };
	int GetHeight() const override { return properties.Height; };
	double GetDAR() const override { return ((double) properties.Width * properties.SAR.Num) / (properties.Height * properties.SAR.Den); };

	agi::vfr::Framerate GetFPS() const override { return Timecodes; };
	std::string GetColorSpace() const override { return colorspace; };
	std::string GetRealColorSpace() const override { return colorspace; };
	std::vector<int> GetKeyFrames() const override { return Keyframes; };
	std::string GetDecoderName() const override { return "BestSource"; };
	bool WantsCaching() const override { return false; };
	bool HasAudio() const override { return has_audio; };
};

// Match the logic from the ffms2 provider, but directly use libavutil's constants and don't abort when encountering an unknown color space
std::string colormatrix_description(const AVFrame *frame) {
	// Assuming TV for unspecified
	std::string str = frame->color_range == AVCOL_RANGE_JPEG ? "PC" : "TV";
	LOG_D("bestsource") << frame->colorspace;

	switch (frame->colorspace) {
		case AVCOL_SPC_BT709:
			return str + ".709";
		case AVCOL_SPC_FCC:
			return str + ".FCC";
		case AVCOL_SPC_BT470BG:
		case AVCOL_SPC_SMPTE170M:
			return str + ".601";
		case AVCOL_SPC_SMPTE240M:
			return str + ".240M";
		default:
			return "None";
	}
}

BSVideoProvider::BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br) try
: bsopts()
, bs(filename.string(), "", -1, false, OPT_GET("Provider/Video/BestSource/Threads")->GetInt(), GetCacheFile(filename), &bsopts)
{
	bs.SetMaxCacheSize(OPT_GET("Provider/Video/BestSource/Max Cache Size")->GetInt() << 20);
	bs.SetSeekPreRoll(OPT_GET("Provider/Video/BestSource/Seek Preroll")->GetInt());
	try {
		BestAudioSource dummysource(filename.string(), -1, 0, "");
		has_audio = true;
	} catch (AudioException const& err) {
		has_audio = false;
	}

	properties = bs.GetVideoProperties();

	if (properties.NumFrames == -1) {
	    LOG_D("bs") << "File not cached or varying samples, creating cache.";
	    br->Run([&](agi::ProgressSink *ps) {
	        ps->SetTitle(from_wx(_("Exacting")));
	        ps->SetMessage(from_wx(_("Creating cache... This can take a while!")));
	        ps->SetIndeterminate();
	        if (bs.GetExactDuration()) {
	            LOG_D("bs") << "File cached and has exact samples.";
	        }
	    });
	    properties = bs.GetVideoProperties();
	}

	br->Run([&](agi::ProgressSink *ps) {
		ps->SetTitle(from_wx(_("Scanning")));
		ps->SetMessage(from_wx(_("Finding Keyframes and Timecodes...")));

		std::vector<int> TimecodesVector;
		for (int n = 0; n < properties.NumFrames; n++) {
			if (ps->IsCancelled()) {
				return;
			}
			std::unique_ptr<BestVideoFrame> frame(bs.GetFrame(n));
			if (frame == nullptr) {
				throw VideoOpenError("Couldn't read frame!");
			}

			if (frame->GetAVFrame()->key_frame) {
				Keyframes.push_back(n);
			}

			TimecodesVector.push_back((int) frame->GetAVFrame()->pts);
			ps->SetProgress(n, properties.NumFrames);
		}

		if (TimecodesVector.size() < 2 || TimecodesVector.front() == TimecodesVector.back()) {
			Timecodes = (double) properties.FPS.Num / properties.FPS.Den;
		} else {
			Timecodes = agi::vfr::Framerate(TimecodesVector);
		}
	});

	// Decode the first frame to get the color space
	std::unique_ptr<BestVideoFrame> frame(bs.GetFrame(0));
	colorspace = colormatrix_description(frame->GetAVFrame());
}
catch (VideoException const& err) {
	throw VideoOpenError("Failed to create BestVideoSource");
}

std::string BSVideoProvider::GetCacheFile(agi::fs::path const& filename) {
	// BS can store all its index data in a single file, but we make a separate index file
	// for each video file to ensure that the old index is invalidated if the file is modified.
	// While BS does check the filesize of the files, it doesn't check the modification time.
	uintmax_t len = agi::fs::Size(filename);
	boost::crc_32_type hash;
	hash.process_bytes(filename.string().c_str(), filename.string().size());

	auto result = config::path->Decode("?local/bsindex/" + filename.filename().string() + "_" + std::to_string(hash.checksum()) + "_" + std::to_string(len) + "_" + std::to_string(agi::fs::ModifiedTime(filename)) + ".json");
	agi::fs::CreateDirectory(result.parent_path());

	return result.string();
}

void BSVideoProvider::GetFrame(int n, VideoFrame &out) {
	std::unique_ptr<BestVideoFrame> bsframe(bs.GetFrame(n));
	if (bsframe == nullptr) {
		throw VideoDecodeError("Couldn't read frame!");
	}
	const AVFrame *frame = bsframe->GetAVFrame();

	SwsContext *context = sws_getContext(
			frame->width, frame->height, (AVPixelFormat) frame->format,  	// TODO figure out aegi's color space forcing.
			frame->width, frame->height, AV_PIX_FMT_BGR0,
			SWS_BICUBIC, nullptr, nullptr, nullptr);

	if (context == nullptr) {
		throw VideoDecodeError("Couldn't convert frame!");
	}

	out.data.resize(frame->width * frame->height * 4);
	uint8_t *data[1] = {&out.data[0]};
	int stride[1] = {frame->width * 4};
	sws_scale(context, frame->data, frame->linesize, 0, frame->height, data, stride);

	out.width = frame->width;
	out.height = frame->height;
	out.pitch = stride[0];
	out.flipped = false; 		// TODO figure out flipped

	sws_freeContext(context);
}

}

std::unique_ptr<VideoProvider> CreateBSVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *br) {
	return agi::make_unique<BSVideoProvider>(path, colormatrix, br);
}

#endif /* WITH_BESTSOURCE */
