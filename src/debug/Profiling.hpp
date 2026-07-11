#pragma once

#if defined(PROJECTV_ENABLE_TRACY)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <tracy/Tracy.hpp>
#pragma clang diagnostic pop
#endif

enum class ProfilingPlotFormat : uint8_t {
	Number = 0,
	Memory = 1,
	Percentage = 2,
};

#if defined(PROJECTV_ENABLE_TRACY)
#define PV_PROFILE_ZONE() ZoneScoped
#define PV_PROFILE_ZONE_N(name) ZoneScopedN(name)
#define PV_PROFILE_FRAME_MARK() FrameMark
#else
#define PV_PROFILE_ZONE() \
	do {                  \
	} while (false)
#define PV_PROFILE_ZONE_N(name) \
	do {                        \
		(void)sizeof(name);     \
	} while (false)
#define PV_PROFILE_FRAME_MARK() \
	do {                        \
	} while (false)
#endif

namespace profiling {

inline void SetThreadName(const char *name)
{
#if defined(PROJECTV_ENABLE_TRACY)
	tracy::SetThreadName(name);
#else
	(void)name;
#endif
}

inline void ConfigurePlot(
	const char *name,
	const ProfilingPlotFormat format,
	const bool step = false,
	const bool fill = false,
	const uint32_t color = 0)
{
#if defined(PROJECTV_ENABLE_TRACY)
	TracyPlotConfig(name, static_cast<tracy::PlotFormatType>(format), step, fill, color);
#else
	(void)name;
	(void)format;
	(void)step;
	(void)fill;
	(void)color;
#endif
}

inline void ConfigureDefaultPlots()
{
#if defined(PROJECTV_ENABLE_TRACY)
	static bool configured = false;
	if (configured) {
		return;
	}
	configured = true;

	ConfigurePlot("Frame Delta (ms)", ProfilingPlotFormat::Number);
	ConfigurePlot("Simulation Accumulator (ms)", ProfilingPlotFormat::Number);
	ConfigurePlot("Simulation Steps", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Dirty Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Active Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Rebuilt Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Repacked Chunk Voxels", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Scene Triangles", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Generated Opaque Faces", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Generated Transparent Faces", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Meshing Dirty Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Visible Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Culled Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Chunk Voxel Words", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Chunk Descriptors", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Voxel Payload Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Chunk Voxel Words", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Upload Descriptor Bytes", ProfilingPlotFormat::Memory, true);
	ConfigurePlot("Upload Chunk Voxel Bytes", ProfilingPlotFormat::Memory, true);
	ConfigurePlot("Walk Support State", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Support Score", ProfilingPlotFormat::Number);
	ConfigurePlot("Walk Feet Y", ProfilingPlotFormat::Number);
	ConfigurePlot("Walk Velocity Y", ProfilingPlotFormat::Number);
	ConfigurePlot("Walk Sneak Active", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Jump Lock", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Cached Sneak Support", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Feet Inside Sneak Cache", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Edge Grace", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Ground Takeoff Grace", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Sneak Support Grace", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Ledge Release Grace", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Walk Ground Return Anchor", ProfilingPlotFormat::Number, true);
	ConfigurePlot("VCT Voxelize Chunks", ProfilingPlotFormat::Number, true);
	ConfigurePlot("VCT Mip Chain Mips", ProfilingPlotFormat::Number, true);
	ConfigurePlot("VCT Active Mip", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Sky Atmosphere Pass", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Volumetric Fog Pass", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Cloudscape Pass", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Sky LUT Precompute (ms)", ProfilingPlotFormat::Number);
	ConfigurePlot("Physics Sync Full Rebuild", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Physics Sync Incremental", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Physics Sync Skipped", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Fluid CA Cells Read", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Fluid CA Cells Moved", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Fluid Edit Version Bumps Suppressed", ProfilingPlotFormat::Number, true);
#endif
}

inline void PlotValue(const char *name, const int64_t value)
{
#if defined(PROJECTV_ENABLE_TRACY)
	TracyPlot(name, value);
#else
	(void)name;
	(void)value;
#endif
}

inline void PlotValue(const char *name, const float value)
{
#if defined(PROJECTV_ENABLE_TRACY)
	TracyPlot(name, value);
#else
	(void)name;
	(void)value;
#endif
}

inline void PlotValue(const char *name, const double value)
{
#if defined(PROJECTV_ENABLE_TRACY)
	TracyPlot(name, value);
#else
	(void)name;
	(void)value;
#endif
}

inline void RecordAllocation(
	const void *pointer,
	const size_t size,
	const char *name = nullptr)
{
#if defined(PROJECTV_ENABLE_TRACY)
	if (!pointer) {
		return;
	}

	if (name) { // NOLINT(bugprone-branch-clone): named vs default Tracy macros are different
		TracyAllocN(pointer, size, name);
	} else {
		TracyAlloc(pointer, size);
	}
#else
	(void)pointer;
	(void)size;
	(void)name;
#endif
}

inline void RecordFree(
	const void *pointer,
	const char *name = nullptr)
{
#if defined(PROJECTV_ENABLE_TRACY)
	if (!pointer) {
		return;
	}

	if (name) { // NOLINT(bugprone-branch-clone): named vs default Tracy macros are different
		TracyFreeN(pointer, name);
	} else {
		TracyFree(pointer);
	}
#else
	(void)pointer;
	(void)name;
#endif
}

} // namespace profiling

