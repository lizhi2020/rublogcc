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
// �������� �ݲ�֧��Ƕ��
// ��� () ��Ϊ�� ��href= []�ڵ�����
void line2html(const string& md, stringstream& ss) {
	enum class ST{NONE,START_TX,END_TX,START_HREF};
	ST s = ST::NONE;
	// todo: ����stringstream
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

// ��һ��״̬��������md�ļ�
// h��ǩ���᳢���޸�״̬
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

	// ����ֵ��ʾ�Ƿ�ɹ�����һ��
	bool processLine() {
		std::string buf;

		if(!getline(ss,buf))return false;

		// ��������
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
		// ת�� < >
		buf = escape(buf);

		// �����
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

		// ���������
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
		
		// ����div��������
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

// ��ȡָ������������
// ���ڹ���brief
// ���Ե�һ�������У�����
// ������� ��ֱ�ӷ���ԭ��
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

	// �������л�
	void Unserialize(const std::string buf) {
		stringstream ss;
		ss << buf;
		Unserialize(ss);
	}
	// �������л�
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

// ��ȾĿ¼��index.html
void renderDir(const path& indir, const path& outdir) {
	// ����Ŀ¼ content/***
	// ���Ŀ¼ public/***

	// ��ȡ�ļ� �����ֵ�
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
			
			// ����ģ���Ƿ�ʹ�õ������Ƕ����ṩ���еļ�ֵ
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
// ����ļ��ǿ��ļ������ɶ�Ӧ���ļ�
void renderFile(const std::filesystem::path& md, const std::filesystem::path& html) {
	// ��md�ļ�����ȡ�ؼ���Ϣ
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

// ���������Ŀ¼��
// ��������Ŀ¼�����
void walk(const std::filesystem::path& indir, const std::filesystem::path& outdir) {
	using namespace std::filesystem;
	
	directory_entry entry(indir);
	if (!entry.is_directory())return;

	// ������Ŀ¼�������򴴽�
	if (!exists(path(outdir))) {
		create_directories(outdir); // ���ܻ��׳��쳣 ����ͬ���ļ�
	}
	
	path config_path = indir;
	config_path.append("gen.config");
	if (exists(config_path)) {
		// ��ȡ�ļ� �������л�
		std::ifstream cfgcontent(config_path.string());

		std::stringstream ss;
		ss << cfgcontent.rdbuf();
		
		// ������һ������ �����ı�
		rub::Config* p = new rub::Config(*cfgstk.top());
		p->Unserialize(ss);

		cfgstk.push(p);
	}
	else{
		cfgstk.push(cfgstk.top());
	}

	renderDir(indir, outdir);
	// ��ǰ�˳�Ҫ��ԭջ
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
		else if(it.path().extension()==".md") { // ������md��׺�ļ�
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
// ����Ŀ¼�����
void cpfiles(path indir, path outdir) {
	if (!exists(outdir))
		create_directories(outdir); // �쳣����

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
	// ��������
	// std::locale::global(std::locale(""));

	// ����contentĿ¼ ��publicĿ¼�������Ӧ�ļ�

	// ����һ��Ĭ�ϵ�ģ��
	ctemplate::StringToTemplateCache("default", "This is a default tpl", ctemplate::DO_NOT_STRIP);

	cfgstk.push(new rub::Config());

	// ������̬�ļ�
	cpstatic();

	render();
}

