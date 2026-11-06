#include "AssetDocument.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include "serialization/ParticlePresetFile.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

//=============================================================================================//
// 実装内補助定義

namespace {
	uint32_t ReadUint32(const std::string& bytes, size_t offset) {
		// 4バイトを順に合成し、32bitの符号なし整数を読み取る。
		uint32_t value = 0;

		for (size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
			value |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + byteIndex])) << (8 * byteIndex);
		}

		return value;
	}

	void WriteUint32(std::string& bytes, uint32_t value) {
		// 32bitの値を、下位のバイトから順に書き出す。
		for (size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
			bytes.push_back(static_cast<char>((value >> (8 * byteIndex)) & 255));
		}
	}

	int HexValue(char character) {
		// 16進数の文字を、対応する数値へ変換する。
		if (character >= '0' && character <= '9') {
			return character - '0';
		}

		if (character >= 'a' && character <= 'f') {
			return character - 'a' + 10;
		}

		if (character >= 'A' && character <= 'F') {
			return character - 'A' + 10;
		}

		return -1;
	}

	std::string EncodeUri(const std::string& text) {
		// URIの参照先として使えるよう、必要なバイトを符号化する。
		static constexpr char hexadecimal[] = "0123456789ABCDEF";

		std::string result;

		for (unsigned char character : text) {
			if (((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')) || character == '/' || character == '.' || character == '-' || character == '_' || character == '~') {
				result.push_back(static_cast<char>(character));
			} else {
				result.push_back('%');

				result.push_back(hexadecimal[character >> 4]);

				result.push_back(hexadecimal[character & 15]);
			}
		}

		return result;
	}

	bool IsNumber(const std::string& token) {
		// 文字列全体を数値として読み取れるか確認する。
		char* end = nullptr;

		std::strtof(token.c_str(), &end);

		return end != token.c_str() && *end == '\0';
	}

	bool SkipTextureOptions(std::istringstream& input, std::string& error) {
		// MTLのテクスチャ指定オプションを解析し、ファイル名の開始位置を求める。
		while (input.peek() == '-') {
			std::string option;

			input >> option;

			if (option == "-s" || option == "-o" || option == "-t") {
				size_t values = 0;

				while (values < 3) {
					input >> std::ws;

					const std::streampos start = input.tellg();

					std::string value;

					input >> value;

					if (!input || !IsNumber(value)) {
						input.clear();

						input.seekg(start);

						break;
					}

					++values;
				}

				if (values == 0) {
					error = "Invalid Wavefront numeric texture option.";

					return false;
				}
			} else {
				size_t count = 1;

				if (option == "-mm") {
					count = 2;
				} else if (option != "-blendu" && option != "-blendv" && option != "-boost" && option != "-cc" &&
					option != "-clamp" && option != "-imfchan" && option != "-texres" && option != "-bm" &&
					option != "-type" && option != "-colorspace") {
					error = "Unsupported Wavefront texture option: " + option;

					return false;
				}

				for (size_t argumentIndex = 0; argumentIndex < count; ++argumentIndex) {
					std::string argument;

					if (!(input >> std::quoted(argument))) {
						error = "Incomplete Wavefront texture option.";

						return false;
					}
				}
			}

			input >> std::ws;
		}

		return true;
	}

	bool ReadUnicode(const std::string& text, size_t& position, uint32_t& codepoint) {
		// 16進数で表されたUnicodeの文字コードを読み取る。
		codepoint = 0;

		for (size_t digitIndex = 0; digitIndex < 4; ++digitIndex) {
			if (position >= text.size()) {
				return false;
			}

			const int digit = HexValue(text[position++]);

			if (digit < 0) {
				return false;
			}

			codepoint = codepoint * 16 + static_cast<uint32_t>(digit);
		}

		return true;
	}

	void AppendUnicode(std::string& result, uint32_t codepoint) {
		// Unicodeの文字コードをUTF-8のバイト列として追加する。
		if (codepoint <= 0x7f) {
			result.push_back(static_cast<char>(codepoint));

		} else if (codepoint <= 0x7ff) {
			result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
			result.push_back(static_cast<char>(0x80 | (codepoint & 63)));

		} else if (codepoint <= 0xffff) {
			result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
			result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
			result.push_back(static_cast<char>(0x80 | (codepoint & 63)));

		} else {
			result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
			result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 63)));
			result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
			result.push_back(static_cast<char>(0x80 | (codepoint & 63)));

		}
	}

	bool ReadJsonString(const std::string& text, size_t& position, std::string& decoded, std::string& error) {
		// エスケープとUnicodeを解釈し、JSONの文字列を読み取る。
		decoded.clear();

		++position;

		while (position < text.size()) {
			char character = text[position++];

			if (character == '"') {
				return true;
			}

			if (static_cast<unsigned char>(character) < 32) {
				break;
			}

			if (character == '\\') {
				if (position >= text.size()) {
					break;
				}

				character = text[position++];

				if (character == 'u') {
					uint32_t codepoint = 0;

					if (!ReadUnicode(text, position, codepoint)) {
						break;
					}

					// UTF-16の上位サロゲートに続く下位サロゲートを読み、1文字のコードポイントに戻す。
					if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
						if (text.substr(position, 2) != "\\u") {
							break;
						}

						position += 2;

						uint32_t low = 0;

						if (!ReadUnicode(text, position, low)) {
							break;
						}

						if (low < 0xdc00 || low > 0xdfff) {
							break;
						}

						codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
					} else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
						break;
					}

					// ファイル名として扱えるよう、復元した文字をUTF-8で追加する。
					AppendUnicode(decoded, codepoint);

					continue;
				}

				if (character == 'b') {
					character = '\b';
				} else if (character == 'f') {
					character = '\f';
				} else if (character == 'n') {
					character = '\n';
				} else if (character == 'r') {
					character = '\r';
				} else if (character == 't') {
					character = '\t';
				} else if (character != '"' && character != '\\' && character != '/') {
					break;
				}
			}

			decoded.push_back(character);
		}

		error = "Invalid or unterminated JSON string.";

		return false;
	}

}

