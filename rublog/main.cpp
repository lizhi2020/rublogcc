#include<iostream>
#include<fstream>
#include<vector>
#include<string>
#include<sstream>
#include<Windows.h>
#include<filesystem>
#include<stack>
#include<ctemplate/template.h>

using namespace std::filesystem;

namespace rub {
using namespace std;

// 处理链接 暂不支持嵌套
void line2html(const string& md, stringstream& ss) {
	enum class ST{NONE,START_TX,END_TX,START_HREF};
	ST s = ST::NONE;
	// todo: 换掉stringstream
	stringstream tx;
	stringstream hf;
	for (const auto& it : md) {
		switch (s)
		{
		case ST::NONE:
			if (it == '[') {
				s = ST::START_TX;
			}
			else {
				ss << it;
			}
			break;
		case ST::START_TX:
			if (it == ']') {
				s = ST::END_TX;
			}
			else {
				tx << it;
			}
			break;
		case ST::END_TX:
			if (it == '(') {
				s = ST::START_HREF;
			}
			else {
				s = ST::NONE;
				tx.str("");
			}
			break;
		case ST::START_HREF:
			if (it == ')') {
				s = ST::NONE;
				ss << "<a href=\"" << hf.str() << "\">" << tx.str() << "</a>";
				hf.str("");
				tx.str("");
			}
			else {
				hf << it;
			}
			break;
		default:
			break;
		}
	}
}

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

		// 处理行内元素
		std:stringstream st;
		line2html(buf, st);

		buf = st.str();

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
				html << "<div>" ;
				current = STATE::DIV;
			}
			html << buf;
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

void renderDir(const path& indir, const path& outdir) {
	// 读取文件 构造字典
	ctemplate::TemplateDictionary dict("dir");

	directory_iterator its(indir);
	for (auto& it : its) {
		if (it.path().extension() == ".md") {
			auto p = dict.AddSectionDictionary("post");
			
			std::ifstream file(it.path());
			std::stringstream ss;
			ss << file.rdbuf();
			
			(*p)["content"] = rub::md2html(ss.str());
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

	std::stringstream ss;
	ss << file.rdbuf();

	dict["content"] = rub::md2html(ss.str());
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

	// 加载一个默认的模板
	ctemplate::StringToTemplateCache("default", "This is a default tpl", ctemplate::DO_NOT_STRIP);

	cfgstk.push(new rub::Config());

	// 拷贝静态文件
	cpstatic();

	render();
}

