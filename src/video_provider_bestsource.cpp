// Copyright (c) 2022, arch1t3cht <arch1t3cht@gmail.com>>
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

/// @file video_provider_lsmashsource.cpp
/// @brief BestSource-based video provider
/// @ingroup video_input bestsource
///

#ifdef WITH_BESTSOURCE
#include "include/aegisub/video_provider.h"

#include "videosource.h"
#include "BSRational.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

/* #include "options.h" */
/* #include "utils.h" */
#include "compat.h"
#include "video_frame.h"
namespace agi { class BackgroundRunner; }

#include <libaegisub/fs.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/background_runner.h>
#include <libaegisub/log.h>

namespace {

/// @class BSVideoProvider
/// @brief Implements video loading through BestSource.
class BSVideoProvider final : public VideoProvider {
	std::map<std::string, std::string> bsopts;
	BestVideoSource bs;
	VideoProperties properties;

	std::vector<int> Keyframes;
	agi::vfr::Framerate Timecodes;

public:
	BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br);

	void GetFrame(int n, VideoFrame &out) override;

	void SetColorSpace(std::string const& matrix) override { }

	int GetFrameCount() const override { return properties.NumFrames; };

	int GetWidth() const override { return properties.Width; };
	int GetHeight() const override { return properties.Height; };
	double GetDAR() const override { return (properties.Width * properties.SAR.Num) / (properties.Height * properties.SAR.Den); };

	agi::vfr::Framerate GetFPS() const override { return Timecodes; };
	std::string GetColorSpace() const override { return "TV.709"; }; 	// TODO
	std::string GetRealColorSpace() const override { return "TV.709"; };
	std::vector<int> GetKeyFrames() const override { return Keyframes; };
	std::string GetDecoderName() const override { return "BestSource"; };
	bool WantsCaching() const override { return false; };
	bool HasAudio() const override { return false; };
};

BSVideoProvider::BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br) try
: bsopts()
, bs(filename.string(), "", -1, false, 0, "", &bsopts)
{
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

			std::vector<int> TimecodesVector;
			for (int n = 0; n < properties.NumFrames; n++) {
				std::unique_ptr<BestVideoFrame> frame(bs.GetFrame(n));
				if (frame == nullptr) {
					throw VideoOpenError("Couldn't read frame!");
				}
				
				if (frame->GetAVFrame()->key_frame) {
					Keyframes.push_back(n);
				}

				TimecodesVector.push_back((int) frame->GetAVFrame()->pts);
			}

			if (TimecodesVector.size() < 2) {
				Timecodes = 25.0;
			} else {
				Timecodes = agi::vfr::Framerate(TimecodesVector);
			}
	    });
	    properties = bs.GetVideoProperties();
	}

	br->Run([&](agi::ProgressSink *ps) {
		ps->SetTitle(from_wx(_("Scanning")));
		ps->SetMessage(from_wx(_("Finding Keyframes and Timecodes...")));

		std::vector<int> TimecodesVector;
		for (int n = 0; n < properties.NumFrames; n++) {
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

		if (TimecodesVector.size() < 2) {
			Timecodes = 25.0;
		} else {
			Timecodes = agi::vfr::Framerate(TimecodesVector);
		}
	});

	LOG_D("bs") << "Loaded!";
	LOG_D("bs") << "Duration: " << properties.Duration;
	LOG_D("bs") << "FrameCount: " << properties.NumFrames;
	LOG_D("bs") << "PixFmt: " << properties.PixFmt;
	LOG_D("bs") << "ColorFamily: " << properties.VF.ColorFamily;
	LOG_D("bs") << "Bits: " << properties.VF.Bits;
}
catch (agi::EnvironmentError const& err) {
	throw VideoOpenError(err.GetMessage());
}

void BSVideoProvider::GetFrame(int n, VideoFrame &out) {
	std::unique_ptr<BestVideoFrame> bsframe(bs.GetFrame(n));
	if (bsframe == nullptr) {
		throw VideoOpenError("Couldn't read frame!");
	}
	const AVFrame *frame = bsframe->GetAVFrame();
	AVFrame *newframe = av_frame_alloc();

	SwsContext *context = sws_getContext(
			frame->width, frame->height, (AVPixelFormat) frame->format,  	// TODO figure out aegi's color space forcing.
			frame->width, frame->height, AV_PIX_FMT_BGR0,
			SWS_BICUBIC, nullptr, nullptr, nullptr);

	if (context == nullptr) {
		throw VideoOpenError("Couldn't convert frame!");
	}

	sws_scale_frame(context, newframe, frame);

	out.width = newframe->width;
	out.height = newframe->height;
	out.pitch = newframe->width * 4;
	out.flipped = false; 		// TODO figure out flipped

	out.data.assign(newframe->data[0], newframe->data[0] + newframe->linesize[0] * newframe->height);

	av_frame_free(&newframe);
}

}

std::unique_ptr<VideoProvider> CreateBSVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *br) {
	return agi::make_unique<BSVideoProvider>(path, colormatrix, br);
}

#endif /* WITH_BESTSOURCE */
