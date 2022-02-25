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
string escape(string& buf) {
	string ret;
	for (auto& it : buf) {
		if (it == '<') {
			ret += "&lt;";
		}
		else if (it == '>') {
			ret += "&gt;";
		}
		else {
			ret += it;
		}
	}
	return ret;
}
// 处理链接 暂不支持嵌套
// 如果 () 内为空 则href= []内的内容
void line2html(const string& md, stringstream& ss) {
	enum class ST{NONE,START_TX,END_TX,START_HREF};
	ST s = ST::NONE;
	// todo: 换掉stringstream
	bool img = false;
	string tx,hf;
	tx.reserve(64);
	hf.reserve(64);
	
	for (int i = 0; i < md.length();i++) {
		const char& it = md[i];
		switch (s)
		{
		case ST::NONE:
			if (it == '[') {
				s = ST::START_TX;
				
			}
			else if (it == '!'&&md[i+1]=='['&&md[i+2]==']') {
				img = true;
				i += 2;
				s = ST::END_TX;
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
				tx+=it;
			}
			break;
		case ST::END_TX:
			if (it == '(') {
				s = ST::START_HREF;
			}
			else {
				s = ST::NONE;
				tx.clear();
			}
			break;
		case ST::START_HREF:
			if (it == ')') {
				s = ST::NONE;
				if (img) {
					ss << "<img ";
				}
				else {
					ss << "<a ";
				}
				ss << "href=\"";
				if (hf.empty()) {
					ss << tx;
				}
				else {
					ss << hf;
				}
				
				if (img) {
					ss << "\"/>";
				}
				else {
					ss << "\">" << tx << "</a>";
				}

				hf.clear();
				tx.clear();
				img = false;
			}
			else {
				hf += it;
			}
			break;
		default:
			break;
		}
	}
}

// 用一个状态机来处理md文件
// h标签不会尝试修改状态
class TM {
	enum class STATE
	{
		OK,DIV,CODE
	};
	STATE current=STATE::OK;
	stringstream ss;

	string html;
public:
	TM(const string& content){
		html.reserve(4096);
		ss << content;
	};

	// 返回值表示是否成功处理一行
	bool processLine() {
		std::string buf;

		if(!getline(ss,buf))return false;

		// 读到空行
		if (buf.empty()) {
			switch (current)
			{
			case STATE::OK:break;
			case STATE::DIV:
				html.append("</div>");
				current = STATE::OK;
				break;
			case STATE::CODE:
				html.append("<li></li>");
				break;
			default:
				assert(1);
			}
			return true;
		}
		// 转义 < >
		buf = escape(buf);

		// 代码块
		if (buf == "```") {
			switch (current)
			{
			case STATE::DIV:
				html.append("</div>");
			case STATE::OK:
				current = STATE::CODE;
				html.append("<code><ul>");
				break;
			case STATE::CODE:
				current = STATE::OK;
				html.append("</ul></code>");
				break;
			default:
				assert(1);
			}
			return true;
		}
		if (current == STATE::CODE) {
			html.append("<li><div class=\"code-line\">")
				.append(buf)
				.append("</div></li>");
			return true;
		}

		// 处理标题行
		if (buf[0] == '#') {
			int i = 1;
			for (; i < 5 && buf[i] == '#'; i++) {};
			if (buf[i] == ' ') {
				html.append("<h")
					.append(to_string(i))
					.append(">")
					.append(buf.substr(i + 1))
					.append("</h")
					.append(to_string(i))
					.append(">");
				return true;
			}
		}
		
		// 处理div行内链接
		std:stringstream st;
		line2html(buf, st);

		buf = st.str();
		{
			if (current == STATE::OK) {
				html.append("<div>");
				current = STATE::DIV;
			}
			html.append(buf).append("<br/>");
		}
		return true;
	};
	string process() {
		html.clear();
		while (processLine()) {};
		if (current != STATE::OK)
		switch (current)
		{
		case STATE::DIV:
			html.append("</div>");
			break;
		case STATE::CODE:
			html.append("</ul></code>");
			break;
		default:
			break;
		}
		return html;
	}
};
string md2html(const string& content) {
	TM tm = TM(content);
	return tm.process();
}

// 提取指定行数的内容
// 用于构造brief
// 忽略第一个标题行，空行
// 如果不够 则直接返会原文
string getContent(const string& md, int cnt) {
	stringstream ss(md);

	string buf,res;
	res.reserve(1024);

	bool firstLine = true;
	while (getline(ss, buf)&&cnt!=0) {
		if (buf.empty()) {
			continue;
		}
		if (firstLine) {
			firstLine = false;
			if (buf[0] != '#') {
				res.append(buf).append("\n");
				cnt--;
			}
		}
		else {
			res.append(buf.append("\n"));
			cnt--;
		}
		
	}
	if (cnt != 0) {
		return md;
	}
	return res;
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

// 渲染目录的index.html
void renderDir(const path& indir, const path& outdir) {
	// 输入目录 content/***
	// 输出目录 public/***

	// 读取文件 构造字典
	ctemplate::TemplateDictionary dict("dir");

	/*
	auto get_last_write_time = [](std::filesystem::file_time_type const& ftime) {
		std::time_t cftime = std::chrono::system_clock::to_time_t(
			std::chrono::file_clock::to_sys(ftime));
		return std::asctime(std::localtime(&cftime));
	};
	*/

	directory_iterator its(indir);
	for (auto& it : its) {
		if (it.path().extension() == ".md") {
			auto p = dict.AddSectionDictionary("post");
			
			std::ifstream file(it.path());
			std::stringstream ss;
			ss << file.rdbuf();
			
			// 不管模板是否使用到，我们都得提供所有的键值
			(*p)["content"] = rub::md2html(ss.str());
			(*p)["brief"] = rub::md2html(rub::getContent(ss.str(), 5));
			auto title = it.path().stem();
			//auto url = relative(it.path(), "content");
			(*p)["title"] = title.u8string();
			(*p)["url"] = title.replace_extension(".html").u8string();
			(*p)["last_time"] = "2021-11-07";
			(*p)["first_time"] = "2021-11-06";
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
// 如果文件是空文件则不生成对应的文件
void renderFile(const std::filesystem::path& md, const std::filesystem::path& html) {
	// 从md文件中提取关键信息
	ctemplate::TemplateDictionary dict(md.string());
	
	std::ifstream file(md);

	if (file_size(md) == 0) {
		std::cout << "file:" << md.string() << "is empty. skipped.\n";
	}

	std::stringstream ss;
	ss << file.rdbuf();


	dict["title"] = md.stem().u8string();
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

