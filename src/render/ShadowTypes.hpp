#pragma once

// CSM (Cascaded Shadow Maps) removed per TODO.md §5.2.D (session 20x).
// RTX shadows are the canonical sun shadow path; the only remaining
// shadow-related contracts in this file are the contact-shadow and
// local-point-light shadow policy enum used by voxel.frag forward paths.
// The contact / local-shadow policy stays until those forward-shader effects
// migrate to RTX in Milestone 5.4+ / 5.6.

// Constants removed: kSunShadowCascadeCount (4), kSunShadowMatrixElementCount (64).
// Forward paths that referenced kSunShadowCascadeCount are now 1-cascade
// (no cascades) since CSM is gone.

enum class TransparentShadowPolicy : uint8_t {
	GlassIgnoredFluidCasts = 0,
};

inline const char *TransparentShadowPolicyToString(const TransparentShadowPolicy policy)
{
	switch (policy) {
	case TransparentShadowPolicy::GlassIgnoredFluidCasts:
	default:
		return "GLASS_IGNORED_FLUID_CASTS";
	}
}
