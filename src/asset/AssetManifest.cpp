#include "asset/AssetManifest.hpp"

#include "core/EnvUtils.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace projectv::asset {

namespace {

std::string_view Trim(const std::string_view s)
{
	std::size_t begin = 0;
	while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
		++begin;
	}
	std::size_t end = s.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
		--end;
	}
	return s.substr(begin, end - begin);
}

std::string DefaultIdStemForPath(const std::string &path)
{
	const std::filesystem::path p(path);
	std::string stem = p.stem().string();
	if (stem.empty()) {
		stem = p.filename().string();
	}
	return stem;
}

bool TryParseFloat(const std::string_view token, float &out)
{
	if (token.empty()) {
		return false;
	}
	const std::string buf(token);
	char *parseEnd = nullptr;
	const float v = std::strtof(buf.c_str(), &parseEnd);
	if (parseEnd != buf.c_str() + buf.size()) {
		return false;
	}
	out = v;
	return true;
}

bool TryParseVec3(const std::string_view tokens, glm::vec3 &out)
{
	float values[3] = {0.0f, 0.0f, 0.0f};
	std::size_t cursor = 0;
	for (int i = 0; i < 3; ++i) {
		const auto comma = tokens.find(',', cursor);
		const std::string_view slice = comma == std::string_view::npos
										   ? tokens.substr(cursor)
										   : tokens.substr(cursor, comma - cursor);
		float v = 0.0f;
		if (!TryParseFloat(Trim(slice), v)) {
			return false;
		}
		values[i] = v;
		if (comma == std::string_view::npos) {
			break;
		}
		cursor = comma + 1;
	}
	out = glm::vec3(values[0], values[1], values[2]);
	return true;
}

bool TryParseFullTransform(
	const std::string_view tokens,
	ManifestEntry &out)
{
	float values[7] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	std::size_t cursor = 0;
	for (int i = 0; i < 7; ++i) {
		const auto comma = tokens.find(',', cursor);
		const std::string_view slice = comma == std::string_view::npos
										   ? tokens.substr(cursor)
										   : tokens.substr(cursor, comma - cursor);
		float v = 0.0f;
		if (!TryParseFloat(Trim(slice), v)) {
			return false;
		}
		values[i] = v;
		if (comma == std::string_view::npos) {
			break;
		}
		cursor = comma + 1;
	}
	out.position = glm::vec3(values[0], values[1], values[2]);
	out.rotationDegrees = glm::vec3(values[3], values[4], values[5]);
	out.scale = values[6];
	return true;
}

bool ParseEntry(const std::string &rawEntry, ManifestEntry &out)
{
	const auto trimmed = Trim(rawEntry);
	if (trimmed.empty()) {
		return false;
	}
	const auto at = trimmed.find('@');
	const std::string_view pathView = at == std::string_view::npos
										  ? trimmed
										  : trimmed.substr(0, at);
	if (pathView.empty()) {
		return false;
	}

	ManifestEntry entry;
	entry.path.assign(pathView);
	entry.id = core::StringID{std::string_view{DefaultIdStemForPath(entry.path)}};
	entry.position = glm::vec3(0.0f);
	entry.rotationDegrees = glm::vec3(0.0f);
	entry.scale = 1.0f;

	const std::string_view transformView = at == std::string_view::npos
											   ? std::string_view{}
											   : trimmed.substr(at + 1);

	bool parsed = false;
	if (transformView.empty()) {
		parsed = true;
	} else {
		const auto commaCount = static_cast<size_t>(std::ranges::count(transformView, ','));
		if (commaCount == 2) {
			parsed = TryParseVec3(transformView, entry.position);
		} else if (commaCount == 6) {
			parsed = TryParseFullTransform(transformView, entry);
		}
	}

	if (parsed) {
		out = std::move(entry);
		return true;
	}
	return false;
}

} // namespace

std::vector<ManifestEntry> ParseAssetManifestString(const std::string &raw)
{
	std::vector<ManifestEntry> entries;
	std::size_t cursor = 0;
	while (cursor < raw.size()) {
		const auto semi = raw.find(';', cursor);
		const auto end = semi == std::string::npos ? raw.size() : semi;
		const std::string token = raw.substr(cursor, end - cursor);
		cursor = semi == std::string::npos ? raw.size() : semi + 1;

		ManifestEntry entry;
		if (ParseEntry(token, entry)) {
			entries.push_back(std::move(entry));
		}
	}
	return entries;
}

std::vector<ManifestEntry> ParseAssetManifestFromEnv()
{
	const char *raw = core::GetEnvVar("PROJECTV_MODELS");
	if (!raw || *raw == '\0') {
		return {};
	}
	return ParseAssetManifestString(std::string(raw));
}

} // namespace projectv::asset
