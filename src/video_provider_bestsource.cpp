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

/* #include "options.h" */
/* #include "utils.h" */
/* #include "video_frame.h" */
namespace agi { class BackgroundRunner; }

#include <libaegisub/fs.h>
#include <libaegisub/make_unique.h>

namespace {

/// @class BSVideoProvider
/// @brief Implements video loading through BestSource.
class BSVideoProvider final : public VideoProvider {
	/* BestVideoSource bs; */

public:
	BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br);

	void GetFrame(int n, VideoFrame &out) override { };

	void SetColorSpace(std::string const& matrix) override { }

	int GetFrameCount() const override { return 0; };

	int GetWidth() const override { return 0; };
	int GetHeight() const override { return 0; };
	double GetDAR() const override { return 0; };

	agi::vfr::Framerate GetFPS() const override { return agi::vfr::Framerate(std::vector<int>()); };
	std::string GetColorSpace() const override { return ""; };
	std::string GetRealColorSpace() const override { return ""; };
	std::vector<int> GetKeyFrames() const override { return std::vector<int>(); };
	std::string GetDecoderName() const override { return ""; };
	bool WantsCaching() const override { return false; };
	bool HasAudio() const override { return false; };
};

BSVideoProvider::BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br) try
{

}
catch (agi::EnvironmentError const& err) {
	throw VideoOpenError(err.GetMessage());
}
}

std::unique_ptr<VideoProvider> CreateBSVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *br) {
	return agi::make_unique<BSVideoProvider>(path, colormatrix, br);
}

#endif /* WITH_BESTSOURCE */