//=============================================================================================//
// 読込・更新ファイルの準備

bool AssetDocument::Load(const std::filesystem::path& root, const std::filesystem::path& relative, std::string& error) {
	root_ = root;
	path_ = relative;

	extension_ = AssetPathChange::Text(relative.extension());

	for (char& character : extension_) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	std::ifstream input(root / relative, std::ios::binary | std::ios::ate);

	if (!input) {
		error = "Cannot read " + AssetPathChange::Text(relative);

		return false;
	}

	if (input.tellg() > 64 * 1024 * 1024) {
		error = "Reference document exceeds 64 MiB: " + AssetPathChange::Text(relative);

		return false;
	}

	input.seekg(0);

	original_.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

	if (input.bad()) {
		error = "Cannot read complete asset document.";

		return false;
	}

	if (extension_ == ".ponscene" || extension_ == ".ponprefab") {
		// 形式1にはAssetパスがなく、既存オブジェクトへの値上書きだけが保存される。
		std::istringstream header(original_);

		std::string identifier;

		unsigned version = 0;

		header >> identifier >> version;

		if (identifier == "PON_SCENE") {
			if (version == 1) {
				return true;
			}
		}

		WorldObjects world;

		if (!SceneFile::Load(world, root / relative, error, extension_ == ".ponscene")) {
			return false;
		}

		objects_ = world.CaptureObjects();

		WorldAssetReferences::Collect(objects_, AssetPathChange::Text(relative), references_);

		return true;
	}

	if (extension_ == ".ponparticle") {
		if (!ParticlePresetFile::Load(particle_, root / relative, error)) {
			return false;
		}

		references_.push_back({AssetPathChange::Text(AssetPathChange::Path(particle_.texture).lexically_relative("resources")), AssetPathChange::Text(relative) + " / Particle"});

		return true;
	}

	text_ = original_;

	if (extension_ == ".glb") {
		if (original_.size() < 20) {
			error = "Invalid GLB header.";

			return false;
		}

		if (ReadUint32(original_, 0) != 0x46546c67 || ReadUint32(original_, 4) != 2 || ReadUint32(original_, 8) != original_.size()) {
			error = "Unsupported GLB header.";

			return false;
		}

		const uint32_t jsonLength = ReadUint32(original_, 12);

		if (jsonLength > original_.size() - 20 || ReadUint32(original_, 16) != 0x4e4f534a) {
			error = "Invalid GLB JSON chunk.";

			return false;
		}

		text_ = original_.substr(20, jsonLength);

		binarySuffix_ = original_.substr(20 + jsonLength);
	}

	if (extension_ == ".gltf" || extension_ == ".glb") {
		return ReadJsonLinks(error);
	}

	if (extension_ == ".obj" || extension_ == ".mtl") {
		return ReadWavefrontLinks(error);
	}

	return true;
}

