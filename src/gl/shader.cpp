#include "shader.hpp"

static std::string load_text(const std::filesystem::path &path) {
	std::ifstream f(path, std::ios::in | std::ios::binary);
	const auto sz = std::filesystem::file_size(path);

	std::string str(sz, '\0');
	f.read(str.data(), sz);

	return str;
}

static std::string process_includes(const std::string& source, const std::filesystem::path& include_dir) {
	std::istringstream iss(source);
	std::ostringstream oss;
	std::string line;
	
	while (std::getline(iss, line)) {
		// check if line is commented out
		size_t first_non_space = line.find_first_not_of(" \t");
		if (first_non_space != std::string::npos && 
			first_non_space + 1 < line.length() && 
			line.substr(first_non_space, 2) == "//") {
			oss << line << "\n";
			continue;
		}
		
		size_t pos = line.find("#include");
		if (pos != std::string::npos) {
			// make sure it's actually #include and not #version or similar
			if (pos + 8 < line.length() && 
				(line[pos + 8] == ' ' || line[pos + 8] == '\t' || line[pos + 8] == '"')) {
				
				size_t start_quote = line.find('"', pos);
				size_t end_quote = line.find('"', start_quote + 1);
				if (start_quote != std::string::npos && end_quote != std::string::npos) {
					std::string filename = line.substr(start_quote + 1, end_quote - start_quote - 1);
					std::filesystem::path inc_path = include_dir / filename;
					if (!std::filesystem::exists(inc_path)) {
						throw std::runtime_error("Include file not found: " + inc_path.string());
					}
					std::string inc_content = load_text(inc_path);
					oss << process_includes(inc_content, include_dir) << "\n";
					continue;
				}
			}
		}
		oss << line << "\n";
	}
	return oss.str();
}

// --- impl ---

shader::shader() {
	prog_id = glCreateProgram();
}
shader::~shader() {
	glDeleteProgram(prog_id);
}

static void
check_shader_compile(GLuint shader_id, const std::string &shader_name) {
	GLint success;
	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar info[512];
		glGetShaderInfoLog(shader_id, 512, NULL, info);
		throw std::runtime_error(
		    shader_name + " compilation failed:\n" + std::string(info) + "\n"
		);
	}
}

static void check_program_link(GLuint prog_id, const std::string &shader_name) {
	GLint success;
	glGetProgramiv(prog_id, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar info[512];
		glGetProgramInfoLog(prog_id, 512, NULL, info);
		throw std::runtime_error(
		    shader_name + " program link failed:\n" + std::string(info) + "\n"
		);
	}
}

void shader::compile(const std::string &vert_src, const std::string &frag_src, const std::optional<std::filesystem::path> &include_dir, const std::string& shader_name_for_errors) {
	std::string final_vert = include_dir ? process_includes(vert_src, *include_dir) : vert_src;
	std::string final_frag = include_dir ? process_includes(frag_src, *include_dir) : frag_src;

	GLuint v_id = glCreateShader(GL_VERTEX_SHADER);
	GLuint f_id = glCreateShader(GL_FRAGMENT_SHADER);

	const char *v_str = final_vert.c_str();
	const char *f_str = final_frag.c_str();
	const GLint v_len = final_vert.size();
	const GLint f_len = final_frag.size();

	glad_glShaderSource(v_id, 1, &v_str, &v_len);
	glShaderSource(f_id, 1, &f_str, &f_len);

	glCompileShader(v_id);
	glCompileShader(f_id);

	std::string v_name = shader_name_for_errors.empty() ? "Vertex shader" : shader_name_for_errors + " (vertex)";
	std::string f_name = shader_name_for_errors.empty() ? "Fragment shader" : shader_name_for_errors + " (fragment)";
	std::string p_name = shader_name_for_errors.empty() ? "Shader" : shader_name_for_errors;

	check_shader_compile(v_id, v_name);
	check_shader_compile(f_id, f_name);

	glAttachShader(prog_id, v_id);
	glAttachShader(prog_id, f_id);
	glLinkProgram(prog_id);

	check_program_link(prog_id, p_name);

	std::cout << "Compiled shader '" << shader_name_for_errors << "' with program ID " << prog_id << std::endl;

	glDeleteShader(f_id);
	glDeleteShader(v_id);
}

void shader::compile_from_files(
    const std::filesystem::path &vert_path,
    const std::filesystem::path &frag_path
) {
	if (vert_path.parent_path() != frag_path.parent_path()) {
		throw std::runtime_error("compile_from_files: Both shaders must be in the same directory for includes to work properly.");
	}

	std::string vert_src = load_text(vert_path);
	std::string frag_src = load_text(frag_path);
	return compile(vert_src, frag_src, vert_path.parent_path(), vert_path.stem().string());
}

void shader::bind() {
	glUseProgram(prog_id);
}
