#include<iostream>
#include<fstream>
#include<vector>
#include<string>
#include<sstream>
#include<Windows.h>
#include<ctemplate/template.h>     
#include<filesystem>
#include<stack>

using namespace std::filesystem;

namespace rub {
using namespace std;
// 用一个状态机来处理md文件
class TM {
	enum class STATE
	{
		OK,DIV
	};
	STATE current=STATE::OK;
	stringstream ss, html;
public:
	TM(const string& content){
		ss << content;
	};

	// 返回值表示是否可以继续处理一行
	bool processLine() {
		std::string buf;
		bool ret = (bool)getline(ss,buf);
		if (buf.find("# ")==0) {
			if (current != STATE::OK) {
				html << "</div>";
				current = STATE::OK;
			}
			html << "<h1>" << buf.substr(2) << "</h1>";
		}
		else if (buf.empty()) {
			if (current != STATE::OK) {
				html << "</div>";
				current = STATE::OK;
			}
		}
		else {
			if (current == STATE::OK) {
				html << "<div>" << buf;
				current = STATE::DIV;
			}
		}
		return ret;
	};
	string process() {
		html.clear();
		while (processLine()) {};
		if (current != STATE::OK)html << "</div>";
		return html.str();
	}
};
string md2html(const string& content) {
	TM tm = TM(content);
	return tm.process();
}

class Config {
public:
	std::string templateFileName = "default";
	std::string dirtemplateName = "default";

	bool index_only = false;
	Config() {};
	Config(const std::string& nm) { templateFileName = nm; }

	// 增量序列化
	void Unserialize(const std::string buf) {
		stringstream ss;
		ss << buf;
		Unserialize(ss);
	}
	// 增量序列化
	void Unserialize(stringstream& ss) {
		std::string tmp;
		while (getline(ss, tmp)) {
			if (tmp.find("index_only") == 0) {
				index_only = true;
			}
			else if (tmp.find("dir ") == 0) {
				dirtemplateName = tmp.substr(4);
			}
			else if (tmp.find("file ") == 0) {
				templateFileName = tmp.substr(5);
			}
		}
	}
};
} // rub

std::stack<rub::Config*> cfgstk;

void genIndex(LPCWSTR dir, const std::vector<std::wstring>& items) {
	// 创建index.html文件
	TCHAR dst[100]=L"public";
	lstrcat(dst, dir+sizeof(TEXT("content"))/sizeof(TCHAR)-1);
	lstrcat(dst, TEXT("\\index.html"));

	// 写入文件
	HANDLE file = CreateFile(dst, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return;

	std::stringstream stream;
	stream << "<html><head><meta charset='utf-8'></head><body>";
	for (auto& i : items) {
		char buffer[100];
		memset(buffer, 0, 100);
		WideCharToMultiByte(CP_UTF8, 0, i.c_str(), i.size(), buffer, 100, NULL, NULL);
		std::string href = std::string(buffer);
		stream << "<a href=\"" << href << "\">" << href << "</a><br/>";// 使用br换行
	}
	stream << "</body></html>";
	auto r = stream.str();
	WriteFile(file, r.c_str(), r.size(), nullptr, nullptr);
	CloseHandle(file);
}

// 递归调用
void walk(LPCWSTR path) {
	WIN32_FIND_DATA fdata;
	HANDLE hFind;
	TCHAR buf[100];

	std::vector<std::wstring> items;

	lstrcpy(buf, path);
	lstrcat(buf, TEXT("\\*"));

	hFind = FindFirstFile(buf, &fdata);
	if (hFind == INVALID_HANDLE_VALUE) {
		std::cerr << __FUNCSIG__ << " " << __LINE__ << " " << path << " not find\n";
		return;
	}
	do {
		if (lstrcmp(fdata.cFileName,TEXT("."))==0||
			lstrcmp(fdata.cFileName,TEXT(".."))==0) {
			continue;
		}
		
		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			lstrcpy(buf, path);
			lstrcat(buf, TEXT("\\"));
			lstrcat(buf, fdata.cFileName);
			TCHAR dst[100]=TEXT("public");
			lstrcat(dst, buf + sizeof(TEXT("content")) / sizeof(TCHAR) - 1);
			CreateDirectory(dst, NULL);
			walk(buf);
			items.push_back(fdata.cFileName);
			continue;
		}
		// 检查是否是md文件
		int length = lstrlen(fdata.cFileName);
		if (length < 3) {
			continue;
		}
		if (lstrcmp(TEXT(".md"),fdata.cFileName+length-3)!=0) {
			continue;
		}
		items.push_back(fdata.cFileName);
		TCHAR src[100];
		TCHAR dst[100]=TEXT("public");
		lstrcpy(src, path);
		lstrcat(src, TEXT("\\"));
		lstrcat(src, fdata.cFileName);
		lstrcat(dst, src + sizeof(TEXT("content")) / sizeof(TCHAR)-1);

		//renderFile(src, dst, false);
	} while (FindNextFile(hFind, &fdata));

	genIndex(path, items);
}
void renderDir(const path& indir, const path& outdir) {
	// 读取文件 构造字典
	ctemplate::TemplateDictionary dict("dir");

	directory_iterator its(indir);
	for (auto& it : its) {
		if (it.path().extension() == ".md") {
			auto p = dict.AddSectionDictionary("post");
			
			std::ifstream file(it.path());
			file.seekg(0, file.end);
			int len = file.tellg();
			file.seekg(0, file.beg);

			char* tmp = new char[len+1];
			for (int i = 0; i < len+1; i++)tmp[i] = 0;

			file.read(tmp, len);
			(*p)["content"] = rub::md2html(tmp);
		}
	}

	auto p = cfgstk.top()->dirtemplateName;
	std::string buf;
	ctemplate::ExpandTemplate("template\\"+p, ctemplate::DO_NOT_STRIP, &dict, &buf);

	//
	auto pp = outdir;
	pp.append("index.html");
	HANDLE hf = CreateFile(pp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf == INVALID_HANDLE_VALUE) {
		return;
	}
	WriteFile(hf, buf.c_str(), buf.length(), nullptr, nullptr);
	CloseHandle(hf);
}
void renderFile(const std::filesystem::path& md, const std::filesystem::path& html) {
	// 从md文件中提取关键信息
	ctemplate::TemplateDictionary dict(md.string());
	
	std::ifstream file(md);
	file.seekg(0, file.end);
	int len = file.tellg();
	file.seekg(0, file.beg);

	char* tmp = new char[len];
	for (int i = 0; i < len; i++)tmp[i] = 0;

	file.read(tmp,len);
	dict["content"] = rub::md2html(tmp);
	std::string buf;

	auto tmpp = std::filesystem::path("template").append(cfgstk.top()->templateFileName);
	ctemplate::ExpandTemplate(tmpp.string(), ctemplate::DO_NOT_STRIP, &dict, &buf);

	//
	auto p = html.wstring();
	HANDLE hf = CreateFile(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf == INVALID_HANDLE_VALUE) {
		std::cerr << html.string() << " invaild template\n";
		return;
	}
	WriteFile(hf, buf.c_str(), buf.length(), nullptr, nullptr);
	CloseHandle(hf);
}