bool AssetDocument::WriteUpdated(const std::filesystem::path& stagingPath, const AssetPathChange& change, std::string& error) const {
	// 元の内容を保ちながら、移動対象の参照だけを書き換えて出力する。
	if (extension_ == ".ponscene" || extension_ == ".ponprefab") {
		std::vector<WorldObjectData> updated = objects_;

		WorldAssetReferences::Remap(updated, change);

		WorldObjects world;

		if (!world.ReplaceObjects(std::move(updated), error, false)) {
			return false;
		}

		return SceneFile::Save(world, stagingPath, error, false);
	}

	if (extension_ == ".ponparticle") {
		ParticleSettings updated = particle_;

		updated.texture = change.RemapResource(updated.texture);

		return ParticlePresetFile::Save(updated, stagingPath, error);
	}

	std::string updated = text_;

	// 後ろから置き換えれば、まだ処理していない参照の開始位置がずれない。
	for (std::vector<Link>::const_reverse_iterator link = links_.rbegin(); link != links_.rend(); ++link) {
		updated.replace(link->begin, link->length, GetUpdatedLink(*link, change));
	}

	if (extension_ == ".glb") {
		// GLBのJSONチャンクの長さは4バイト境界にそろえる。
		while (updated.size() % 4 != 0) {
			updated.push_back(' ');
		}

		std::string container;

		// GLBヘッダーとJSONチャンクの長さを再計算し、元のバイナリ部分を末尾につなぐ。
		WriteUint32(container, 0x46546c67);
		WriteUint32(container, 2);
		WriteUint32(container, static_cast<uint32_t>(20 + updated.size() + binarySuffix_.size()));
		WriteUint32(container, static_cast<uint32_t>(updated.size()));
		WriteUint32(container, 0x4e4f534a);

		updated = container + updated + binarySuffix_;
	}

	// 元ファイルへ直接書かず、指定された準備先へ更新結果を書く。
	std::ofstream output(stagingPath, std::ios::binary | std::ios::trunc);

	output.write(updated.data(), static_cast<std::streamsize>(updated.size()));

	output.close();

	if (!output) {
		error = "Cannot stage updated asset document.";

		return false;
	}

	return true;
}

//=============================================================================================//
// Getter / 原本・参照

const std::filesystem::path& AssetDocument::GetPath() const {
	// 編集中のファイルのパスを取得する。
	return path_;
}

const std::string& AssetDocument::GetOriginal() const {
	// 更新前のファイル内容を取得する。
	return original_;
}

const std::vector<AssetReference>& AssetDocument::GetReferences() const {
	// ファイルから収集したAsset参照を取得する。
	return references_;
}

bool AssetDocument::IsOpaque() const {
	// FBXの内部参照を安全に解析できないことを呼出側へ伝える。
	if (extension_ == ".fbx") {
		return true;
	}

	return false;
}

bool AssetDocument::HasChanges(const AssetPathChange& change) const {
	// パスを置き換えた場合に、参照先が変わる項目があるか調べる。
	for (const AssetReference& reference : references_) {
		if (change.RemapRelative(reference.path) != reference.path) {
			return true;
		}
	}

	for (const Link& link : links_) {
		if (GetUpdatedLink(link, change) != text_.substr(link.begin, link.length)) {
			return true;
		}
	}

	return false;
}

//=============================================================================================//
// モデル内の参照

bool AssetDocument::ReadJsonLinks(std::string& error) {
	// JSON内のパス項目を探し、書換え可能な参照として登録する。
	size_t position = 0;

	while (position < text_.size()) {
		if (text_[position] != '"') {
			++position;

			continue;
		}

		std::string key;

		if (!ReadJsonString(text_, position, key, error)) {
			return false;
		}

		if (key != "uri") {
			continue;
		}

		size_t valueStart = text_.find_first_not_of(" \t\r\n", position);

		if (valueStart == std::string::npos || text_[valueStart] != ':') {
			continue;
		}

		valueStart = text_.find_first_not_of(" \t\r\n", valueStart + 1);

		if (valueStart == std::string::npos || text_[valueStart] != '"') {
			error = "A glTF URI is not a string.";

			return false;
		}

		std::string value;

		position = valueStart;

		if (!ReadJsonString(text_, position, value, error)) {
			return false;
		}

		if (!AddLink(valueStart, position - valueStart, value, true, error)) {
			return false;
		}
	}

	return true;
}

