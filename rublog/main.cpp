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

// �������� �ݲ�֧��Ƕ��
void line2html(const string& md, stringstream& ss) {
	enum class ST{NONE,START_TX,END_TX,START_HREF};
	ST s = ST::NONE;
	// todo: ����stringstream
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

// ��һ��״̬��������md�ļ�
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

	// ����ֵ��ʾ�Ƿ���Լ�������һ��
	bool processLine() {
		std::string buf;

		bool ret = (bool)getline(ss,buf);

		// ��������Ԫ��
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

void renderDir(const path& indir, const path& outdir) {
	// ��ȡ�ļ� �����ֵ�
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
	// ��md�ļ�����ȡ�ؼ���Ϣ
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

