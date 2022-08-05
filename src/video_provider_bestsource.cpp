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

public:
	BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br);

	void GetFrame(int n, VideoFrame &out) override;

	void SetColorSpace(std::string const& matrix) override { }

	int GetFrameCount() const override { return properties.NumFrames; };

	int GetWidth() const override { return properties.Width; };
	int GetHeight() const override { return properties.Height; };
	double GetDAR() const override { return (properties.Width * properties.SAR.Num) / (properties.Height * properties.SAR.Den); };

	agi::vfr::Framerate GetFPS() const override { return agi::vfr::Framerate(properties.FPS.Num, properties.FPS.Den); }; // TODO figure out VFR with bs
	std::string GetColorSpace() const override { return "TV.709"; }; 	// TODO
	std::string GetRealColorSpace() const override { return "TV.709"; };
	std::vector<int> GetKeyFrames() const override { return std::vector<int>(); };
	std::string GetDecoderName() const override { return "BestSource"; };
	bool WantsCaching() const override { return false; };
	bool HasAudio() const override { return false; };
};

BSVideoProvider::BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br) try
: bsopts()
, bs(filename.string(), "", 0, false, 0, "", &bsopts)
{
	properties = bs.GetVideoProperties();

	if (properties.NumFrames == -1)
	{
	    LOG_D("bs") << "File not cached or varying samples, creating cache.";
	    br->Run([&](agi::ProgressSink *ps) {
	        ps->SetTitle(from_wx(_("Exacting")));
	        ps->SetMessage(from_wx(_("Creating cache... This can take a while!")));
	        ps->SetIndeterminate();
	        if (bs.GetExactDuration())
	        {
	            LOG_D("bs") << "File cached and has exact samples.";
	        }
	    });
	    properties = bs.GetVideoProperties();
	}

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
	BestVideoFrame *frame = bs.GetFrame(n);
	out.width = GetWidth();
	out.height = GetHeight();
	out.pitch = GetWidth() * 4;
	out.flipped = false;

	out.data.resize(out.width * out.height * 4);
	unsigned char *dst = &out.data[0];

	switch (properties.VF.ColorFamily)
	{
	case 0:
	    throw VideoOpenError("Unknown Color Family.");
	case 1:
	    throw VideoOpenError("Greyscale not supported.");
	case 2: // RGB
	{
		uint8_t *PlrPtrs[3] = {};
		PlrPtrs[0] = new uint8_t[out.width * out.height];
		PlrPtrs[1] = new uint8_t[out.width * out.height];
		PlrPtrs[2] = new uint8_t[out.width * out.height];
		ptrdiff_t PlrStride[3] = { (ptrdiff_t) out.width, (ptrdiff_t) out.width, (ptrdiff_t) out.width };

		if (!frame->ExportAsPlanar(PlrPtrs, PlrStride)) {
			throw VideoOpenError("Couldn't unpack frame!");
		}

		for (size_t py = 0; py < out.height; py++) {
			for (size_t px = 0; px < out.width; px++) {
				*dst++ = PlrPtrs[1][py * out.width + px];
				*dst++ = PlrPtrs[0][py * out.width + px];
				*dst++ = PlrPtrs[2][py * out.width + px];
				*dst++ = 0;
			}
		}

		break;
	}
	case 3: // yuv
	{
	    // however, it's not so easy for YUV!
	    // I believe this needs yuv to rgb conversion, bestsource does not do that with p2p.
	    // (I think you can steal that from yuv4mpeg video provider.)
	    throw VideoOpenError("YUV not supported.");
	    break;
	}    
	default:
	    throw VideoOpenError("Unknown ColorFamily");
	}

	// Now to somehow go from *frame to &out...
}

}

std::unique_ptr<VideoProvider> CreateBSVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *br) {
	return agi::make_unique<BSVideoProvider>(path, colormatrix, br);
}

#endif /* WITH_BESTSOURCE */