bool AssetDocument::ReadWavefrontLinks(std::string& error) {
	size_t lineStart = 0;

	while (lineStart < text_.size()) {
		size_t lineEnd = text_.find('\n', lineStart);

		if (lineEnd == std::string::npos) {
			lineEnd = text_.size();
		}

		std::istringstream input(text_.substr(lineStart, lineEnd - lineStart));

		std::string command;

		input >> command;

		bool hasDependency = command == "mtllib";

		if (extension_ == ".mtl") {
			hasDependency = command.starts_with("map_") || command == "bump" || command == "disp" || command == "decal" || command == "norm";
		}

		if (hasDependency) {
			input >> std::ws;

			if (extension_ == ".mtl") {
				if (!SkipTextureOptions(input, error)) {
					return false;
				}
			}

			const std::streampos offset = input.tellg();

			if (offset >= 0) {
				std::string value;

				std::getline(input, value);

				while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
					value.pop_back();
				}

				if (!value.empty()) {
					std::error_code pathError;

					const bool doesWholeFilenameExist = std::filesystem::exists(root_ / path_.parent_path() / AssetPathChange::Path(value), pathError);

					if (command == "mtllib" && !doesWholeFilenameExist) {
						// 複数のMTLは各トークンの位置を保持する。引用された空白入りの名前も一つとして扱う。
						std::istringstream entries(value);

						while (entries >> std::ws && entries.peek() != std::char_traits<char>::eof()) {
							if (entries.peek() == '#') {
								break;
							}

							const std::streampos entryStart = entries.tellg();

							std::string filename;

							if (!(entries >> std::quoted(filename))) {
								error = "Invalid quoted Wavefront filename.";

								return false;
							}

							size_t entryEnd = value.size();

							if (!entries.eof()) {
								entryEnd = static_cast<size_t>(entries.tellg());
							}

							if (!AddLink(lineStart + static_cast<size_t>(offset) + static_cast<size_t>(entryStart),
								entryEnd - static_cast<size_t>(entryStart), filename, false, error)) {
								return false;
							}
						}
					} else {
						std::string decoded = value;

						if (value.front() == '"') {
							std::istringstream quoted(value);

							if (!(quoted >> std::quoted(decoded))) {
								error = "Invalid quoted Wavefront filename.";

								return false;
							}

							if (!quoted.eof()) {
								value.resize(static_cast<size_t>(quoted.tellg()));
							}
						} else if (!doesWholeFilenameExist) {
							const size_t comment = (std::min)(value.find(" #"), value.find("\t#"));

							if (comment != std::string::npos) {
								value.erase(comment);

								while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
									value.pop_back();
								}

								decoded = value;
							}
						}

						if (!AddLink(lineStart + static_cast<size_t>(offset), value.size(), decoded, false, error)) {
							return false;
						}
					}
				}
			}
		}

		lineStart = lineEnd + 1;
	}

	return true;
}

bool AssetDocument::AddLink(size_t begin, size_t length, const std::string& value, bool isUri, std::string& error) {
	std::string scheme = value.substr(0, 5);

	for (char& character : scheme) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	if (scheme == "file:") {
		error = "Absolute file URIs must be exported as resources-relative paths before asset editing.";

		return false;
	}

	if (scheme == "data:" || value.find("://") != std::string::npos) {
		return true;
	}

	std::string decoded = value;

	if (isUri) {
		decoded.clear();

		for (size_t position = 0; position < value.size(); ++position) {
			if (value[position] == '%') {
				if (position + 2 >= value.size()) {
					error = "Invalid percent-encoded glTF URI.";

					return false;
				}

				const int high = HexValue(value[position + 1]);
				const int low = HexValue(value[position + 2]);

				if (high < 0 || low < 0) {
					error = "Invalid percent-encoded glTF URI.";

					return false;
				}

				decoded.push_back(static_cast<char>(high * 16 + low));

				position += 2;
			} else {
				decoded.push_back(value[position]);
			}
		}
	}

	std::replace(decoded.begin(), decoded.end(), '\\', '/');

	const std::filesystem::path dependency = (path_.parent_path() / AssetPathChange::Path(decoded)).lexically_normal();

	if (dependency.is_absolute() || dependency.has_root_name() || dependency.empty() || decoded.find('\0') != std::string::npos) {
		error = "Model dependency is not inside resources.";

		return false;
	}

	if (*dependency.begin() == "..") {
		error = "Model dependency escapes resources.";

		return false;
	}

	links_.push_back({begin, length, dependency, isUri});

	references_.push_back({AssetPathChange::Text(dependency), AssetPathChange::Text(path_) + " / imported dependency"});

	return true;
}

std::string AssetDocument::GetUpdatedLink(const Link& link, const AssetPathChange& change) const {
	const std::filesystem::path owner = AssetPathChange::Path(change.RemapRelative(AssetPathChange::Text(path_)));
	const std::filesystem::path target = AssetPathChange::Path(change.RemapRelative(AssetPathChange::Text(link.target)));

	if (owner == path_ && target == link.target) {
		return text_.substr(link.begin, link.length);
	}

	std::filesystem::path parent = owner.parent_path();

	if (parent.empty()) {
		parent = ".";
	}

	const std::string relative = AssetPathChange::Text(target.lexically_relative(parent));

	if (link.isUri) {
		return "\"" + EncodeUri(relative) + "\"";
	}

	// AssimpのWavefront読込は行末までをファイル名として読む。引用符は名前の一部になるため付けない。
	return relative;
}