// 输入必须是目录名
// 不对输入目录做检查
void walk(const std::filesystem::path& indir, const std::filesystem::path& outdir) {
	using namespace std::filesystem;
	
	directory_entry entry(indir);
	if (!entry.is_directory())return;

	// 如果输出目录不存在则创建
	if (!exists(path(outdir))) {
		create_directories(outdir); // 可能会抛出异常 存在同名文件
	}
	
	path config_path = indir;
	config_path.append("gen.config");
	if (exists(config_path)) {
		// 读取文件 并反序列化
		std::ifstream cfgcontent(config_path.string());

		std::stringstream ss;
		ss << cfgcontent.rdbuf();
		
		// 复制上一个配置 增量改变
		rub::Config* p = new rub::Config(*cfgstk.top());
		p->Unserialize(ss);

		cfgstk.push(p);
	}
	else{
		cfgstk.push(cfgstk.top());
	}

	renderDir(indir, outdir);
	// 提前退出要还原栈
	if (cfgstk.top()->index_only) {
		cfgstk.pop();
		return;
	}

	directory_iterator its(indir);
	for (const auto& it : its) {
		std::string src = it.path().string();
		auto p = path(outdir);
		p.append(it.path().filename().string());
		if (it.is_directory())
			walk(it.path(), p);
		else if(it.path().extension()==".md") { // 仅处理md后缀文件
			p.replace_extension("html");
			renderFile(it.path(), p);
		}
	}

	cfgstk.pop();
}

void render() {
	using namespace std::filesystem;

	path src = "content";
	path build = "public";
	if (!exists(src)) {
		return;
	}
	walk(src, build);
}
// 输入目录不检查
void cpfiles(path indir, path outdir) {
	if (!exists(outdir))
		create_directories(outdir); // 异常处理

	directory_iterator its(indir);
	for (auto& it : its) {
		auto p = outdir;
		p.append(it.path().filename().string());
		if (it.is_directory()) {
			cpfiles(it.path(), p);
		}
		else {
			CopyFileW(it.path().wstring().c_str(), p.wstring().c_str(), FALSE);
		}
	}
}
void cpstatic() {
	
	path src = "static";
	path build = "public";
	if (!exists(src)||!directory_entry(src).is_directory()) {
		return;
	}

	cpfiles(src, build);
}
int main() {
	// 解析参数
	// std::locale::global(std::locale(""));

	// 遍历content目录 在public目录下输出对应文件
	// walk(TEXT("content"));
	// walk(std::string("content"),"public");

	// 加载一个默认的模板
	ctemplate::StringToTemplateCache("default", "This is a default tpl", ctemplate::DO_NOT_STRIP);

	cfgstk.push(new rub::Config());

	// 拷贝静态文件
	cpstatic();

	render();
}

