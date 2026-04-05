#ifndef PROFILING_HPP
#define PROFILING_HPP

// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstdint>

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
#define PV_PROFILE_ZONE() do { } while (false)
#define PV_PROFILE_ZONE_N(name) do { (void)sizeof(name); } while (false)
#define PV_PROFILE_FRAME_MARK() do { } while (false)
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
	ConfigurePlot("Rebuilt Mesh Vertices", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Scene Triangles", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Vertices", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Chunk Descriptors", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Opaque Vertices", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Uploaded Transparent Vertices", ProfilingPlotFormat::Number, true);
	ConfigurePlot("Upload Vertex Bytes", ProfilingPlotFormat::Memory, true);
	ConfigurePlot("Upload Descriptor Bytes", ProfilingPlotFormat::Memory, true);
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

	if (name) {
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

	if (name) {
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

#endif
