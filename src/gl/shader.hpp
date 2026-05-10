#pragma once

#include "../pch.hpp"
#include <optional>

class shader {
public:
	shader();
	~shader();

	void compile(const std::string &vert_src, const std::string &frag_src, const std::optional<std::filesystem::path> &include_dir = std::nullopt, const std::string& shader_name_for_errors = "");
	void compile_from_files(
	    const std::filesystem::path &vert_path,
	    const std::filesystem::path &frag_path
	);
	void bind();

private:
	GLuint prog_id;
};
